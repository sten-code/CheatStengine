#include "MemoryViewerPane.h"

#include <Windows.h>

#include <cstdio>
#include <imgui.h>

MemoryViewerPane::MemoryViewerPane(State& state)
    : Pane("Memory Viewer", state)
{
    m_HexState.ReadCallback = [](ImGuiHexEditorState* state, size_t offset, void* buf, size_t size) -> size_t {
        SIZE_T read;
        ReadProcessMemory(GetCurrentProcess(), (char*)state->Bytes + offset, buf, size, &read);
        return read;
    };

    m_HexState.WriteCallback = [](ImGuiHexEditorState* state, size_t offset, void* buf, size_t size) -> size_t {
        SIZE_T write;
        WriteProcessMemory(GetCurrentProcess(), (char*)state->Bytes + offset, buf, size, &write);
        return write;
    };

    m_HexState.GetAddressNameCallback = [](ImGuiHexEditorState* state, size_t offset, char* buf, size_t size) -> bool {
        if (offset >= 0 && offset < sizeof(ImGuiIO)) {
            snprintf(buf, size, "%0.*zX", 4, offset);
            return true;
        }

        return false;
    };

    m_HexState.SingleHighlightCallback = [](ImGuiHexEditorState* state, size_t offset, ImColor* color, ImColor* text_color, ImColor* border_color) -> ImGuiHexEditorHighlightFlags {
        if (offset >= 100 && offset <= 150) {
            // *color = ImColor(user_highlight_color);
            return ImGuiHexEditorHighlightFlags_Apply | ImGuiHexEditorHighlightFlags_TextAutomaticContrast | ImGuiHexEditorHighlightFlags_Ascii
                | ImGuiHexEditorHighlightFlags_BorderAutomaticContrast;
        }

        return ImGuiHexEditorHighlightFlags_None;
    };

    m_HexState.HighlightRanges.clear();

    m_HexState.Bytes = 0;
    m_HexState.MaxBytes = 0x7FFFFFFFFFFF;
}

void MemoryViewerPane::Draw()
{
    ImGui::Begin("Memory Viewer", &m_Open);

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
        && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        m_HexState.SelectStartByte = -1;
        m_HexState.SelectStartSubByte = 0;
        m_HexState.SelectEndByte = -1;
        m_HexState.SelectEndSubByte = 0;
        m_HexState.LastSelectedByte = -1;
        m_HexState.SelectDragByte = -1;
        m_HexState.SelectDragSubByte = 0;
    }

    ImGui::BeginHexEditor("##HexEditor", &m_HexState);
    ImGui::EndHexEditor();

    ImGui::End();
}