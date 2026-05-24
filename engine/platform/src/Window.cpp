#include <AK/Platform/Window.hpp>

#include <AK/Core/Log.hpp>

#include <chrono>
#include <thread>
#include <utility>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>
    #include <windowsx.h>
#endif

namespace AK
{
#if defined(_WIN32)
    namespace
    {
        constexpr wchar_t WindowClassName[] = L"AKEngineWindowClass";
        bool gWindowClassRegistered = false;

        struct Win32WindowState
        {
            HWND hwnd = nullptr;
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            bool open = false;
            wchar_t pendingHighSurrogate = 0;
            WindowInput input{};
        };

        std::wstring ToWide(const std::string& value)
        {
            if (value.empty())
            {
                return {};
            }

            const int required = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
            if (required <= 0)
            {
                return L"AK Engine";
            }

            std::wstring result(static_cast<std::size_t>(required), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), required);
            result.pop_back();
            return result;
        }

        void AppendUtf8Codepoint(std::string& output, std::uint32_t cp)
        {
            if (cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu))
            {
                cp = 0xFFFDu;
            }

            if (cp <= 0x7Fu)
            {
                output.push_back(static_cast<char>(cp));
            }
            else if (cp <= 0x7FFu)
            {
                output.push_back(static_cast<char>(0xC0u | (cp >> 6u)));
                output.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
            }
            else if (cp <= 0xFFFFu)
            {
                output.push_back(static_cast<char>(0xE0u | (cp >> 12u)));
                output.push_back(static_cast<char>(0x80u | ((cp >> 6u) & 0x3Fu)));
                output.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
            }
            else
            {
                output.push_back(static_cast<char>(0xF0u | (cp >> 18u)));
                output.push_back(static_cast<char>(0x80u | ((cp >> 12u) & 0x3Fu)));
                output.push_back(static_cast<char>(0x80u | ((cp >> 6u) & 0x3Fu)));
                output.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
            }
        }

        void AppendUtf8FromWin32Char(Win32WindowState& state, WPARAM wparam)
        {
            const std::uint32_t unit = static_cast<std::uint32_t>(wparam);
            if (unit < 32u && unit != L'\t')
            {
                return;
            }

            if (unit >= 0xD800u && unit <= 0xDBFFu)
            {
                state.pendingHighSurrogate = static_cast<wchar_t>(unit);
                return;
            }

            if (unit >= 0xDC00u && unit <= 0xDFFFu)
            {
                if (state.pendingHighSurrogate != 0)
                {
                    const std::uint32_t high = static_cast<std::uint32_t>(state.pendingHighSurrogate);
                    const std::uint32_t cp = 0x10000u + (((high - 0xD800u) << 10u) | (unit - 0xDC00u));
                    AppendUtf8Codepoint(state.input.textInput, cp);
                    state.pendingHighSurrogate = 0;
                }
                else
                {
                    AppendUtf8Codepoint(state.input.textInput, 0xFFFDu);
                }
                return;
            }

            if (state.pendingHighSurrogate != 0)
            {
                AppendUtf8Codepoint(state.input.textInput, 0xFFFDu);
                state.pendingHighSurrogate = 0;
            }

            AppendUtf8Codepoint(state.input.textInput, unit);
        }

        void ResetTransientInput(WindowInput& input)
        {
            input.mouseLeftPressed = false;
            input.mouseRightPressed = false;
            input.mouseMiddlePressed = false;
            input.mouseWheelDelta = 0;
            input.keyUpPressed = false;
            input.keyDownPressed = false;
            input.keyLeftPressed = false;
            input.keyRightPressed = false;
            input.keyDeletePressed = false;
            input.keyEscapePressed = false;
            input.keyEnterPressed = false;
            input.keyBackspacePressed = false;
            input.keyHomePressed = false;
            input.keyF2Pressed = false;
            input.keyFPressed = false;
            input.keyGPressed = false;
            input.keyCtrlDPressed = false;
            input.keyCtrlZPressed = false;
            input.keyCtrlYPressed = false;
            input.keyNPressed = false;
            input.keySPressed = false;
            input.keyLPressed = false;
            input.keyRPressed = false;
            input.keyWPressed = false;
            input.keyAPressed = false;
            input.keyDPressed = false;
            input.keyQPressed = false;
            input.keyEPressed = false;
            input.keyIPressed = false;
            input.keyJPressed = false;
            input.keyKPressed = false;
            input.keyMinusPressed = false;
            input.keyEqualsPressed = false;
            input.textInput.clear();
        }

        Win32WindowState* GetState(HWND hwnd)
        {
            return reinterpret_cast<Win32WindowState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        bool IsKeyRepeat(LPARAM lparam)
        {
            return (lparam & (1L << 30)) != 0;
        }

        void UpdateModifierState(WindowInput& input)
        {
            input.keyCtrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            input.keyShiftDown = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        }

        void HandleKeyDown(Win32WindowState& state, WPARAM wparam, LPARAM lparam)
        {
            UpdateModifierState(state.input);
            const bool repeat = IsKeyRepeat(lparam);

            switch (wparam)
            {
                case VK_UP:
                    state.input.keyUpPressed = true;
                    break;
                case VK_DOWN:
                    state.input.keyDownPressed = true;
                    break;
                case VK_LEFT:
                    state.input.keyLeftPressed = true;
                    break;
                case VK_RIGHT:
                    state.input.keyRightPressed = true;
                    break;
                case VK_DELETE:
                    state.input.keyDeletePressed = !repeat;
                    break;
                case VK_ESCAPE:
                    state.input.keyEscapePressed = !repeat;
                    break;
                case VK_RETURN:
                    state.input.keyEnterPressed = !repeat;
                    break;
                case VK_BACK:
                    state.input.keyBackspacePressed = !repeat;
                    break;
                case VK_HOME:
                    state.input.keyHomePressed = !repeat;
                    break;
                case VK_F2:
                    state.input.keyF2Pressed = !repeat;
                    break;
                case VK_OEM_MINUS:
                case VK_SUBTRACT:
                    state.input.keyMinusPressed = true;
                    break;
                case VK_OEM_PLUS:
                case VK_ADD:
                    state.input.keyEqualsPressed = true;
                    break;
                case 'Y':
                    if (state.input.keyCtrlDown)
                    {
                        state.input.keyCtrlYPressed = !repeat;
                    }
                    break;
                case 'Z':
                    if (state.input.keyCtrlDown)
                    {
                        state.input.keyCtrlZPressed = !repeat;
                    }
                    break;
                case 'N':
                    state.input.keyNPressed = !repeat;
                    break;
                case 'S':
                    state.input.keySPressed = true;
                    break;
                case 'L':
                    state.input.keyLPressed = true;
                    break;
                case 'R':
                    state.input.keyRPressed = !repeat;
                    break;
                case 'W':
                    state.input.keyWPressed = true;
                    break;
                case 'A':
                    state.input.keyAPressed = true;
                    break;
                case 'D':
                    if (state.input.keyCtrlDown)
                    {
                        state.input.keyCtrlDPressed = !repeat;
                    }
                    else
                    {
                        state.input.keyDPressed = true;
                    }
                    break;
                case 'Q':
                    state.input.keyQPressed = true;
                    break;
                case 'E':
                    state.input.keyEPressed = true;
                    break;
                case 'F':
                    state.input.keyFPressed = !repeat;
                    break;
                case 'G':
                    state.input.keyGPressed = !repeat;
                    break;
                case 'I':
                    state.input.keyIPressed = true;
                    break;
                case 'J':
                    state.input.keyJPressed = true;
                    break;
                case 'K':
                    state.input.keyKPressed = true;
                    break;
                default:
                    break;
            }
        }

        LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
        {
            switch (message)
            {
                case WM_NCCREATE:
                {
                    const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
                    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
                    return TRUE;
                }
                case WM_MOUSEMOVE:
                {
                    if (Win32WindowState* state = GetState(hwnd))
                    {
                        state->input.mouseX = GET_X_LPARAM(lparam);
                        state->input.mouseY = GET_Y_LPARAM(lparam);
                    }
                    return 0;
                }
                case WM_LBUTTONDOWN:
                {
                    if (Win32WindowState* state = GetState(hwnd))
                    {
                        state->input.mouseX = GET_X_LPARAM(lparam);
                        state->input.mouseY = GET_Y_LPARAM(lparam);
                        state->input.mouseLeftPressed = true;
                        state->input.mouseLeftDown = true;
                        SetFocus(hwnd);
                        SetCapture(hwnd);
                    }
                    return 0;
                }
                case WM_LBUTTONUP:
                {
                    if (Win32WindowState* state = GetState(hwnd))
                    {
                        state->input.mouseX = GET_X_LPARAM(lparam);
                        state->input.mouseY = GET_Y_LPARAM(lparam);
                        state->input.mouseLeftDown = false;
                        if (!state->input.mouseRightDown && !state->input.mouseMiddleDown)
                        {
                            ReleaseCapture();
                        }
                    }
                    return 0;
                }
                case WM_RBUTTONDOWN:
                {
                    if (Win32WindowState* state = GetState(hwnd))
                    {
                        state->input.mouseX = GET_X_LPARAM(lparam);
                        state->input.mouseY = GET_Y_LPARAM(lparam);
                        state->input.mouseRightPressed = true;
                        state->input.mouseRightDown = true;
                        SetFocus(hwnd);
                        SetCapture(hwnd);
                    }
                    return 0;
                }
                case WM_RBUTTONUP:
                {
                    if (Win32WindowState* state = GetState(hwnd))
                    {
                        state->input.mouseX = GET_X_LPARAM(lparam);
                        state->input.mouseY = GET_Y_LPARAM(lparam);
                        state->input.mouseRightDown = false;
                        if (!state->input.mouseLeftDown && !state->input.mouseMiddleDown)
                        {
                            ReleaseCapture();
                        }
                    }
                    return 0;
                }
                case WM_MBUTTONDOWN:
                {
                    if (Win32WindowState* state = GetState(hwnd))
                    {
                        state->input.mouseX = GET_X_LPARAM(lparam);
                        state->input.mouseY = GET_Y_LPARAM(lparam);
                        state->input.mouseMiddlePressed = true;
                        state->input.mouseMiddleDown = true;
                        SetFocus(hwnd);
                        SetCapture(hwnd);
                    }
                    return 0;
                }
                case WM_MBUTTONUP:
                {
                    if (Win32WindowState* state = GetState(hwnd))
                    {
                        state->input.mouseX = GET_X_LPARAM(lparam);
                        state->input.mouseY = GET_Y_LPARAM(lparam);
                        state->input.mouseMiddleDown = false;
                        if (!state->input.mouseLeftDown && !state->input.mouseRightDown)
                        {
                            ReleaseCapture();
                        }
                    }
                    return 0;
                }
                case WM_MOUSEWHEEL:
                {
                    if (Win32WindowState* state = GetState(hwnd))
                    {
                        state->input.mouseWheelDelta += GET_WHEEL_DELTA_WPARAM(wparam);
                    }
                    return 0;
                }
                case WM_KEYDOWN:
                case WM_SYSKEYDOWN:
                {
                    if (Win32WindowState* state = GetState(hwnd))
                    {
                        HandleKeyDown(*state, wparam, lparam);
                    }
                    return 0;
                }
                case WM_CHAR:
                {
                    if (Win32WindowState* state = GetState(hwnd))
                    {
                        AppendUtf8FromWin32Char(*state, wparam);
                    }
                    return 0;
                }
                case WM_KEYUP:
                case WM_SYSKEYUP:
                {
                    if (Win32WindowState* state = GetState(hwnd))
                    {
                        UpdateModifierState(state->input);
                    }
                    return 0;
                }
                case WM_ERASEBKGND:
                    return 1;
                case WM_PAINT:
                {
                    PAINTSTRUCT paint{};
                    BeginPaint(hwnd, &paint);
                    EndPaint(hwnd, &paint);
                    return 0;
                }
                case WM_CLOSE:
                    DestroyWindow(hwnd);
                    return 0;
                case WM_DESTROY:
                {
                    if (Win32WindowState* state = GetState(hwnd))
                    {
                        state->open = false;
                    }
                    PostQuitMessage(0);
                    return 0;
                }
                default:
                    return DefWindowProcW(hwnd, message, wparam, lparam);
            }
        }

        void RegisterWindowClass()
        {
            if (gWindowClassRegistered)
            {
                return;
            }

            WNDCLASSEXW wc{};
            wc.cbSize = sizeof(WNDCLASSEXW);
            wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
            wc.lpfnWndProc = WindowProc;
            wc.hInstance = GetModuleHandleW(nullptr);
            wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
            wc.hbrBackground = nullptr;
            wc.lpszClassName = WindowClassName;

            if (!RegisterClassExW(&wc))
            {
                LogError("Failed to register Win32 window class");
                return;
            }

            gWindowClassRegistered = true;
        }
    }

    struct Window::Impl
    {
        Win32WindowState state{};
    };

    Window::Window(const WindowDesc& desc)
        : mImpl(std::make_unique<Impl>())
    {
        RegisterWindowClass();

        mImpl->state.width = desc.width;
        mImpl->state.height = desc.height;

        RECT rect{};
        rect.right = static_cast<LONG>(desc.width);
        rect.bottom = static_cast<LONG>(desc.height);
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

        const std::wstring title = ToWide(desc.title);

        mImpl->state.hwnd = CreateWindowExW(
            0,
            WindowClassName,
            title.c_str(),
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            rect.right - rect.left,
            rect.bottom - rect.top,
            nullptr,
            nullptr,
            GetModuleHandleW(nullptr),
            &mImpl->state);

        if (!mImpl->state.hwnd)
        {
            LogError("Failed to create Win32 window");
            return;
        }

        ShowWindow(mImpl->state.hwnd, SW_SHOW);
        UpdateWindow(mImpl->state.hwnd);
        mImpl->state.open = true;
    }

    Window::~Window()
    {
        if (mImpl && mImpl->state.hwnd)
        {
            DestroyWindow(mImpl->state.hwnd);
            mImpl->state.hwnd = nullptr;
        }
    }

    bool Window::IsOpen() const
    {
        return mImpl && mImpl->state.open;
    }

    bool Window::PollEvents()
    {
        if (mImpl)
        {
            ResetTransientInput(mImpl->state.input);
            UpdateModifierState(mImpl->state.input);
        }

        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                if (mImpl)
                {
                    mImpl->state.open = false;
                }
                return false;
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        return IsOpen();
    }

    void Window::RequestClose()
    {
        if (mImpl && mImpl->state.hwnd)
        {
            PostMessageW(mImpl->state.hwnd, WM_CLOSE, 0, 0);
        }
    }

    void Window::SetTitle(const std::string& title)
    {
        if (mImpl && mImpl->state.hwnd)
        {
            const std::wstring wide = ToWide(title);
            SetWindowTextW(mImpl->state.hwnd, wide.c_str());
        }
    }

    void* Window::NativeHandle() const
    {
        return mImpl ? mImpl->state.hwnd : nullptr;
    }
