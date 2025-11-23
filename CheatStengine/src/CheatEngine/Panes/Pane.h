#pragma once

#include <CheatEngine/Core/State.h>

#include <string>
#include <utility>

class Pane {
public:
    explicit Pane(std::string name, State& state, bool open = true)
        : m_State(state)
        , m_Name(std::move(name))
        , m_Open(open) {}
    virtual ~Pane() = default;

    virtual void Draw() = 0;

    void Open() { m_Open = true; }
    void Close() { m_Open = false; }
    void SetOpen(bool open) { m_Open = open; }

    [[nodiscard]] bool IsOpen() const { return m_Open; }
    [[nodiscard]] const std::string& GetName() const { return m_Name; }

protected:
    State& m_State;

    std::string m_Name;
    bool m_Open = true;
};