#pragma once

#include "core/types.h"

namespace action {

/*
 * Audio System (Stub)
 * 
 * TODO: Implement with platform audio API or library
 */

class Audio {
public:
    bool Initialize();
    void Shutdown();
    void Update(float dt);
};

} // namespace action
