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

void Resource::Release() {
    if (--m_ref_count <= 0) {
        // Resource can be garbage collected
        // The ResourceCache will handle actual deletion
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
