#ifndef PERUN_LITERAL_H
#define PERUN_LITERAL_H

#include <cmath>
#include <functional>
#include <ostream>

#include "Variable.h"

namespace perun {

class Literal_hash;

/** Boolean variable or its negation.
 */
class Literal {
public:
    friend class Literal_hash;

    // default-constructible
    inline Literal() {}

    // copyable
    inline Literal(Literal const&) = default;
    inline Literal& operator=(Literal const&) = default;

    /** Check whether two literals are equal.
     *
     * @param other other literal
     * @return true iff this literal is equalivant to @p other
     */
    inline bool operator==(Literal const& other) const { return value == other.value; }

    /** Check whether two literals are different.
     *
     * @param other other literal
     * @return true iff this literal is not equalivant to @p other
     */
    inline bool operator!=(Literal const& other) const { return !operator==(other); }

    /** Construct a literal from boolean variable ordinal number
     *
     * @param var_ord 0-based ordinal number of a boolean variable
     */
    inline explicit Literal(int var_ord)
        : value(var_ord + 1) {} // + 1 so that we can represent 0 and its negation

    /** Get negation of this literal
     *
     * @return new literal which represents negation of this literal
     */
    inline Literal negate() const
    {
        Literal r;
        r.value = -value;
        return r;
    }

    /** Get representation of the boolean variable used in this literal
     *
     * @return boolean variable used in this literal
     */
    inline Variable var() const { return {std::abs(value) - 1, Variable::boolean}; }

    /** Check whether `var()` is negated in this literal.
     *
     * @return true iff `var()` is negated in this literal
     */
    inline bool is_negation() const { return value < 0; }

private:
    // `variable ordinal + 1`, negative value indicates negative literal
    int value;
};

inline std::ostream& operator<<(std::ostream& out, Literal lit)
{
    if (lit.is_negation())
    {
        out << "not(";
    }
    out << lit.var();
    if (lit.is_negation())
    {
        out << ")";
    }
    return out;
}

class Literal_hash {
public:
    /** Compute hash of a literal
     *
     * @param l literal
     * @return hash of @p l
     */
    inline std::size_t operator()(Literal l) const { return hash(l.value); }

private:
    std::hash<int> hash;
};

} // namespace perun

#endif // PERUN_LITERAL_H