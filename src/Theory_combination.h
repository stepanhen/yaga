#ifndef YAGA_THEORY_COMBINATION_H
#define YAGA_THEORY_COMBINATION_H

#include <concepts>
#include <deque>
#include <memory>
#include <ranges>

#include "Clause.h"
#include "Database.h"
#include "Theory.h"
#include "Trail.h"
#include "Variable.h"

namespace yaga {

/** Combination of several theories.
 */
class Theory_combination final : public Theory {
public:
    virtual ~Theory_combination() = default;

    /** Run propagate in all theories until no new propagations are generated.
     *
     * @param db clause database
     * @param trail current solver trail
     * @return conflict clause if a conflict is detected by any theory
     */
    std::vector<Clause> propagate(Database&, Trail&) override;
    void decide_val(Trail&, Variable, std::shared_ptr<Value>) override;
    /** Call decide in all theories
     *
     * @param db clause database
     * @param trail current solver trail
     * @param var variable to decide
     */
    void decide(Database&, Trail&, Variable) override;

    /** Call the event in all theories.
     *
     * @param db clause database
     * @param trail current solver trail
     */
    void on_init(Database&, Trail&) override;

    /** Call the event in all theories
     *
     * @param db clause database
     * @param trail current solver trail before backtracking
     * @param decision_level decision level to backtrack to
     */
    void on_before_backtrack(Database&, Trail&, int) override;

    /** Call the event in all theories.
     *
     * @param type type of variables
     * @param num_vars new number of variables of type @p type
     */
    void on_variable_resize(Variable::Type, int) override;

    /** Call the event in all theories.
     *
     * @param db clause database
     * @param trail current solver trail
     * @param learned newly learned clause
     */
    void on_learned_clause(Database&, Trail&, Clause const&) override;

    /** Call the event in all theories.
     *
     * @param db clause database
     * @param trail current solver trail
     * @param other clause that is resolved with current conflict clause
     */
    void on_conflict_resolved(Database&, Trail&, Clause const&) override;

    /** Call the event in all theories.
     *
     * @param db clause database
     * @param trail current solver trail
     */
    void on_restart(Database&, Trail&) override;

    /** Create a new theory and add it to this object.
     *
     * @tparam T type of the theory to create
     * @tparam Args types of arguments passed to a constructor of T
     * @param args arguments forwarded to a constructor of T
     * @return reference to the new theory in this object
     */
    template <class T, typename... Args>
        requires std::is_base_of_v<Theory, T>
    inline T& add_theory(Args&&... args)
    {
        auto theory = std::make_unique<T>(std::forward<Args>(args)...);
        for (std::size_t type = 0; type < current_num_vars.size(); ++type)
        {
            if (current_num_vars[type] > 0)
            {
                theory->on_variable_resize(static_cast<Variable::Type>(type),
                                           current_num_vars[type]);
            }
        }
        auto conc_theory_ptr = theory.get();
        theory_list.emplace_back(std::move(theory));
        return *conc_theory_ptr;
    }

    /** Get all theories in this object
     * 
     * @return range of theory pointers in this object
     */
    inline std::ranges::view auto theories()
    {
        return theory_list | std::views::transform([](auto& ptr) { return ptr.get(); });
    }

private:
    std::deque<std::unique_ptr<Theory>> theory_list;
    std::vector<int> current_num_vars;
};

} // namespace yaga

#endif // YAGA_THEORY_COMBINATION_H