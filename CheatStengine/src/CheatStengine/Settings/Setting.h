#pragma once

#include <string>

class Setting {
public:
    virtual ~Setting() = default;

    virtual void Draw() = 0;
    virtual void Restore() = 0;
    virtual void Apply() = 0;

    [[nodiscard]] virtual std::string GetName() const = 0;
    [[nodiscard]] virtual std::string GetDescription() const = 0;
    [[nodiscard]] virtual bool HasValueChanged() const = 0;
};
