#pragma once

#include "Setting.h"

#include <imgui.h>

#define DEFINE_ENUM_SETTING(EnumType, ...)                                                      \
    template <>                                                                                 \
    struct EnumTraits<EnumType> {                                                               \
        static const std::vector<std::pair<EnumType, const char*>>& GetItems()                  \
        {                                                                                       \
            static const std::vector<std::pair<EnumType, const char*>> items = { __VA_ARGS__ }; \
            return items;                                                                       \
        }                                                                                       \
        static const char* GetName(EnumType value)                                              \
        {                                                                                       \
            for (const auto& item : GetItems()) {                                               \
                if (item.first == value) {                                                      \
                    return item.second;                                                         \
                }                                                                               \
            }                                                                                   \
            return "Unknown";                                                                   \
        }                                                                                       \
        static EnumType GetValue(const char* name)                                              \
        {                                                                                       \
            for (const auto& item : GetItems()) {                                               \
                if (strcmp(item.second, name) == 0) {                                           \
                    return item.first;                                                          \
                }                                                                               \
            }                                                                                   \
            return GetItems().front().first;                                                    \
        }                                                                                       \
        static int GetIndex(EnumType value)                                                     \
        {                                                                                       \
            for (size_t i = 0; i < GetItems().size(); ++i) {                                    \
                if (GetItems()[i].first == value) {                                             \
                    return static_cast<int>(i);                                                 \
                }                                                                               \
            }                                                                                   \
            return 0;                                                                           \
        }                                                                                       \
        static EnumType GetValueFromIndex(int index)                                            \
        {                                                                                       \
            if (index >= 0 && index < static_cast<int>(GetItems().size())) {                    \
                return GetItems()[index].first;                                                 \
            }                                                                                   \
            return GetItems().front().first;                                                    \
        }                                                                                       \
    };

template <typename T>
struct EnumTraits;

template <typename EnumType>
class EnumSetting final : public Setting {
public:
    EnumSetting(std::string name, std::string description, EnumType defaultValue)
        : m_Name(std::move(name))
        , m_Description(std::move(description))
        , m_Value(defaultValue)
        , m_TempValue(defaultValue)
    {
        static_assert(std::is_enum_v<EnumType>, "EnumSetting requires an enum type");
    }

    void Draw() override
    {
        ImGui::PushID(m_Name.c_str());
        ImGui::BeginGroup();

        const auto& items = EnumTraits<EnumType>::GetItems();
        int currentIndex = EnumTraits<EnumType>::GetIndex(m_TempValue);

        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.4f);
        if (ImGui::BeginCombo(m_Name.c_str(), items[currentIndex].second)) {
            for (size_t i = 0; i < items.size(); ++i) {
                bool isSelected = (currentIndex == static_cast<int>(i));
                if (ImGui::Selectable(items[i].second, isSelected)) {
                    m_TempValue = items[i].first;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::IsItemHovered() && !m_Description.empty()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(m_Description.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        ImGui::EndGroup();
        ImGui::PopID();
    }

    void Restore() override { m_TempValue = m_Value; }
    void Apply() override { m_Value = m_TempValue; }

    [[nodiscard]] EnumType GetValue() const { return m_Value; }
    [[nodiscard]] const char* GetCurrentItemName() const { return EnumTraits<EnumType>::GetName(m_Value); }

    [[nodiscard]] std::string GetName() const override { return m_Name; }
    [[nodiscard]] std::string GetDescription() const override { return m_Description; }
    [[nodiscard]] bool HasValueChanged() const override { return m_Value != m_TempValue; }

    void SetValue(EnumType value)
    {
        m_Value = value;
        m_TempValue = value;
    }

private:
    std::string m_Name;
    std::string m_Description;
    EnumType m_Value;
    EnumType m_TempValue;
};