#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace AK
{
    struct WindowDesc
    {
        std::string title = "AK Engine";
        std::uint32_t width = 1280;
        std::uint32_t height = 720;
    };

    struct WindowInput
    {
        std::int32_t mouseX = 0;
        std::int32_t mouseY = 0;
        bool mouseLeftPressed = false;
        bool mouseRightPressed = false;
        bool mouseMiddlePressed = false;
        bool mouseLeftDown = false;
        bool mouseRightDown = false;
        bool mouseMiddleDown = false;
        std::int32_t mouseWheelDelta = 0;
        bool keyUpPressed = false;
        bool keyDownPressed = false;
        bool keyLeftPressed = false;
        bool keyRightPressed = false;
        bool keyDeletePressed = false;
        bool keyEscapePressed = false;
        bool keyEnterPressed = false;
        bool keyBackspacePressed = false;
        bool keyHomePressed = false;
        bool keyF2Pressed = false;
        bool keyFPressed = false;
        bool keyGPressed = false;
        bool keyCtrlDPressed = false;
        bool keyCtrlZPressed = false;
        bool keyCtrlYPressed = false;
        bool keyNPressed = false;
        bool keySPressed = false;
        bool keyLPressed = false;
        bool keyRPressed = false;
        bool keyWPressed = false;
        bool keyAPressed = false;
        bool keyDPressed = false;
        bool keyQPressed = false;
        bool keyEPressed = false;
        bool keyIPressed = false;
        bool keyJPressed = false;
        bool keyKPressed = false;
        bool keyMinusPressed = false;
        bool keyEqualsPressed = false;
        bool keyCtrlDown = false;
        bool keyShiftDown = false;
        std::string textInput;
    };

    class Window final
    {
    public:
        explicit Window(const WindowDesc& desc);
        ~Window();

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        bool IsOpen() const;
        bool PollEvents();
        void RequestClose();
        void SetTitle(const std::string& title);
        void* NativeHandle() const;

        const WindowInput& Input() const;

        std::uint32_t Width() const;
        std::uint32_t Height() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}
