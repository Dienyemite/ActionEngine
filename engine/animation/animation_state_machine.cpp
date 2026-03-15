#include "animation_state_machine.h"
#include <cmath>

namespace action {

bool AnimTransition::Evaluate(
    const std::unordered_map<std::string, float>& floats,
    const std::unordered_map<std::string, bool>&  bools,
    std::unordered_map<std::string, bool>&         triggers) const
{
    for (const auto& cond : conditions) {
        switch (cond.op) {
            case AnimCondition::Op::FloatEqual:
            case AnimCondition::Op::FloatNotEqual:
            case AnimCondition::Op::FloatGreater:
            case AnimCondition::Op::FloatLess:
            case AnimCondition::Op::FloatGreaterEq:
            case AnimCondition::Op::FloatLessEq: {
                auto it = floats.find(cond.parameter);
                float val = (it != floats.end()) ? it->second : 0.0f;
                bool pass = false;
                switch (cond.op) {
                    case AnimCondition::Op::FloatEqual:     pass = std::abs(val - cond.threshold) < 1e-5f; break;
                    case AnimCondition::Op::FloatNotEqual:  pass = std::abs(val - cond.threshold) >= 1e-5f; break;
                    case AnimCondition::Op::FloatGreater:   pass = val > cond.threshold; break;
                    case AnimCondition::Op::FloatLess:      pass = val < cond.threshold; break;
                    case AnimCondition::Op::FloatGreaterEq: pass = val >= cond.threshold; break;
                    case AnimCondition::Op::FloatLessEq:    pass = val <= cond.threshold; break;
                    default: break;
                }
                if (!pass) return false;
                break;
            }

            case AnimCondition::Op::BoolTrue:
            case AnimCondition::Op::BoolFalse: {
                auto it = bools.find(cond.parameter);
                bool val = (it != bools.end()) && it->second;
                if (cond.op == AnimCondition::Op::BoolTrue  && !val) return false;
                if (cond.op == AnimCondition::Op::BoolFalse &&  val) return false;
                break;
            }

            case AnimCondition::Op::Trigger: {
                auto it = triggers.find(cond.parameter);
                if (it == triggers.end() || !it->second) return false;
                // Clear the trigger — it fires once
                it->second = false;
                break;
            }
        }
    }
    return true;
}

} // namespace action
