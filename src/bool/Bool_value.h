//
// Created by sorx on 12/4/23.
//

#ifndef YAGA_BOOL_VALUE_H
#define YAGA_BOOL_VALUE_H

#include "Value.h"

namespace yaga {

class Bool_value : public Value {
public:
   inline bool get_value() const { return value; }
   inline Type type()  override{ return Value::boolean; }
private:
    bool value;
};
}

#endif // YAGA_BOOL_VALUE_H
