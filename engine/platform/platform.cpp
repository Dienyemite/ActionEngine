#include "platform.h"
#include "core/logging.h"

#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#include <windowsx.h>

// Forward declare ImGui Win32 handler (C++ linkage)
LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

namespace action {

// Input implementation
void Input::Update() {
    m_previous = m_current;
    m_mouse.delta_x = 0;
    m_mouse.delta_y = 0;
    m_mouse.scroll_delta = 0;
}

bool Input::IsKeyDown(Key key) const {
    return m_current[static_cast<size_t>(key)];
}

bool Input::IsKeyPressed(Key key) const {
    size_t idx = static_cast<size_t>(key);
    return m_current[idx] && !m_previous[idx];
}

bool Input::IsKeyReleased(Key key) const {
    size_t idx = static_cast<size_t>(key);
    return !m_current[idx] && m_previous[idx];
}

void Input::OnKeyDown(Key key) {
    m_current[static_cast<size_t>(key)] = true;
}

void Input::OnKeyUp(Key key) {
    m_current[static_cast<size_t>(key)] = false;
}

void Input::OnMouseMove(i32 x, i32 y, i32 dx, i32 dy) {
    m_mouse.x = x;
    m_mouse.y = y;
    m_mouse.delta_x += dx;
    m_mouse.delta_y += dy;
}

void Input::OnMouseScroll(float delta) {
    m_mouse.scroll_delta = delta;
}

void Input::SetMouseCapture(bool capture) {
    m_mouse.captured = capture;
#ifdef PLATFORM_WINDOWS
    if (capture) {
        SetCapture(GetActiveWindow());
        ShowCursor(FALSE);
    } else {
        ReleaseCapture();
        ShowCursor(TRUE);
    }
#endif
}

#ifdef PLATFORM_WINDOWS

Platform* Platform::s_instance = nullptr;

// Windows key mapping
static Key MapWindowsKey(WPARAM wparam) {
    switch (wparam) {
        case 'A': return Key::A;
        case 'B': return Key::B;
        case 'C': return Key::C;
        case 'D': return Key::D;
        case 'E': return Key::E;
        case 'F': return Key::F;
        case 'G': return Key::G;
        case 'H': return Key::H;
        case 'I': return Key::I;
        case 'J': return Key::J;
        case 'K': return Key::K;
        case 'L': return Key::L;
        case 'M': return Key::M;
        case 'N': return Key::N;
        case 'O': return Key::O;
        case 'P': return Key::P;
        case 'Q': return Key::Q;
        case 'R': return Key::R;
        case 'S': return Key::S;
        case 'T': return Key::T;
        case 'U': return Key::U;
        case 'V': return Key::V;
        case 'W': return Key::W;
        case 'X': return Key::X;
        case 'Y': return Key::Y;
        case 'Z': return Key::Z;
        case '0': return Key::Num0;
        case '1': return Key::Num1;
        case '2': return Key::Num2;
        case '3': return Key::Num3;
        case '4': return Key::Num4;
        case '5': return Key::Num5;
        case '6': return Key::Num6;
        case '7': return Key::Num7;
        case '8': return Key::Num8;
        case '9': return Key::Num9;
        case VK_F1: return Key::F1;
        case VK_F2: return Key::F2;
        case VK_F3: return Key::F3;
        case VK_F4: return Key::F4;
        case VK_F5: return Key::F5;
        case VK_F6: return Key::F6;
        case VK_F7: return Key::F7;
        case VK_F8: return Key::F8;
        case VK_F9: return Key::F9;
        case VK_F10: return Key::F10;
        case VK_F11: return Key::F11;
        case VK_F12: return Key::F12;
        case VK_ESCAPE: return Key::Escape;
        case VK_TAB: return Key::Tab;
        case VK_CAPITAL: return Key::CapsLock;
        case VK_SHIFT: return Key::Shift;
        case VK_CONTROL: return Key::Control;
        case VK_MENU: return Key::Alt;
        case VK_SPACE: return Key::Space;
        case VK_RETURN: return Key::Enter;
        case VK_BACK: return Key::Backspace;
        case VK_INSERT: return Key::Insert;
        case VK_DELETE: return Key::Delete;
        case VK_HOME: return Key::Home;
        case VK_END: return Key::End;
        case VK_PRIOR: return Key::PageUp;
        case VK_NEXT: return Key::PageDown;
        case VK_LEFT: return Key::Left;
        case VK_RIGHT: return Key::Right;
        case VK_UP: return Key::Up;
        case VK_DOWN: return Key::Down;
        default: return Key::Unknown;
    }
}

LRESULT CALLBACK Platform::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    // Let ImGui handle messages first
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return true;
    }
    
    Platform* platform = s_instance;
    
    switch (msg) {
        case WM_CLOSE:
            if (platform) platform->m_should_close = true;
            return 0;
            
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
            
        case WM_SIZE:
            if (platform) {
                platform->m_width = LOWORD(lparam);
                platform->m_height = HIWORD(lparam);
            }
            return 0;
            
        case WM_SETFOCUS:
            if (platform) platform->m_focused = true;
            return 0;
            
        case WM_KILLFOCUS:
            if (platform) platform->m_focused = false;
            return 0;
            
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (platform) {
                Key key = MapWindowsKey(wparam);
                if (key != Key::Unknown) {
                    platform->m_input.OnKeyDown(key);
                }
            }
            return 0;
            
        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (platform) {
                Key key = MapWindowsKey(wparam);
                if (key != Key::Unknown) {
                    platform->m_input.OnKeyUp(key);
                }
            }
            return 0;
            
        case WM_LBUTTONDOWN:
            if (platform) platform->m_input.OnKeyDown(Key::MouseLeft);
            return 0;
        case WM_LBUTTONUP:
            if (platform) platform->m_input.OnKeyUp(Key::MouseLeft);
            return 0;
        case WM_RBUTTONDOWN:
            if (platform) platform->m_input.OnKeyDown(Key::MouseRight);
            return 0;
        case WM_RBUTTONUP:
            if (platform) platform->m_input.OnKeyUp(Key::MouseRight);
            return 0;
        case WM_MBUTTONDOWN:
            if (platform) platform->m_input.OnKeyDown(Key::MouseMiddle);
            return 0;
        case WM_MBUTTONUP:
            if (platform) platform->m_input.OnKeyUp(Key::MouseMiddle);
            return 0;
            
        case WM_MOUSEMOVE: {
            if (platform) {
                int x = GET_X_LPARAM(lparam);
                int y = GET_Y_LPARAM(lparam);
                
                if (platform->m_input.GetMouse().captured) {
                    // When captured, calculate delta from window center
                    RECT rect;
                    GetClientRect(hwnd, &rect);
                    int center_x = (rect.right - rect.left) / 2;
                    int center_y = (rect.bottom - rect.top) / 2;
                    
                    int dx = x - center_x;
                    int dy = y - center_y;
                    
                    if (dx != 0 || dy != 0) {
                        platform->m_input.OnMouseMove(x, y, dx, dy);
                        
                        // Recenter the cursor
                        POINT pt = {center_x, center_y};
                        ClientToScreen(hwnd, &pt);
                        SetCursorPos(pt.x, pt.y);
                    }
                } else {
                    static int last_x = 0, last_y = 0;
                    platform->m_input.OnMouseMove(x, y, x - last_x, y - last_y);
                    last_x = x;
                    last_y = y;
                }
            }
            return 0;
        }
            
        case WM_MOUSEWHEEL:
            if (platform) {
                float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / WHEEL_DELTA;
                platform->m_input.OnMouseScroll(delta);
            }
            return 0;
    }
    
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

