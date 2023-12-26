#ifndef YAGA_BOOL_THEORY_H
#define YAGA_BOOL_THEORY_H

#include <algorithm>
#include <optional>
#include <vector>
#include <ranges>

#include "Database.h"
#include "Literal.h"
#include "Literal_map.h"
#include "Model.h"
#include "Theory.h"
#include "Trail.h"

namespace yaga {

enum class Phase {
    /** Always decide true for boolean variables.
     */
    positive,

    /** Always decide false for boolean variables.
     */
    negative,

    /** Cache values of boolean variables.
     */
    cache,
};

class Bool_theory : public Theory {
public:
    virtual ~Bool_theory() = default;

    /** Run BCP to exhaustion
     *
     * @param db clause database
     * @param trail current solver trail
     * @return conflict clause if there is a conflict, none otherwise.
     */
    std::vector<Clause> propagate(Database& db, Trail& trail) override;
    void decide_val(Trail& trail, Variable var, std::shared_ptr<Value> value) override;
    /** Decide value for variable @p var if it is a boolean variable
     *
     * @param db clause database
     * @param trail current solver trail
     * @param var variable to decide
     */
    void decide(Database& db, Trail& trail, Variable var) override;

    /** Initialize @p learned clause
     *
     * @param db clause database
     * @param trail current solver trail
     * @param learned reference to the learned clause in @p db
     */
    void on_learned_clause(Database& db, Trail& trail, Clause const& learned) override;

    /** Cache variable polarity
     * 
     * @param db clause database
     * @param trail current solver trail
     * @param level decision level to backtrack to
     */
    void on_before_backtrack(Database&, Trail&, int) override;

    /** Allocates memory for @p num_vars watch lists if @p type is boolean
     *
     * @param type variable type
     * @param num_vars new number of variables of type @p type
     */
    void on_variable_resize(Variable::Type, int) override;

    /** Set phase of variables decided in `decide()`
     * 
     * @param phase phase of boolean variables decided in `decide()`
     */
    inline void set_phase(Phase phase) { var_phase = phase; }

private:
    // we move the watched literals to the first two position in each clause
    struct Watched_clause {
        // pointer to the watched clause in database
        Clause* clause;
        // the next index to check in clause
        int index;

        inline Watched_clause() {}
        inline explicit Watched_clause(Clause* clause)
            : clause(clause), index(std::min<int>(2, clause->size() - 1))
        {
        }
    };

    // satisfied literal with pointer to the reason clause (or nullptr)
    struct Satisfied_literal {
        // satisfied literal
        Literal lit;
        // clause that led to propagation of the literal or nullptr if there is none
        Clause* reason;

        /** Convert the structure to pair so we can tie the properties
         * 
         * @return pair of the values from this structure
         */
        inline operator std::pair<Literal, Clause*>() { return {lit, reason}; }
    };

    // map literal -> list of clauses in which it is watched
    Literal_map<std::vector<Watched_clause>> watched;
    // stack of true literals to propagate with a pointer to the reason clause
    std::vector<Satisfied_literal> satisfied;
    // cached variable phase
    std::vector<bool> phase;
    // phase strategy
    Phase var_phase{Phase::positive};

    /** Propagate assigned literals at current decision level in @p trail
     *
     * @param db clause database
     * @param trail current trail
     */
    void initialize(Database& db, Trail& trail);

    /** Move watch from recently falsified literal @p lit to some other literal.
     *
     * -# If some clause becomes unit, this method will propagate the implied
     * literal by adding it to `satisfied`
     * -# If some clause becomes false, this method will return a copy of that
     * clause.
     *
     * @param trail current solver trail
     * @param model current assignment of boolean variables
     * @param lit recently falsified literal in @p model
     * @return conflict clause if a clause becomes false. None, otherwise.
     */
    std::optional<Clause> falsified(Trail const& trail, Model<bool> const& model, Literal lit);

    /** Try to replace the second watched literal in @p watch with some other
     * non-falsified literal
     *
     * @param model current assignment of boolean variables
     * @param watch
     * @return true iff the second watched literal has been replaced with some non-falsified 
     * literal in the clause
     */
    bool replace_second_watch(Model<bool> const& model, Watched_clause& watch);
};

} // namespace yaga

#endif // YAGA_BOOL_THEORY_H