#pragma once

#include <CheatStengine/Core/ModalManager.h>
#include <CheatStengine/Panes/Pane.h>
#include <CheatStengine/Tools/MemoryScanner.h>

#include <string>

class WatchPane final : public Pane {
public:
    explicit WatchPane(State& state, ModalManager& modalManager);

    void Draw() override;

private:
    void HandleKeyboardShortcuts();
    void AddAddressModal(const std::string& name, const std::any& payload);

    void DeleteSelectedAddress();

private:
    ModalManager& m_ModalManager;

    size_t m_SelectedIndex = -1;

    std::string m_AddressInput;
    ValueType m_AddressType = ValueType::Int32;
};