#else
    namespace
    {
        void ResetTransientInput(WindowInput& input)
        {
            input.mouseLeftPressed = false;
            input.mouseRightPressed = false;
            input.mouseMiddlePressed = false;
            input.mouseWheelDelta = 0;
            input.keyUpPressed = false;
            input.keyDownPressed = false;
            input.keyLeftPressed = false;
            input.keyRightPressed = false;
            input.keyDeletePressed = false;
            input.keyEscapePressed = false;
            input.keyEnterPressed = false;
            input.keyBackspacePressed = false;
            input.keyHomePressed = false;
            input.keyF2Pressed = false;
            input.keyFPressed = false;
            input.keyGPressed = false;
            input.keyCtrlDPressed = false;
            input.keyCtrlZPressed = false;
            input.keyCtrlYPressed = false;
            input.keyNPressed = false;
            input.keySPressed = false;
            input.keyLPressed = false;
            input.keyRPressed = false;
            input.keyWPressed = false;
            input.keyAPressed = false;
            input.keyDPressed = false;
            input.keyQPressed = false;
            input.keyEPressed = false;
            input.keyIPressed = false;
            input.keyJPressed = false;
            input.keyKPressed = false;
            input.keyMinusPressed = false;
            input.keyEqualsPressed = false;
            input.textInput.clear();
        }
    }

    struct Window::Impl
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        bool open = false;
        WindowInput input{};
    };

    Window::Window(const WindowDesc& desc)
        : mImpl(std::make_unique<Impl>())
    {
        mImpl->width = desc.width;
        mImpl->height = desc.height;
        mImpl->open = true;
        LogWarning("Window backend is stubbed on this platform");
    }

    Window::~Window() = default;

    bool Window::IsOpen() const
    {
        return mImpl && mImpl->open;
    }

    bool Window::PollEvents()
    {
        if (mImpl)
        {
            ResetTransientInput(mImpl->input);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        return IsOpen();
    }

    void Window::RequestClose()
    {
        if (mImpl)
        {
            mImpl->open = false;
        }
    }

    void Window::SetTitle(const std::string&)
    {
    }

    void* Window::NativeHandle() const
    {
        return nullptr;
    }
#endif

    const WindowInput& Window::Input() const
    {
        static const WindowInput empty{};
#if defined(_WIN32)
        return mImpl ? mImpl->state.input : empty;
#else
        return mImpl ? mImpl->input : empty;
#endif
    }

    std::uint32_t Window::Width() const
    {
#if defined(_WIN32)
        if (mImpl && mImpl->state.hwnd)
        {
            RECT rect{};
            GetClientRect(mImpl->state.hwnd, &rect);
            return static_cast<std::uint32_t>(rect.right - rect.left);
        }
        return mImpl ? mImpl->state.width : 0;
#else
        return mImpl ? mImpl->width : 0;
#endif
    }

    std::uint32_t Window::Height() const
    {
#if defined(_WIN32)
        if (mImpl && mImpl->state.hwnd)
        {
            RECT rect{};
            GetClientRect(mImpl->state.hwnd, &rect);
            return static_cast<std::uint32_t>(rect.bottom - rect.top);
        }
        return mImpl ? mImpl->state.height : 0;
#else
        return mImpl ? mImpl->height : 0;
#endif
    }
}
