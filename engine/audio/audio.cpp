#include "audio.h"
#include "core/logging.h"

namespace action {

bool Audio::Initialize() {
    LOG_INFO("Audio initialized (stub)");
    return true;
}

void Audio::Shutdown() {
    LOG_INFO("Audio shutdown");
}

void Audio::Update(float dt) {
    (void)dt;
}

} // namespace action
