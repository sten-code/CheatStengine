#pragma once

#include <CheatEngine/Core/ModalManager.h>
#include <CheatEngine/Panes/Pane.h>
#include <CheatEngine/Tools/StructDissect.h>

#include <Engine/Core/Core.h>

struct AddElementPayload {
    uintptr_t Offset;
    Field& Field;
};

struct EditValuePayload {
    uintptr_t Address = 0;
    Field& Field;
    FieldValue CurrentValue;
};

class StructDissectPane final : public Pane {
public:
    explicit StructDissectPane(State& state, ModalManager& modalManager);

    void Draw() override;

    void AddDissection(const std::string& name, uintptr_t address);

private:
    static std::string FormatFieldValue(const FieldValue& fieldValue, Field& field);
    static FieldValue ParseFieldValue(const std::string& str, const Field& field);

    void DrawDissection(Dissection& dissection) const;
    bool DrawField(Field& field, uintptr_t baseAddress, size_t depth = 0) const;

    bool FieldContextMenu(
        const std::string& label, Field& field,
        uintptr_t address, const FieldValue& value) const;

    void AddDissectionModal(const std::string& name, const std::any& rawPayload);
    void AddElementModal(const std::string& name, const std::any& rawPayload);
    void EditValueModal(const std::string& name, const std::any& rawPayload);

private:
    ModalManager& m_ModalManager;
};