bool Platform::Initialize(u32 width, u32 height, const std::string& title, bool fullscreen) {
    s_instance = this;
    m_width = width;
    m_height = height;
    m_fullscreen = fullscreen;
    
    m_hinstance = GetModuleHandle(nullptr);
    
    // Register window class
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = static_cast<HINSTANCE>(m_hinstance);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = "ActionEngineWindow";
    
    if (!RegisterClassEx(&wc)) {
        LOG_ERROR("Failed to register window class");
        return false;
    }
    
    // Calculate window size for desired client area
    DWORD style = fullscreen ? WS_POPUP : (WS_OVERLAPPEDWINDOW | WS_VISIBLE);
    RECT rect = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    AdjustWindowRect(&rect, style, FALSE);
    
    int window_width = rect.right - rect.left;
    int window_height = rect.bottom - rect.top;
    
    // Center window
    int screen_width = GetSystemMetrics(SM_CXSCREEN);
    int screen_height = GetSystemMetrics(SM_CYSCREEN);
    int x = (screen_width - window_width) / 2;
    int y = (screen_height - window_height) / 2;
    
    if (fullscreen) {
        x = y = 0;
        window_width = screen_width;
        window_height = screen_height;
        m_width = screen_width;
        m_height = screen_height;
    }
    
    m_hwnd = CreateWindowEx(
        0,
        "ActionEngineWindow",
        title.c_str(),
        style,
        x, y, window_width, window_height,
        nullptr, nullptr,
        static_cast<HINSTANCE>(m_hinstance),
        nullptr
    );
    
    if (!m_hwnd) {
        LOG_ERROR("Failed to create window");
        return false;
    }
    
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    
    LOG_INFO("Platform initialized: {}x{} {}", 
             m_width, m_height, fullscreen ? "(fullscreen)" : "(windowed)");
    
    return true;
}

void Platform::Shutdown() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    
    UnregisterClass("ActionEngineWindow", static_cast<HINSTANCE>(m_hinstance));
    s_instance = nullptr;
    
    LOG_INFO("Platform shutdown");
}

void Platform::PollEvents() {
    m_input.Update();
    
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            m_should_close = true;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void Platform::SetTitle(const std::string& title) {
    SetWindowText(m_hwnd, title.c_str());
}

void Platform::SetFullscreen(bool fullscreen) {
    if (m_fullscreen == fullscreen) return;
    
    m_fullscreen = fullscreen;
    
    if (fullscreen) {
        SetWindowLong(m_hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(m_hwnd, HWND_TOP, 0, 0, 
                     GetSystemMetrics(SM_CXSCREEN), 
                     GetSystemMetrics(SM_CYSCREEN),
                     SWP_FRAMECHANGED);
    } else {
        SetWindowLong(m_hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
        RECT rect = {0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height)};
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
        SetWindowPos(m_hwnd, HWND_NOTOPMOST,
                     100, 100,
                     rect.right - rect.left,
                     rect.bottom - rect.top,
                     SWP_FRAMECHANGED);
    }
}

double Platform::GetTime() const {
    static LARGE_INTEGER frequency;
    static LARGE_INTEGER start_time;
    static bool initialized = false;
    
    if (!initialized) {
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&start_time);
        initialized = true;
    }
    
    LARGE_INTEGER current_time;
    QueryPerformanceCounter(&current_time);
    
    return static_cast<double>(current_time.QuadPart - start_time.QuadPart) 
         / static_cast<double>(frequency.QuadPart);
}

void Platform::Sleep(u32 milliseconds) {
    ::Sleep(milliseconds);
}

#endif // PLATFORM_WINDOWS

} // namespace action
