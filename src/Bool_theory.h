#ifndef PERUN_BOOL_THEORY_H
#define PERUN_BOOL_THEORY_H

#include <algorithm>
#include <optional>
#include <vector>

#include "Database.h"
#include "Literal.h"
#include "Literal_map.h"
#include "Model.h"
#include "Theory.h"
#include "Trail.h"

namespace perun {

class Bool_theory : public Theory {
public:
    virtual ~Bool_theory() = default;

    /** Run BCP to exhaustion
     *
     * @param db clause database
     * @param trail current solver trail
     * @return conflict clause if there is a conflict, none otherwise.
     */
    std::optional<Clause> propagate(Database& db, Trail& trail) override;

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
     * @param learned pointer to the learned clause in @p db
     */
    void on_learned_clause(Database& db, Trail& trail, Clause* learned) override;

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

    // map literal -> list of clauses in which it is watched
    Literal_map<std::vector<Watched_clause>> watched;
    // stack of true literals to propagate with a pointer to the reason clause
    std::vector<std::pair<Literal, Clause*>> satisfied;

    /** Propagate assigned literals at current decision level in @p trail
     *
     * @param db clause database
     * @param trail current trail
     * @return conflict clause if a clause becomes false. None, otherwise.
     */
    std::optional<Clause> initialize(Database& db, Trail& trail);

    /** Move watch from recently falsified literal @p lit to some other literal.
     *
     * -# If some clause becomes unit, this method will propagate the implied
     * literal by adding it to `satisfied`
     * -# If some clause becomes false, this method will return a copy of that
     * clause.
     *
     * @param model current assignment of boolean variables
     * @param lit recently falsified literal in @p model
     * @return conflict clause if a clause becomes flase. None, otherwise.
     */
    std::optional<Clause> falsified(Model<bool> const& model, Literal lit);

    /** Try to replace the second watched literal in @p watch with some other
     * non-falsified literal
     *
     * @param model current assignment of boolean variables
     * @param watch
     * @return true iff the second watched literal has been replaced with some
     * other non-falsified literal in the clause
     */
    bool replace_second_watch(Model<bool> const& model, Watched_clause& watch);
};

} // namespace perun

#endif // PERUN_BOOL_THEORY_H