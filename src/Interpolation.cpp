//
// Created by sorx on 10/18/23.
//
#include "Interpolation.h"
#include "Solver.h"
#include <vector>

namespace yaga {
std::pair<bool, Clause> Interpolaion::interpolate(Clause a, Clause b, Variable::Type type) {
//    Solver s_a;
//    Solver s_b;
//    s_a.db().assert_clause(a);
//    s_b.db().assert_clause(b);
//
//    Clause i;
//    i.push_back(true_lit);
//
//    while (true) {
//        auto res_b = s_b.check();
//        if (res_b == Solver::Result::unsat) {
//           return std::make_pair(false, i);
//        }
//        auto res_a = s_a.check();
//        if (res_a == Solver::Result::sat) {
////           auto m_a = lra.relevant_models(solver.trail());
//
//        }
//    }


}
}