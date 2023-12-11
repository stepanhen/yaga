#ifndef YAGA_CONFLICT_ANALYSIS_H
#define YAGA_CONFLICT_ANALYSIS_H

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <optional>
#include <tuple>
#include <unordered_set>
#include <vector>

#include "Clause.h"
#include "Trail.h"

namespace yaga {

class Conflict_analysis {
public:
    template <std::invocable<Clause const&> Resolve_callback>
    std::pair<Clause, int> analyze_final(Trail const& trail, Clause&& conflict,
                                   Resolve_callback&& on_resolve, const std::vector<Variable>& vars_to_check)
    {
        auto const& model = trail.model<bool>(Variable::boolean);
        assert(eval(model, conflict) == false);

        init(trail, conflict);

        auto const& assigned = trail.assigned(top_level);
        for (auto it = assigned.rbegin(); !can_backtrack() && it != assigned.rend(); ++it)
        {
            auto [var, reason] = *it;
            auto var_to_check = std::find(vars_to_check.begin(), vars_to_check.end(), var);
            if (var_to_check != vars_to_check.end()) {
                // If the variable is part of the set x, skip it and resolve next one
                continue;
            }
            if (var.type() == Variable::boolean && reason != nullptr &&
                trail.decision_level(var).value() == top_level)
            {
                auto lit =
                    model.value(var.ord()) ? ~Literal{var.ord()} : Literal{var.ord()};
                if (can_resolve(lit))
                {
                    on_resolve(*reason);
                    resolve(trail, *reason, lit);
                }
            }
        }

        return finish(trail);
    }

    inline std::pair<Clause, int> analyze_final(Trail const& trail, Clause&& conflict, const std::vector<Variable>& vars_to_check)
    {
        return analyze_final(trail, std::move(conflict), [](auto const&) {}, vars_to_check);
    }
    /** Derive a conflict clause suitable for backtracking using resolution.
     *
     * Postcondition: Literals in the returned clause are ordered by decision level from
     * the highest to the smallest.
     *
     * @tparam Resolve_callback function which takes a clause reference as a parameter
     * @param trail current trail
     * @param conflict conflict clause -- clause that is false in @p trail
     * @param on_resolve callback called for each clause that is resolved with @p conflict
     * @return conflict clause suitable for backtracking and decision level to backtrack to.
     */
    template <std::invocable<Clause const&> Resolve_callback>
    std::pair<Clause, int> analyze(Trail const& trail, Clause&& conflict,
                                   Resolve_callback&& on_resolve)
    {
        auto const& model = trail.model<bool>(Variable::boolean);
        assert(eval(model, conflict) == false);

        init(trail, conflict);

        auto const& assigned = trail.assigned(top_level);
        for (auto it = assigned.rbegin(); !can_backtrack() && it != assigned.rend(); ++it)
        {
            auto [var, reason] = *it;
            if (var.type() == Variable::boolean && reason != nullptr &&
                trail.decision_level(var).value() == top_level)
            {
                auto lit =
                    model.value(var.ord()) ? ~Literal{var.ord()} : Literal{var.ord()};
                if (can_resolve(lit))
                {
                    on_resolve(*reason);
                    resolve(trail, *reason, lit);
                }
            }
        }

        return finish(trail);
    }

    inline std::pair<Clause, int> analyze(Trail const& trail, Clause&& conflict)
    {
        return analyze(trail, std::move(conflict), [](auto const&) {});
    }
    /** Derive a conflict clause suitable for backtracking using resolution.
     *
     * Postcondition: Literals in the returned clause are ordered by decision level from
     * the highest to the smallest.
     *
     * @tparam Resolve_callback function which takes a clause reference as a parameter
     * @param trail current trail
     * @param conflict conflict clause -- clause that is false in @p trail
     * @param on_resolve callback called for each clause that is resolved with @p conflict
     * @return conflict clause suitable for backtracking and decision level to backtrack to.
     */
    template <std::invocable<Clause const&> Resolve_callback>
    std::pair<Clause, int> analyze_with_vars(Trail const& trail, Clause&& conflict,
                                   Resolve_callback&& on_resolve, const std::vector<Variable>& vars_to_check)
    {
        auto const& model = trail.model<bool>(Variable::boolean);
        assert(eval(model, conflict) == false);

        init(trail, conflict);

        auto const& assigned = trail.assigned(top_level);
        for (auto it = assigned.rbegin(); !can_backtrack() && it != assigned.rend(); ++it)
        {
            auto [var, reason] = *it;
            auto var_to_check = std::find(vars_to_check.begin(), vars_to_check.end(), var);
            if (var_to_check != vars_to_check.end()) {
                // If the variable is part of the set x, return the current conflict and true
                return finish(trail);
            }
            if (var.type() == Variable::boolean && reason != nullptr &&
                trail.decision_level(var).value() == top_level)
            {
                auto lit =
                    model.value(var.ord()) ? ~Literal{var.ord()} : Literal{var.ord()};
                if (can_resolve(lit))
                {
                    on_resolve(*reason);
                    resolve(trail, *reason, lit);
                }
            }
        }

        return finish(trail);
    }

    inline std::pair<Clause, int> analyze_with_vars(Trail const& trail, Clause&& conflict, const std::vector<Variable>& vars_to_check)
    {
        return analyze_with_vars(trail, std::move(conflict), [](auto const&) {}, vars_to_check);
    }

private:
    // TODO: use some open addressing implementation?
    // current conflict clause
    std::unordered_set<Literal, Literal_hash> conflict;
    // the highest decision level in current conflict clause
    int top_level;
    // number of literals at the `top_level` in current conflict clause
    int num_top_level;

    // check if solver can backtrack with current conflict clause
    inline bool can_backtrack() const { return num_top_level == 1 && conflict.size() > 1; }
    // check if current conflict clause contains lit
    inline bool can_resolve(Literal lit) const { return conflict.contains(lit); }
    // initialize current conflict clause
    void init(Trail const& trail, Clause const& conflict);
    // resolve current conflict with other clause using literal lit
    // (precondition: can_resolve(lit))
    void resolve(Trail const& trail, Clause const& other, Literal lit);
    // finish the conflict derivation
    std::pair<Clause, int> finish(Trail const& trail) const;
};

} // namespace yaga

#endif // YAGA_CONFLICT_ANALYSIS_H