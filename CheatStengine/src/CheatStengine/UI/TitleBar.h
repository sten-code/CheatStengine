#pragma once

#include <CheatStengine/Core/State.h>
#include <Engine/Core/Window.h>

class TitleBar {
public:
    static constexpr float TITLEBAR_HEIGHT = 35.0f;
    static constexpr float BUTTON_WIDTH = 45.0f;

    explicit TitleBar(Window& window, State& state)
        : m_Window(window)
        , m_State(state) {}

    bool Draw(); // Returns true if hovered

private:
    Window& m_Window;
    State& m_State;
};