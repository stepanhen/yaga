//
// Created by sorx on 10/18/23.
//

#ifndef YAGA_INTERPOLATION_H
#define YAGA_INTERPOLATION_H

#include "Clause.h"
#include <utility>

namespace yaga {

class Interpolaion {
    public:
        std::pair<bool, Clause> interpolate(Clause a, Clause b, Variable::Type type);
        inline static Literal true_lit{std::numeric_limits<int>::max() - 1};
};
}
#endif // YAGA_INTERPOLATION_H
