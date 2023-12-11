//
// Created by sorx on 12/4/23.
//

#ifndef YAGA_RATIONAL_VALUE_H
#define YAGA_RATIONAL_VALUE_H

#include "Long_fraction.h"
#include "Value.h"

namespace yaga {

class Rational_value : public Value {
public:
    inline Rational_value(Long_fraction v) { value = v; }
    inline Long_fraction get_value() const { return value; }
    inline Type type() override { return Value::rational; }
private:
    Long_fraction value;
};
}
#endif // YAGA_RATIONAL_VALUE_H
