//
// Created by sorx on 12/4/23.
//

#ifndef YAGA_VALUE_H
#define YAGA_VALUE_H

namespace yaga {

class Value {
public:
    enum Type {
        boolean = 0,
        rational = 1
    };
    virtual Type type() = 0;
};
}

#endif // YAGA_VALUE_H
