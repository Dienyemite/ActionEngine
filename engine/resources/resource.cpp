#include "resource.h"
#include "resource_cache.h"
#include "core/logging.h"

namespace action {

ResourceID Resource::s_next_id = 1;

Resource::Resource()
    : m_id(s_next_id++)
{
}

Resource::Resource(const std::string& path)
    : m_id(s_next_id++)
    , m_path(path)
{
}

Resource::~Resource() {
    if (m_state == ResourceState::Loaded) {
        Unload();
    }
}

long Resource::GetUseCount() const {
    // Try to get the shared_ptr use_count. This is safe because enable_shared_from_this
    // stores a weak_ptr internally. If no shared_ptr manages this object, return 0.
    try {
        auto sp = const_cast<Resource*>(this)->shared_from_this();
        return sp.use_count() - 1; // Subtract the temporary we just created
    } catch (const std::bad_weak_ptr&) {
        return 0; // Not managed by shared_ptr
    }
}

void Resource::Reload() {
    if (m_path.empty()) {
        LOG_WARN("Cannot reload resource without path");
        return;
    }
    
    Unload();
    m_state = ResourceState::Unloaded;
    Load();
}

} // namespace action
