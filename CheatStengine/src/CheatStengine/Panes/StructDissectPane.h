#pragma once

#include <CheatStengine/Core/KeybindManager.h>
#include <CheatStengine/Core/ModalManager.h>
#include <CheatStengine/Panes/Pane.h>
#include <CheatStengine/Tools/StructDissect.h>

#include <Engine/Core/Core.h>

class StructDissectPane final : public Pane {
public:
    explicit StructDissectPane(State& state, ModalManager& modalManager, KeybindManager& keybindManager);

    void Draw() override;

    void AddDissection(const std::string& name, uintptr_t address) const;

private:
    static std::string FormatFieldValue(const FieldValue& fieldValue, const Field& field);
    static FieldValue ParseFieldValue(const std::string& str, const Field& field);

    void HandleKeybinds();
    void DrawFindPopup(ImVec2 pos, float width, Dissection& dissection);
    void OpenFindPopup();

    void PerformSearch(Dissection& dissection);

    void DrawDissection(Dissection& dissection);
    void DrawField(Field& field, uintptr_t baseAddress, size_t depth = 0);

    bool FieldContextMenu(
        const std::string& label, Field& field,
        uintptr_t address, const FieldValue& value) const;

    void AddDissectionModal(const std::string& name, const std::any& rawPayload);
    void AddElementModal(const std::string& name, const std::any& rawPayload);
    void EditValueModal(const std::string& name, const std::any& rawPayload) const;
    void ChangeSizeModal(const std::string& name, const std::any& rawPayload) const;

private:
    ModalManager& m_ModalManager;
    KeybindManager& m_KeybindManager;
    bool m_FindPopupOpened = false;
    bool m_FocusOnFind = false;
    bool m_FocusOnSearchResult = false;

    size_t m_SearchResultIndex = -1;
    size_t m_SearchIndex = 0;
    size_t m_SearchDepth = 0;
    std::string m_SearchQuery;
};
