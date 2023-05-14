#include "Linear_arithmetic.h"

namespace perun {

void Linear_arithmetic::on_variable_resize(Variable::Type type, int num_vars)
{
    if (type == Variable::rational)
    {
        bounds.resize(num_vars);
        watched.resize(num_vars);
        cached_values.resize(num_vars);
        occur.resize(num_vars);
    }
    else if (type == Variable::boolean)
    {
        constraints.resize(num_vars);
    }
}

std::vector<Clause> Linear_arithmetic::propagate(Database&, Trail& trail)
{
    auto models = relevant_models(trail);

    // find relevant variables which have been assigned at current decision level
    std::vector<Variable> variables;
    for (auto [var, _] : assigned(trail))
    {
        if (var.type() == Variable::rational ||
            (var.type() == Variable::boolean && !constraints[var.ord()].empty()))
        {
            variables.push_back(var);
        }
    }

    for (auto var : variables)
    {
        if (var.type() == Variable::boolean)
        {
            auto cons = constraints[var.ord()];
            assert(!cons.empty());

            if (is_fully_assigned(models.owned(), cons))
            {
                assert(eval(models.owned(), cons) == eval(models.boolean(), cons.lit()));
            }
            else if (is_unit(models.owned(), cons))
            {
                unit(models, cons);
            }
        }
        else if (var.type() == Variable::rational)
        {
            replace_watch(trail, models, var.ord());
        }
        else
        {
            assert(false);
        }
    }

    if (options.prop_bounds)
    {
        propagate_bounds(trail, models);
    }

    if (options.prop_unassigned)
    {
        propagate_unassigned(trail, models);
    }
    return finish(trail);
}

void Linear_arithmetic::watch(Constraint& cons)
{
    assert(!cons.empty());

    watched[cons.vars()[0]].push_back(cons);
    if (cons.size() > 1)
    {
        watched[cons.vars()[1]].push_back(cons);
    }
}

void Linear_arithmetic::watch(Constraint& cons, Model<Rational> const& model)
{
    // move 2 unassigned variables to the front
    auto out_var_it = cons.vars().begin();
    auto out_var_end = cons.size() == 1 ? out_var_it + 1 : out_var_it + 2;
    auto out_coef_it = cons.coef().begin();
    auto var_it = out_var_it;
    auto coef_it = out_coef_it;
    for (; var_it != cons.vars().end() && out_var_it != out_var_end; ++var_it, ++coef_it)
    {
        if (!model.is_defined(*var_it))
        {
            std::iter_swap(var_it, out_var_it++);
            std::iter_swap(coef_it, out_coef_it++);
        }
    }

    watch(cons);
}

bool Linear_arithmetic::replace_watch(Model<Rational> const& lra_model, Watched_constraint& watch,
                                      int lra_var_ord)
{
    auto& cons = watch.constraint;

    if (cons.size() <= 1)
    {
        assert(cons.vars().front() == lra_var_ord);
        return false;
    }

    // if both watched variables are assigned, the constraint is fully assigned
    if (lra_model.is_defined(cons.vars()[0]) && lra_model.is_defined(cons.vars()[1]))
    {
        assert(std::all_of(cons.vars().begin(), cons.vars().end(),
                           [&](auto var) { return lra_model.is_defined(var); }));
        return false;
    }

    // move the assigned variable to the second position
    auto rep_var_it = ++cons.vars().begin();
    auto rep_coef_it = ++cons.coef().begin();
    if (*rep_var_it != lra_var_ord)
    {
        std::iter_swap(cons.vars().begin(), rep_var_it);
        std::iter_swap(cons.coef().begin(), rep_coef_it);
    }
    assert(*rep_var_it == lra_var_ord);

    // find an unassigned variable to watch
    if (cons.size() > 2)
    {
        assert(2 <= watch.index && watch.index < cons.size());
        auto var_it = cons.vars().begin() + watch.index;
        auto coef_it = cons.coef().begin() + watch.index;
        auto const end_var_it = var_it;
        do
        {
            assert(coef_it != cons.coef().end());
            if (!lra_model.is_defined(*var_it))
            {
                std::iter_swap(rep_var_it, var_it);
                std::iter_swap(rep_coef_it, coef_it);
                watched[*rep_var_it].push_back(watch);
                break;
            }

            // move to the next variable
            ++watch.index;
            ++coef_it;
            if (++var_it == cons.vars().end())
            {
                var_it = cons.vars().begin() + 2; // skip the watched variables
                coef_it = cons.coef().begin() + 2;
                watch.index = 2;
            }
        } while (var_it != end_var_it);
    }

    return *rep_var_it != lra_var_ord;
}

void Linear_arithmetic::replace_watch(Trail& trail, Models& models, int lra_var_ord)
{
    assert(models.owned().is_defined(lra_var_ord));

    auto& watchlist = watched[lra_var_ord];
    for (std::size_t i = 0; i < watchlist.size();)
    {
        auto& watch = watchlist[i];
        auto& cons = watch.constraint;

        if (replace_watch(models.owned(), watch, lra_var_ord))
        {
            // remove the watch
            std::swap(watchlist[i], watchlist.back());
            watchlist.pop_back();
        }
        else // cons is unit or fully assigned
        {
            if (models.boolean().is_defined(cons.lit().var().ord())) // cons is on the trail
            {
                if (is_fully_assigned(models.owned(), cons))
                {
                    assert(eval(models.owned(), cons) == eval(models.boolean(), cons.lit()));
                }
                else // cons is unit
                {
                    assert(is_unit(models.owned(), cons));
                    unit(models, cons);
                }
            }
            else // cons is *not* on the trail
            {
                if (is_fully_assigned(models.owned(), cons))
                {
                    propagate(trail, models, cons);
                }
                else
                {
                    assert(is_unit(models.owned(), cons));
                }
            }
            ++i;
        }
    }
}

int Linear_arithmetic::decision_level(Trail const& trail, Constraint const& cons) const
{
    int level = trail.decision_level(cons.lit().var()).value_or(0);
    for (auto lra_var_ord : cons.vars())
    {
        Variable var{lra_var_ord, Variable::rational};
        level = std::max<int>(level, trail.decision_level(var).value_or(0));
    }
    return level;
}

bool Linear_arithmetic::is_unit(Model<Rational> const& model, Constraint const& cons) const
{
    // Unit constraint will have exactly one watched variable assigned. The first two variables
    // in each constraint are the watched variables. Moreover, we move the unassigned variable
    // to the front in case one of the watched variables is assigned.
    if (cons.empty() || model.is_defined(cons.vars().front()))
    {
        return false;
    }
    return cons.size() == 1 || model.is_defined(cons.vars()[1]);
}

bool Linear_arithmetic::is_fully_assigned(Model<Rational> const& model,
                                          Constraint const& cons) const
{
    return cons.empty() || model.is_defined(cons.vars().front());
}

std::optional<Clause> Linear_arithmetic::check_bounds(Trail& trail, int var_ord)
{
    if (auto conflict = Bound_conflict_analysis{this}.analyze(trail, bounds, var_ord))
    {
        return conflict;
    }

    if (auto conflict = Inequality_conflict_analysis{this}.analyze(trail, bounds, var_ord))
    {
        return conflict;
    }
    return {}; // no conflict
}

void Linear_arithmetic::unit(Models const& models, Constraint cons) 
{ 
    bounds.update(models, cons); 
}

void Linear_arithmetic::propagate_bounds(Trail const& trail, Models const& models)
{
    if (!trail.empty() &&
        trail.assigned(trail.decision_level()).front().var.type() == Variable::rational)
    {
        auto decided_var = trail.assigned(trail.decision_level()).front().var;
        to_check.push_back(decided_var.ord());
        for (auto cons : occur[decided_var.ord()])
        {
            if (auto val = eval(models.boolean(), cons.lit()))
            {
                bounds.deduce(models, val == true ? cons : ~cons);
            }
        }
    }

    for (;;)
    {
        auto old_size = to_check.size();
        for (auto var_ord : bounds.changed())
        {
            to_check.push_back(var_ord);
            for (auto cons : occur[var_ord])
            {
                if (auto val = eval(models.boolean(), cons.lit()))
                {
                    bounds.deduce(models, val == true ? cons : ~cons);
                }
            }
        }

        // if there was no new propagation
        if (to_check.size() == old_size)
        {
            break;
        }
    }
}

void Linear_arithmetic::propagate_unassigned(Trail& trail, Models& models)
{
    if (trail.decision_level() == 0)
    {
        return;
    }

    auto decided_var = trail.assigned(trail.decision_level()).front().var;
    if (decided_var.type() != Variable::rational)
    {
        return;
    }

    for (auto cons : occur[decided_var.ord()])
    {
        if (!models.boolean().is_defined(cons.lit().var().ord()))
        {
            for (auto c : {cons, ~cons})
            {
                if (bounds.is_implied(models, c))
                {
                    trail.propagate(c.lit().var(), nullptr, trail.decision_level());
                    models.boolean().set_value(c.lit().var().ord(), !c.lit().is_negation());
                }
            }
        }
    }
}

std::vector<Clause> Linear_arithmetic::finish(Trail& trail)
{
    // find all rational variables whose bound has changed
    auto const& changed = bounds.changed();
    to_check.insert(to_check.end(), changed.begin(), changed.end());

    // check for conflict
    std::unordered_set<int> checked;
    std::vector<Clause> result;
    for (auto var_ord : to_check)
    {
        auto [_, is_inserted] = checked.insert(var_ord);
        if (is_inserted)
        {
            if (auto conflict = check_bounds(trail, var_ord))
            {
                result.push_back(std::move(*conflict));
                if (!options.return_all_conflicts)
                {
                    break;
                }
            }
        }
    }
    to_check.clear();
    return result;
}

void Linear_arithmetic::propagate(Trail& trail, Models& models, Constraint const& cons)
{
    assert(!eval(models.boolean(), cons.lit()));

    // find decision level of the propagation
    int dec_level = 0;
    for (auto var_ord : cons.vars())
    {
        auto level = trail.decision_level(Variable{var_ord, Variable::rational});
        assert(level.has_value());
        dec_level = std::max<int>(dec_level, level.value());
    }

    // propagate the boolean variable of the constraint
    auto value = cons.eval(models.owned());
    models.boolean().set_value(cons.lit().var().ord(), cons.lit().is_negation() ^ value);
    trail.propagate(cons.lit().var(), /*reason=*/nullptr, dec_level);
}

bool Linear_arithmetic::is_new(Models const& models, Variable var) const
{
    return (var.type() == Variable::boolean &&
            var.ord() >= static_cast<int>(models.boolean().num_vars())) ||
           (var.type() == Variable::rational &&
            var.ord() >= static_cast<int>(models.owned().num_vars()));
}

void Linear_arithmetic::add_variable(Trail& trail, Models const& models, Variable var)
{
    if (is_new(models, var))
    {
        trail.resize(var.type(), var.ord() + 1);
    }
}

std::optional<Rational> Linear_arithmetic::find_integer(Models const& models,
                                                                           Bounds_type& bounds)
{
    Rational lb{std::numeric_limits<int>::lowest()};
    Rational ub{std::numeric_limits<int>::max()};
    if (auto lower_bound = bounds.lower_bound(models))
    {
        lb = lower_bound->value();
    }

    if (auto upper_bound = bounds.upper_bound(models))
    {
        ub = upper_bound->value();
    }

    assert(lb <= ub);

    Rational abs_min_value = 0;
    Rational abs_bound = 0;
    if (lb <= Rational{0} && ub >= Rational{0})
    {
        abs_bound = std::max(abs(lb), ub);
    }
    else if (lb > Rational{0})
    {
        abs_min_value = lb;
        abs_bound = ub;
    }
    else // lb <= ub < 0
    {
        abs_bound = abs(lb);
        abs_min_value = abs(ub);
    }
    assert(abs_bound >= 0);
    assert(abs_min_value >= 0);

    Rational value{0};
    for (auto int_value = abs_min_value; int_value <= abs_bound; int_value = int_value + 1)
    {
        value = int_value;
        if (lb <= value && value <= ub && bounds.is_allowed(models, value))
        {
            break;
        }

        value = -int_value;
        if (lb <= value && value <= ub && bounds.is_allowed(models, value))
        {
            break;
        }
    }

    return bounds.is_allowed(models, value) ? value : std::optional<Rational>{};
}

void Linear_arithmetic::decide(Database&, Trail& trail, Variable var)
{
    if (var.type() != Variable::rational)
    {
        return;
    }

    auto models = relevant_models(trail);
    auto& bnds = bounds[var.ord()];

    Rational value =
        cached_values.is_defined(var.ord()) ? cached_values.value(var.ord()) : Rational{0};
    if (!bnds.is_allowed(models, value))
    {
        if (auto int_value = find_integer(models, bnds))
        {
            value = int_value.value();
        }
        else // there is no suitable integer value
        {
            assert(bnds.lower_bound(models) != nullptr);
            assert(bnds.upper_bound(models) != nullptr);

            auto lb = bnds.lower_bound(models)->value();
            auto ub = bnds.upper_bound(models)->value();

            value = ub;
            while (!bnds.is_allowed(models, value))
            {
                value = lb / Rational{2} + value / Rational{2};
            }
        }
    }

    // decide the value
    cached_values.set_value(var.ord(), value);
    assert(bnds.is_allowed(models, value));
    models.owned().set_value(var.ord(), value);
    trail.decide(var);
}

void Linear_arithmetic::check_bounds_consistency([[maybe_unused]] Trail const& trail,
                                                 Models const& models)
{
    std::vector<std::optional<Rational>> ub;
    std::vector<std::optional<Rational>> lb;
    std::vector<Constraint> ub_reason;
    std::vector<Constraint> lb_reason;
    ub.resize(models.owned().num_vars(), {});
    lb.resize(models.owned().num_vars(), {});
    ub_reason.resize(models.owned().num_vars());
    lb_reason.resize(models.owned().num_vars());

    // compute actual bounds
    for (auto c : constraints)
    {
        if (c.empty() || !models.boolean().is_defined(c.lit().var().ord()))
        {
            continue;
        }

        auto cons = models.boolean().value(c.lit().var().ord()) == !c.lit().is_negation() ? c : ~c;
        bool unit = !models.owned().is_defined(cons.vars().front()) &&
                    std::all_of(cons.vars().begin() + 1, cons.vars().end(),
                                [&](auto ord) { return models.owned().is_defined(ord); });
        if (unit)
        {
            auto var = cons.vars().front();
            auto bound = cons.implied_value(models.owned()) / cons.coef().front();
            if (bounds.implies_upper_bound(cons))
            {
                if (!ub[var] || bound < *ub[var])
                {
                    ub[var] = bound;
                    ub_reason[var] = cons;
                }
            }

            if (bounds.implies_lower_bound(cons))
            {
                if (!lb[var] || bound > *lb[var])
                {
                    lb[var] = bound;
                    lb_reason[var] = cons;
                }
            }
        }
    }

    // check consistency with the bounds object
    for (int var_ord = 0; var_ord < static_cast<int>(ub.size()); ++var_ord)
    {
        if (models.owned().is_defined(var_ord))
        {
            continue;
        }

        auto& bnds = bounds[var_ord];
        if ([[maybe_unused]] auto lower_bound = bnds.lower_bound(models))
        {
            assert(!lb[var_ord] || lower_bound->value() >= *lb[var_ord]);
        }

        if ([[maybe_unused]] auto upper_bound = bnds.upper_bound(models))
        {
            assert(!ub[var_ord] || upper_bound->value() <= *ub[var_ord]);
        }
    }
}

void Linear_arithmetic::check_watch_consistency([[maybe_unused]] Models const& models)
{
    for (auto cons : constraints)
    {
        if (cons.empty())
        {
            continue;
        }

        // the first variable is assigned => all variables are assigned
        assert(!models.owned().is_defined(cons.vars().front()) ||
               std::all_of(cons.vars().begin(), cons.vars().end(),
                           [&](auto var) { return models.owned().is_defined(var); }));
        if (cons.size() > 1)
        {
            // the second variable is assigned => the constraint is unit
            assert(!models.owned().is_defined(cons.vars()[1]) ||
                   std::all_of(cons.vars().begin() + 2, cons.vars().end(),
                               [&](auto var) { return models.owned().is_defined(var); }));
        }

        for ([[maybe_unused]] auto var : cons.vars() | std::views::take(2))
        {
            assert(std::find_if(watched[var].begin(), watched[var].end(), [cons](auto& watch) {
                       return watch.constraint.lit().var() == cons.lit().var();
                   }) != watched[var].end());
        }
    }
}

} // namespace perun