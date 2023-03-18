#include "Terms.h"

#include <cassert>

namespace perun {
namespace terms {

namespace { // Hash functions. TODO: Improve
uint64_t hash_composite_term(Kind kind, std::span<term_t> args) {
    std::hash<int32_t> hasher;
    uint64_t result = hasher(static_cast<std::underlying_type<Kind>::type>(kind));
    for (term_t arg : args) {
        result = result * 31 + hasher(arg.x);
    }
    return result;
}

uint64_t hash_integer_term(Kind kind, type_t tau, int32_t index) {
    std::hash<int32_t> hasher;
    uint64_t result = hasher(static_cast<std::underlying_type<Kind>::type>(kind));
    result = result * 31 + hasher(tau);
    result = result * 31 + hasher(index);
    return result;
}

uint64_t hash_rational(Rational const& value) {
    std::hash<int32_t> hasher;
    uint64_t result = hasher(value.numerator());
    result = result * 31 + hasher(value.denominator());
    return result;
}
}

Term_table::Term_table()
{
    add_primitive_terms();
}

composite_term_descriptor_t::composite_term_descriptor_t(std::span<term_t> args)
    : m_args(args.begin(), args.end())
{
}

std::unique_ptr<composite_term_descriptor_t>
composite_term_descriptor_t::make(std::span<term_t> args)
{
    return std::unique_ptr<composite_term_descriptor_t>(new composite_term_descriptor_t(args));
}

std::unique_ptr<rational_term_descriptor_t>
rational_term_descriptor_t::make(Rational const& val)
{
    return std::unique_ptr<rational_term_descriptor_t>(new rational_term_descriptor_t(val));
}

std::unique_ptr<constant_term_descriptor_t>
constant_term_descriptor_t::make(int32_t index)
{
    return std::unique_ptr<constant_term_descriptor_t>(new constant_term_descriptor_t(index));
}




Kind Term_table::get_kind(term_t t) const { return inner_table[index_of(t)].kind; }

type_t Term_table::get_type(term_t t) const { return inner_table[index_of(t)].type; }

term_descriptor_t const& Term_table::get_descriptor(term_t t) const { return *inner_table[index_of(t)].descriptor; }

term_t Term_table::construct_composite(Kind kind, type_t type, std::span<term_t> args)
{
    auto term = composite_term_descriptor_t::make(args);
    auto index = static_cast<int32_t>(inner_table.size()); // TODO: Check Max term count
    this->inner_table.push_back(Term{kind, type, std::move(term)});
    return positive_term(index);
}

term_t Term_table::construct_rational(Kind kind, type_t type, Rational const& value)
{
    auto term = rational_term_descriptor_t::make(value);
    auto index = static_cast<int32_t>(inner_table.size()); // TODO: Check Max term count
    this->inner_table.push_back(Term{kind, type, std::move(term)});
    return positive_term(index);
}

term_t Term_table::construct_constant(Kind kind, type_t type, int32_t index)
{
    auto term = constant_term_descriptor_t::make(index);
    auto term_index = static_cast<int32_t>(inner_table.size()); // TODO: Check Max term count
    this->inner_table.push_back(Term{kind, type, std::move(term)});
    return positive_term(term_index);
}

term_t Term_table::construct_uninterpreted_constant(type_t type)
{
    auto term_descriptor = std::unique_ptr<term_descriptor_t>();
    auto term_index = static_cast<int32_t>(inner_table.size()); // TODO: Check Max term count
    this->inner_table.push_back(Term{Kind::UNINTERPRETED_TERM, type, std::move(term_descriptor)});
    return positive_term(term_index);
}

void Term_table::add_primitive_terms()
{
    assert(inner_table.empty());
    inner_table.push_back(Term{Kind::RESERVED_TERM, types::null_type, nullptr});

    term_t allocated_true_term = constant_term(types::bool_type, 0);
    assert(allocated_true_term == true_term);

    term_t allocated_zero = arithmetic_constant(Rational(0));
    assert(allocated_zero == zero_term);
}

term_t Term_table::constant_term(type_t tau, int32_t index)
{
    Constant_term_proxy proxy{Kind::CONSTANT_TERM, tau, hash_integer_term(Kind::CONSTANT_TERM, tau, index), *this, index};
    return known_terms.get_constant_term(proxy);
}

term_t Term_table::arithmetic_constant(Rational const& value)
{
    Rational_proxy proxy{hash_rational(value), *this, value};
    return known_terms.get_rational_term(proxy);
}

term_t Term_table::or_term(std::span<term_t> args)
{
    Composite_term_proxy proxy{Kind::OR_TERM, types::bool_type, hash_composite_term(Kind::OR_TERM, args), *this, args};
    return known_terms.get_composite_term(proxy);
}

term_t Term_table::arithmetic_product(Rational const& coeff, term_t var)
{
    assert(is_uninterpreted_constant(var));
    term_t coeff_term = arithmetic_constant(coeff);
    std::array<term_t, 2> args{var, coeff_term};
    Composite_term_proxy proxy{Kind::ARITH_PRODUCT, types::real_type, hash_composite_term(Kind::ARITH_PRODUCT, args), *this, args};
    return known_terms.get_composite_term(proxy);
}

term_t Term_table::arithmetic_polynomial(std::span<term_t> args)
{
    Composite_term_proxy proxy{Kind::ARITH_POLY, types::real_type, hash_composite_term(Kind::ARITH_POLY, args), *this, args};
    return known_terms.get_composite_term(proxy);
}

term_t Term_table::arithmetic_geq_zero(term_t t)
{
    assert(get_type(t) == types::real_type);
    std::array<term_t, 1> args{t};
    Composite_term_proxy proxy{Kind::ARITH_GE_ATOM, types::bool_type, hash_composite_term(Kind::ARITH_GE_ATOM, args), *this, args};
    return known_terms.get_composite_term(proxy);
}

term_t Term_table::arithmetic_eq_zero(term_t t)
{
    assert(get_type(t) == types::real_type);
    std::array<term_t, 1> args{t};
    Composite_term_proxy proxy{Kind::ARITH_EQ_ATOM, types::bool_type, hash_composite_term(Kind::ARITH_EQ_ATOM, args), *this, args};
    return known_terms.get_composite_term(proxy);
}

term_t Term_table::arithmetic_binary_eq(term_t t1, term_t t2)
{
    assert(get_type(t1) == types::real_type);
    assert(get_type(t2) == types::real_type);
    assert(get_kind(t1) != Kind::ARITH_PRODUCT and get_kind(t1) != Kind::ARITH_POLY);
    assert(get_kind(t2) != Kind::ARITH_PRODUCT and get_kind(t2) != Kind::ARITH_POLY);
    assert(t1 < t2);
    std::array<term_t, 2> args{t1, t2};
    Composite_term_proxy proxy{Kind::ARITH_BINEQ_ATOM, types::real_type, hash_composite_term(Kind::ARITH_BINEQ_ATOM, args), *this, args};
    return known_terms.get_composite_term(proxy);
}

/*
 * Declare a new uninterpreted constant of the given type.
 * Always creates a fresh term!
 */
term_t Term_table::new_uninterpreted_constant(type_t tau)
{
    // TODO: This does not insert the term into the hash table; Should it be inserted?
    return construct_uninterpreted_constant(tau);
}

void Term_table::set_term_name(term_t t, std::string const& name)
{
    {
        auto [it, inserted] = symbol_table.insert({name, t});
        assert(inserted);
        (void)inserted;
    }

    {
        auto [it, inserted] = name_table.insert({t, name});
        assert(inserted);
        (void)inserted;
    }
}

term_t Term_table::get_term_by_name(std::string const& name) const
{
    auto it = symbol_table.find(name);
    return it == symbol_table.end() ? null_term : it->second;
}

/*
 * Queries on terms
 */

bool Term_table::is_arithmetic_constant(term_t t) const
{
    return this->inner_table[index_of(t)].kind == Kind::ARITH_CONSTANT;
}

Rational const& Term_table::arithmetic_constant_value(term_t t) const
{
    assert(is_arithmetic_constant(t));
    auto const& descriptor = static_cast<rational_term_descriptor_t const&>(*this->inner_table[index_of(t)].descriptor);
    return descriptor.value();
}

bool Term_table::is_uninterpreted_constant(term_t t) const
{
    return this->inner_table[index_of(t)].kind == Kind::UNINTERPRETED_TERM;
}

bool Term_table::is_arithmetic_product(term_t t) const
{
    return this->inner_table[index_of(t)].kind == Kind::ARITH_PRODUCT;
}

bool Term_table::is_arithmetic_polynomial(term_t t) const
{
    return this->inner_table[index_of(t)].kind == Kind::ARITH_POLY;
}

term_t Term_table::var_of_product(term_t t) const
{
    assert(is_arithmetic_product(t));
    auto const& descriptor = static_cast<composite_term_descriptor_t const&>(*this->inner_table[index_of(t)].descriptor);
    assert(descriptor.size() == 2);
    return descriptor.args()[1];
}

Rational const& Term_table::coeff_of_product(term_t t) const
{
    assert(is_arithmetic_product(t));
    auto const& descriptor = static_cast<composite_term_descriptor_t const&>(*this->inner_table[index_of(t)].descriptor);
    assert(descriptor.size() == 2);
    return arithmetic_constant_value(descriptor.args()[0]);
}

std::span<const term_t> Term_table::monomials_of(term_t t) const
{
    assert(is_arithmetic_polynomial(t));
    auto const& descriptor = static_cast<composite_term_descriptor_t const&>(*this->inner_table[index_of(t)].descriptor);
    return descriptor.args();
}

std::span<const term_t> Term_table::get_args(term_t t) const
{
    auto kind = get_kind(t);
    switch (kind)
    {
    case Kind::ARITH_CONSTANT:
    case Kind::CONSTANT_TERM:
    case Kind::UNINTERPRETED_TERM:
    case Kind::VARIABLE:
        return {};
    default:
        term_descriptor_t const& descriptor = get_descriptor(t);
        auto const& composite_descriptor = dynamic_cast<composite_term_descriptor_t const&>(descriptor);
        return composite_descriptor.args();
    }
}

} // namespace terms
} // namespace perun