#pragma once

#include <imgui.h>

enum ImGuiHexEditorHighlightFlags_ : int {
    ImGuiHexEditorHighlightFlags_None = 0,
    ImGuiHexEditorHighlightFlags_Apply = 1 << 0,
    ImGuiHexEditorHighlightFlags_TextAutomaticContrast = 1 << 1,
    ImGuiHexEditorHighlightFlags_FullSized = 1 << 2, // Highlight entire byte space including it's container, has no effect on ascii
    ImGuiHexEditorHighlightFlags_Ascii = 1 << 3, // Highlight ascii (doesn't affect single byte highlighting)
    ImGuiHexEditorHighlightFlags_Border = 1 << 4,
    ImGuiHexEditorHighlightFlags_OverrideBorderColor = 1 << 5,
    ImGuiHexEditorHighlightFlags_BorderAutomaticContrast = 1 << 6,
};

typedef int ImGuiHexEditorHighlightFlags; // -> enum ImGuiHexEditorHighlightFlags_ // Flags: for ImGuiHexEditor callbacks

struct ImGuiHexEditorHighlightRange {
    size_t From = 0;
    size_t To = 0;
    ImColor Color;
    ImColor BorderColor;
    ImGuiHexEditorHighlightFlags Flags = ImGuiHexEditorHighlightFlags_None;
};

enum ImGuiHexEditorClipboardFlags_ : int {
    ImGuiHexEditorClipboardFlags_None = 0,
    ImGuiHexEditorClipboardFlags_Multiline = 1 << 0, // Separate resulting hex editor lines with carriage return
};

typedef int ImGuiHexEditorClipboardFlags; // -> enum ImGuiHexEditorClipboardFlags_

struct ImGuiHexEditorState {
    void* Bytes;
    size_t MaxBytes;

    bool InfiniteScroll = false;

    size_t BytesPerLine = -1;
    bool ShowPrintable = false;
    bool LowercaseBytes = false;
    bool RenderZeroesDisabled = true;
    bool ShowAddress = true;
    size_t AddressChars = -1;
    bool ShowAscii = true;
    bool ReadOnly = false;
    size_t Separators = 8;
    void* UserData = nullptr;
    ImVector<ImGuiHexEditorHighlightRange> HighlightRanges;
    bool EnableClipboard = true;
    ImGuiHexEditorClipboardFlags ClipboardFlags = ImGuiHexEditorClipboardFlags_Multiline;

    size_t (*ReadCallback)(ImGuiHexEditorState* state, size_t offset, void* buf, size_t size) = nullptr;
    size_t (*WriteCallback)(ImGuiHexEditorState* state, size_t offset, void* buf, size_t size) = nullptr;
    bool (*GetAddressNameCallback)(ImGuiHexEditorState* state, size_t offset, char* buf, size_t size) = nullptr;
    ImGuiHexEditorHighlightFlags (*SingleHighlightCallback)(ImGuiHexEditorState* state, size_t offset, ImColor* color, ImColor* text_color, ImColor* border_color) = nullptr;
    void (*HighlightRangesCallback)(ImGuiHexEditorState* state, size_t display_start, size_t display_end) = nullptr;

    size_t SelectStartByte = -1;
    size_t SelectStartSubByte = 0;
    size_t SelectEndByte = -1;
    size_t SelectEndSubByte = 0;
    size_t LastSelectedByte = -1;
    size_t SelectDragByte = -1;
    size_t SelectDragSubByte = 0;
    float SelectCursorAnimationTime = 0.f;

    ImGuiHexEditorHighlightFlags SelectionHighlightFlags = ImGuiHexEditorHighlightFlags_FullSized | ImGuiHexEditorHighlightFlags_Ascii;
};

namespace ImGui {
    bool BeginHexEditor(const char* str_id, ImGuiHexEditorState* state, const ImVec2& size = { 0.f, 0.f }, ImGuiChildFlags child_flags = 0, ImGuiWindowFlags window_flags = 0);
    void EndHexEditor();

    // Helpers

    bool CalcHexEditorRowRange(size_t row_offset, size_t row_bytes_count, size_t range_min, size_t range_max, size_t* out_min, size_t* out_max);
}