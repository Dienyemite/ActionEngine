#pragma once

#include <functional>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace action {

/*
 * Signal<Args...> — Godot-inspired type-safe signal/slot system.
 *
 * Usage:
 *   Signal<int, float> on_health_changed;
 *   on_health_changed.connect([](int old_hp, float new_hp){ ... });
 *   on_health_changed.emit(100, 50.5f);
 *
 * Connections can be disconnected by handle:
 *   auto handle = on_health_changed.connect(...);
 *   on_health_changed.disconnect(handle);
 *
 * Connections are automatically cleaned up when the Signal is destroyed.
 */
template<typename... Args>
class Signal {
public:
    using Callback = std::function<void(Args...)>;
    using Handle   = uint64_t;

    Signal() = default;
    ~Signal() = default;

    // Non-copyable, movable
    Signal(const Signal&) = delete;
    Signal& operator=(const Signal&) = delete;
    Signal(Signal&&) = default;
    Signal& operator=(Signal&&) = default;

    // Connect a callback. Returns a handle that can be used to disconnect.
    Handle connect(Callback cb) {
        Handle h = ++m_next_handle;
        m_connections.push_back({ h, std::move(cb) });
        return h;
    }

    // Disconnect by handle. Returns true if found.
    bool disconnect(Handle h) {
        auto it = std::find_if(m_connections.begin(), m_connections.end(),
            [h](const Connection& c){ return c.handle == h; });
        if (it != m_connections.end()) {
            m_connections.erase(it);
            return true;
        }
        return false;
    }

    // Disconnect all slots.
    void disconnect_all() {
        m_connections.clear();
    }

    // Emit the signal – calls all connected slots with the given arguments.
    void emit(Args... args) const {
        // Copy the list in case a slot modifies it
        auto copy = m_connections;
        for (const auto& c : copy) {
            c.callback(args...);
        }
    }

    // Number of connected slots.
    size_t connection_count() const { return m_connections.size(); }

    bool has_connections() const { return !m_connections.empty(); }

private:
    struct Connection {
        Handle   handle;
        Callback callback;
    };

    Handle m_next_handle = 0;
    std::vector<Connection> m_connections;
};

/*
 * SignalRef — a non-owning reference to a Signal that only allows connecting/
 * disconnecting (not emitting). Useful for exposing signals from objects while
 * keeping emit() private.
 *
 * Usage:
 *   class Button {
 *       Signal<> m_clicked;
 *   public:
 *       SignalRef<> on_clicked() { return m_clicked; }
 *   };
 */
template<typename... Args>
class SignalRef {
public:
    using Handle = typename Signal<Args...>::Handle;
    using Callback = typename Signal<Args...>::Callback;

    explicit SignalRef(Signal<Args...>& sig) : m_signal(&sig) {}

    Handle connect(Callback cb) { return m_signal->connect(std::move(cb)); }
    bool   disconnect(Handle h) { return m_signal->disconnect(h); }

private:
    Signal<Args...>* m_signal;
};

} // namespace action
