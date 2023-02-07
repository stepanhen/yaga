#include "First_unassigned.h"

namespace perun {

std::optional<Variable> First_unassigned::pick(Database&, Trail& trail)
{
    auto models = trail.models();

    for (auto [type, model] : models)
    {
        if (!var_type || var_type == type)
        {
            for (int i = 0; i < static_cast<int>(model->num_vars()); ++i)
            {
                if (!model->is_defined(i))
                {
                    return Variable{i, type};
                }
            }
        }
    }
    return {};
}

} // namespace perun