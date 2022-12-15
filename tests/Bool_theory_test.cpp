#include <catch2/catch_test_macros.hpp>

#include "test.h"
#include "Bool_theory.h"

TEST_CASE("Propagate unit clauses if the trail is empty", "[bool_theory][bcp]")
{
    using namespace perun;
    using namespace perun::test;

    Database db;
    db.assert_clause(lit(0), lit(1), lit(2));
    db.assert_clause(-lit(0));
    db.assert_clause(-lit(1));

    Trail trail;
    auto& model = trail.add_model<bool>(Variable::boolean);
    trail.resize(Variable::boolean, 10);

    Bool_theory theory;
    auto conflict = theory.propagate(db, trail);
    REQUIRE(!conflict.has_value());
    
    REQUIRE(trail.assigned(0).size() == 3);
    REQUIRE(trail.assigned(0).begin()->var == bool_var(1));
    REQUIRE(trail.assigned(0).begin()->reason == &*(db.asserted().begin() + 2));
    REQUIRE((trail.assigned(0).begin() + 1)->var == bool_var(0));
    REQUIRE((trail.assigned(0).begin() + 1)->reason == &*(db.asserted().begin() + 1));
    REQUIRE((trail.assigned(0).begin() + 2)->var == bool_var(2));
    REQUIRE((trail.assigned(0).begin() + 2)->reason == &*db.asserted().begin());

    REQUIRE(model.is_defined(0));
    REQUIRE(model.value(0) == false);

    REQUIRE(model.is_defined(1));
    REQUIRE(model.value(1) == false);

    REQUIRE(model.is_defined(2));
    REQUIRE(model.value(2) == true);
}

TEST_CASE("Run BCP after a value is decided", "[bool_theory][bcp]")
{
    using namespace perun;
    using namespace perun::test;

    Bool_theory theory;

    Database db;
    db.assert_clause(lit(0), lit(1));
    db.assert_clause(-lit(0), -lit(2));
    db.assert_clause(lit(0), lit(3));

    Trail trail;
    auto& model = trail.add_model<bool>(Variable::boolean);
    trail.resize(Variable::boolean, 10);

    // initialize watch lists
    auto conflict = theory.propagate(db, trail);
    REQUIRE(!conflict.has_value());
    REQUIRE(trail.empty());

    // decide value
    model.set_value(0, false);
    trail.decide(bool_var(0));

    // propagate after the decision
    conflict = theory.propagate(db, trail);
    REQUIRE(!conflict.has_value());

    REQUIRE(trail.assigned(0).size() == 0);
    REQUIRE(trail.assigned(1).size() == 3);
    REQUIRE(trail.assigned(1).begin()->var == bool_var(0));
    REQUIRE(trail.assigned(1).begin()->reason == nullptr);
    REQUIRE((trail.assigned(1).begin() + 1)->var == bool_var(3));
    REQUIRE((trail.assigned(1).begin() + 1)->reason == &*(db.asserted().begin() + 2));
    REQUIRE((trail.assigned(1).begin() + 2)->var == bool_var(1));
    REQUIRE((trail.assigned(1).begin() + 2)->reason == &*db.asserted().begin());

    REQUIRE(model.is_defined(0));
    REQUIRE(model.value(0) == false);

    REQUIRE(model.is_defined(1));
    REQUIRE(model.value(1) == true);

    REQUIRE(!model.is_defined(2));

    REQUIRE(model.is_defined(3));
    REQUIRE(model.value(3) == true);
}

TEST_CASE("Run BCP after backtracking", "[bool_theory][bcp]")
{
    using namespace perun;
    using namespace perun::test;

    Bool_theory theory;

    Database db;
    db.assert_clause(lit(0), lit(1));
    db.assert_clause(-lit(0));
    db.assert_clause(-lit(1), -lit(2), lit(3));

    Trail trail;
    auto& model = trail.add_model<bool>(Variable::boolean);
    trail.resize(Variable::boolean, 10);

    // init
    auto conflict = theory.propagate(db, trail);
    REQUIRE(!conflict.has_value());

    model.set_value(2, true);
    trail.decide(bool_var(2));
    trail.backtrack(0);

    conflict = theory.propagate(db, trail);
    REQUIRE(!conflict.has_value());

    REQUIRE(trail.assigned(0).size() == 2);
    REQUIRE(trail.assigned(0).begin()->var == bool_var(0));
    REQUIRE(trail.assigned(0).begin()->reason == &*(db.asserted().begin() + 1));
    REQUIRE((trail.assigned(0).begin() + 1)->var == bool_var(1));
    REQUIRE((trail.assigned(0).begin() + 1)->reason == &*(db.asserted().begin()));

    REQUIRE(model.is_defined(0));
    REQUIRE(model.value(0) == false);

    REQUIRE(model.is_defined(1));
    REQUIRE(model.value(1) == true);

    REQUIRE(!model.is_defined(2));
    REQUIRE(!model.is_defined(3));
}

TEST_CASE("Skip satisfied clauses", "[bool_theory][bcp]")
{
    using namespace perun;
    using namespace perun::test;

    Bool_theory theory;

    Database db;
    db.assert_clause(lit(0), lit(1));
    db.assert_clause(-lit(0), lit(1), lit(2));

    Trail trail;
    auto& model = trail.add_model<bool>(Variable::boolean);
    trail.resize(Variable::boolean, 10);

    auto conflict = theory.propagate(db, trail);
    REQUIRE(!conflict);
    REQUIRE(!model.is_defined(0));
    REQUIRE(!model.is_defined(1));
    REQUIRE(!model.is_defined(2));

    model.set_value(0, true);
    trail.decide(bool_var(0));

    conflict = theory.propagate(db, trail);
    REQUIRE(!conflict);
    REQUIRE(model.is_defined(0));
    REQUIRE(model.value(0) == true);
    REQUIRE(!model.is_defined(1));
    REQUIRE(!model.is_defined(2));

    model.set_value(1, false);
    trail.decide(bool_var(1));

    conflict = theory.propagate(db, trail);
    REQUIRE(!conflict);
    REQUIRE(model.is_defined(0));
    REQUIRE(model.value(0) == true);
    REQUIRE(model.is_defined(1));
    REQUIRE(model.value(1) == false);
    REQUIRE(model.is_defined(2));
    REQUIRE(model.value(2) == true);

}