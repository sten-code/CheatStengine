#pragma once

#include "Setting.h"

#include <functional>
#include <utility>

class McpControls final : public Setting {
public:
    explicit McpControls(std::string name, std::function<void()> draw)
        : m_Name(std::move(name))
        , m_Draw(std::move(draw))
    {
    }

    void Draw() override
    {
        if (m_Draw) {
            m_Draw();
        }
    }

    void Restore() override { }
    void Apply() override { }

    [[nodiscard]] std::string GetName() const override { return m_Name; }
    [[nodiscard]] std::string GetDescription() const override { return {}; }
    [[nodiscard]] bool HasValueChanged() const override { return false; }

private:
    std::string m_Name;
    std::function<void()> m_Draw;
};
