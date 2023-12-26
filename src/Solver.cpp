#include "Solver.h"
#include <iostream>

namespace yaga {

Solver::Solver() : solver_trail(dispatcher)
{
    subsumption = std::make_unique<Subsumption>();
    dispatcher.add(subsumption.get());
}

std::vector<Clause> Solver::propagate() { return theory()->propagate(database, solver_trail); }

std::pair<std::vector<Clause>, int> Solver::analyze_conflicts(std::vector<Clause>&& conflicts)
{
    ++total_conflicts;
    std::vector<Clause> learned;
    int level = std::numeric_limits<int>::max();
    for (auto&& conflict : conflicts)
    {
        ++total_conflict_clauses;

        // derive clause suitable for backtracking
        auto [clause, clause_level] =
            analysis.analyze(trail(), std::move(conflict), [&](auto const& other_clause) {
                dispatcher.on_conflict_resolved(db(), trail(), other_clause);
            });

        if (!clause.empty())
        {
            subsumption->minimize(trail(), clause);
        }

        // find all conflict clauses at the lowest decision level
        if (clause_level < level)
        {
            level = clause_level;
            learned.clear();
            learned.push_back(std::move(clause));
        }
        else if (clause_level == level)
        {
            learned.push_back(std::move(clause));
        }
    }
    return {learned, level};
}
std::pair<std::vector<Clause>, int> Solver::analyze_conflicts_with_vars(std::vector<Clause>&& conflicts, const std::vector<Variable>& model)
{
    ++total_conflicts;
    std::vector<Clause> learned;
    int level = std::numeric_limits<int>::max();
    for (auto&& conflict : conflicts)
    {
        ++total_conflict_clauses;

        // derive clause suitable for backtracking
        auto [clause, clause_level] =
            analysis.analyze_with_vars(trail(), std::move(conflict), [&](auto const& other_clause) {
                dispatcher.on_conflict_resolved(db(), trail(), other_clause);
            }, std::move(model));

        if (!clause.empty())
        {
            subsumption->minimize(trail(), clause);
        }

        // find all conflict clauses at the lowest decision level
        if (clause_level < level)
        {
            level = clause_level;
            learned.clear();
            learned.push_back(std::move(clause));
        }
        else if (clause_level == level)
        {
            learned.push_back(std::move(clause));
        }
    }
    return {learned, level};
}
std::pair<std::vector<Clause>, int> Solver::analyze_final(std::vector<Clause>&& conflicts, const std::vector<Variable>& vars_to_skip)
{
   ++total_conflicts;
    std::vector<Clause> learned;
    int level = std::numeric_limits<int>::max();
    for (auto&& conflict : conflicts)
    {
        ++total_conflict_clauses;

        // derive clause suitable for backtracking
        auto [clause, clause_level] =
            analysis.analyze_final(trail(), std::move(conflict), [&](auto const& other_clause) {
                dispatcher.on_conflict_resolved(db(), trail(), other_clause);
            }, vars_to_skip);

        if (!clause.empty())
        {
            subsumption->minimize(trail(), clause);
        }

        // find all conflict clauses at the lowest decision level
        if (clause_level < level)
        {
            level = clause_level;
            learned.clear();
            learned.push_back(std::move(clause));
        }
        else if (clause_level == level)
        {
            learned.push_back(std::move(clause));
        }
    }
    return {learned, level};
}

Solver::Clause_range Solver::learn(std::vector<Clause>&& clauses)
{
    // remove duplicate clauses
    std::sort(clauses.begin(), clauses.end(), [](auto const& lhs, auto const& rhs) {
        if (lhs.size() < rhs.size())
        {
            return true;
        }
        else if (lhs.size() > rhs.size())
        {
            return false;
        }
        else // lhs.size() == rhs.size()
        {
            return lhs < rhs;
        }
    });
    clauses.erase(std::unique(clauses.begin(), clauses.end()), clauses.end());

    // prefer UIP clauses (propagations) over semantic split clauses (decisions)
    if (std::any_of(clauses.begin(), clauses.end(), [&](auto const& learned) {
        return !is_semantic_split(learned);
    }))
    {
        clauses.erase(std::remove_if(clauses.begin(), clauses.end(), [&](auto const& learned) {
            return is_semantic_split(learned);
        }), clauses.end());
    }

    for (auto const& clause : clauses)
    {
        ++total_learned_clauses;
        // add the clause to database
        auto& learned_ref = db().learn_clause(std::move(clause));
        // trigger events
        dispatcher.on_learned_clause(db(), trail(), learned_ref);
    }
    return {db().learned().begin() + (db().learned().size() - clauses.size()), 
            db().learned().end()};
}

bool Solver::is_semantic_split(Clause const& clause) const
{
    return clause.size() >= 2 && trail().decision_level(clause[0].var()).value() ==
                                     trail().decision_level(clause[1].var()).value();
}

void Solver::backtrack_with(Clause_range clauses, int level)
{
    dispatcher.on_before_backtrack(db(), trail(), level);

    auto& model = trail().model<bool>(Variable::boolean);
    if (is_semantic_split(clauses[0]))
    {
        assert(std::all_of(clauses.begin(), clauses.end(), [&](auto const& other_clause) {
            return is_semantic_split(other_clause);
        }));

        // find the best variable to decide
        auto top_it = clauses[0].begin();
        auto top_level = trail().decision_level(top_it->var()).value();
        auto it = top_it + 1;
        for (; it != clauses[0].end() && trail().decision_level(it->var()) == top_level; ++it)
        {
            assert(trail().reason(it->var()) == nullptr);
            if (variable_order->is_before(it->var(), top_it->var()))
            {
                top_it = it;
            }
        }

        // We have to backtrack a semantic decision. Otherwise, the proof of MCSat termination 
        // does not hold and the solver is not guaranteed to terminate.
        assert(trail().decision_level() >= level + 1);
        assert(trail().assigned(level + 1).front().var.type() != Variable::boolean);

        trail().backtrack(level);
        // decide one of the literals at the highest decision level
        trail().decide(top_it->var());
        model.set_value(top_it->var().ord(), !top_it->is_negation());
    }
    else // UIP
    {
        assert(std::all_of(clauses.begin(), clauses.end(), [&](auto const& other_clause) {
            return !is_semantic_split(other_clause);
        }));

        trail().backtrack(level);

        // propagate top level literals from all clauses
        for (auto& clause : clauses)
        {
            if (!model.is_defined(clause[0].var().ord()))
            {
                trail().propagate(clause[0].var(), &clause, level);
                model.set_value(clause[0].var().ord(), !clause[0].is_negation());
            }
        }
    }
}

std::optional<Variable> Solver::pick_variable() { return variable_order->pick(db(), trail()); }

void Solver::decide(Variable var)
{
    ++total_decisions;
    theory()->decide(db(), trail(), var);
}

void Solver::init()
{
    // allocate memory
    for (auto [type, model] : trail().models())
    {
        if (type == Variable::boolean)
        {
            num_bool_vars = model->num_vars();
        }
        dispatcher.on_variable_resize(type, model->num_vars());
    }

    // reset solver state
    total_conflicts = 0;
    total_decisions = 0;
    total_restarts = 0;
    dispatcher.on_init(db(), trail());
}

void Solver::restart()
{
    dispatcher.on_before_backtrack(db(), trail(), /*decision_level=*/0);

    ++total_restarts;
    trail().clear();

    dispatcher.on_restart(db(), trail());
}
Solver::Result Solver::check()
{
    init();

    for (;;)
    {
        auto conflicts = propagate();
        if (!conflicts.empty())
        {
            if (trail().decision_level() == 0)
            {
                return Result::unsat;
            }

            auto [learned, level] = analyze_conflicts(std::move(conflicts));
            if (std::any_of(learned.begin(), learned.end(), [](auto const& clause) { return clause.empty(); }))
            {
                return Result::unsat;
            }

            auto clauses = learn(std::move(learned));
            if (restart_policy->should_restart())
            {
                restart();
            }
            else // backtrack instead of restarting
            {
                backtrack_with(clauses, level);
            }
        }
        else // no conflict
        {
            auto var = pick_variable();
            if (!var)
            {
                return Result::sat;
            }
            decide(var.value());
        }
    }
}
std::pair<Solver::Result, std::vector<Clause>> Solver::check_with_model(const std::unordered_map<Variable, std::shared_ptr<Value>, Variable_hash>& model)
{
    init();
    std::vector<Variable> vars;

    for (auto& var : model) {
        vars.push_back(var.first);
    }

    for (;;)
    {
        auto conflicts = propagate();
        if (!conflicts.empty())
        {
            if (trail().decision_level() == 0)
            {
                return std::make_pair(Result::unsat, std::vector<Clause>{});
            }

            auto [learned, level] = analyze_conflicts_with_vars(std::move(conflicts), vars);
            if (std::any_of(learned.begin(), learned.end(), [](auto const& clause) { return clause.empty(); }))
            {
                auto i = analyze_final(std::move(conflicts), vars);
                return std::make_pair(Result::unsat, i.first);
            }

            auto clauses = learn(std::move(learned));
            if (restart_policy->should_restart())
            {
                restart();
            }
            else // backtrack instead of restarting
            {
                backtrack_with(clauses, level);
            }
        }
        else // no conflict
        {
            bool value_decided = false;

            auto& model_b = solver_trail.model<bool>(Variable::boolean);
            auto& model_r = solver_trail.model<Rational>(Variable::rational);
            for (auto& var : model) {
                // Check var type
                auto& value = *var.second;
                if (value.type() == Value::boolean)
                {
                    if (!model_b.is_defined(var.first.ord()))
                    {
                        std::cout << "DECIDING BOOL VALUE" << std::endl;
                        ++total_decisions;
                        theory()->decide_val(trail(), var.first, var.second);
//                        auto& bool_value = dynamic_cast<Bool_value&>(value);
//                        model_b.set_value(var.first.ord(), bool_value.get_value());
//                        trail().decide(var.first);
                        value_decided = true;
                        break;
                    }
                } else if (value.type() == Value::rational)
                {
                    if (!model_r.is_defined(var.first.ord()))
                    {
                        std::cout << "DECIDING RATIONAL VALUE" << std::endl;
                        ++total_decisions;
                        theory()->decide_val(trail(), var.first, var.second);
//                        auto& rational_value = dynamic_cast<Rational_value&>(value);
//                        model_r.set_value(var.first.ord(), rational_value.get_value());
//                        trail().decide(var.first);
                        value_decided = true;
                        break;
                    }
                }
            }
            if (!value_decided)
            {
                auto var = pick_variable();
                if (!var)
                {
                    return std::make_pair(Result::sat, std::vector<Clause>{});
                }

                decide(var.value());
            }
        }
    }
}

} // namespace yaga