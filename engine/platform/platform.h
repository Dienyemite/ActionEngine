#pragma once

#include "core/types.h"
#include <string>

// Forward declare platform-specific types
#ifdef PLATFORM_WINDOWS
struct HWND__;
typedef HWND__* HWND;
#endif

namespace action {

// Input key codes
enum class Key : u16 {
    Unknown = 0,
    
    // Letters
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    
    // Numbers
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    
    // Function keys
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    
    // Special keys
    Escape, Tab, CapsLock, Shift, Control, Alt, Space, Enter, Backspace,
    Insert, Delete, Home, End, PageUp, PageDown,
    Left, Right, Up, Down,
    
    // Mouse buttons (treated as keys for simplicity)
    MouseLeft, MouseRight, MouseMiddle, Mouse4, Mouse5,
    
    Count
};

// Mouse state
struct MouseState {
    i32 x = 0;
    i32 y = 0;
    i32 delta_x = 0;
    i32 delta_y = 0;
    float scroll_delta = 0;
    bool captured = false;
};

// Input state
class Input {
public:
    void Update();
    
    bool IsKeyDown(Key key) const;
    bool IsKeyPressed(Key key) const;  // Just pressed this frame
    bool IsKeyReleased(Key key) const; // Just released this frame
    
    const MouseState& GetMouse() const { return m_mouse; }
    
    void SetMouseCapture(bool capture);
    
    // Called by platform layer
    void OnKeyDown(Key key);
    void OnKeyUp(Key key);
    void OnMouseMove(i32 x, i32 y, i32 dx, i32 dy);
    void OnMouseScroll(float delta);
    
private:
    std::array<bool, static_cast<size_t>(Key::Count)> m_current{};
    std::array<bool, static_cast<size_t>(Key::Count)> m_previous{};
    MouseState m_mouse{};
};

// Platform abstraction
class Platform {
public:
    Platform() = default;
    ~Platform() = default;
    
    bool Initialize(u32 width, u32 height, const std::string& title, bool fullscreen);
    void Shutdown();
    
    void PollEvents();
    bool ShouldClose() const { return m_should_close; }
    void RequestClose() { m_should_close = true; }
    
    // Window info
    u32 GetWidth() const { return m_width; }
    u32 GetHeight() const { return m_height; }
    float GetAspectRatio() const { return static_cast<float>(m_width) / m_height; }
    bool IsFullscreen() const { return m_fullscreen; }
    bool IsFocused() const { return m_focused; }
    
    void SetTitle(const std::string& title);
    void SetFullscreen(bool fullscreen);
    
    // Platform-specific handles
#ifdef PLATFORM_WINDOWS
    HWND GetWindowHandle() const { return m_hwnd; }
    void* GetHInstance() const { return m_hinstance; }
#endif
    
    // Input
    Input& GetInput() { return m_input; }
    const Input& GetInput() const { return m_input; }
    
    // Timing
    double GetTime() const;
    void Sleep(u32 milliseconds);
    
    // File dialogs
    std::string OpenFileDialog(const std::string& title, const std::string& filter, const std::string& default_path = "");
    std::string SaveFileDialog(const std::string& title, const std::string& filter, const std::string& default_path = "");
    
private:
    u32 m_width = 0;
    u32 m_height = 0;
    bool m_fullscreen = false;
    bool m_should_close = false;
    bool m_focused = true;
    
    Input m_input;
    
#ifdef PLATFORM_WINDOWS
    HWND m_hwnd = nullptr;
    void* m_hinstance = nullptr;
    static class Platform* s_instance;
    
    static long long __stdcall WindowProc(HWND hwnd, unsigned int msg, 
                                           unsigned long long wparam, long long lparam);
#endif
};

} // namespace action
