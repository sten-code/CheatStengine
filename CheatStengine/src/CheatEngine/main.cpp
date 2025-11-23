#include <CheatEngine/Assembly/Assembler.h>
#include <CheatEngine/Icons/FontAwesome6.h>
#include <CheatEngine/MainLayer.h>
#include <CheatEngine/UI/ImGui/Fonts.h>

#include <Engine/Core/Application.h>
#include <Engine/Core/EntryPoint.h>
#include <Engine/Core/Log.h>

enum EditorCol_ {
    EditorCol_Primary1,
    EditorCol_Primary2,
    EditorCol_Primary3,
    EditorCol_Secondary1,
    EditorCol_Secondary2,
    EditorCol_Accent1,
    EditorCol_Accent2,
    EditorCol_Text1,
    EditorCol_Text2,
    EditorCol_Background1,
    EditorCol_Background2,
    EditorCol_Background3,
    EditorCol_Background4,
    EditorCol_Error,
    EditorCol_Warning,
    EditorCol_Success,
    EditorCol_COUNT
};

inline static ImVec4 RGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
    constexpr float inv255 = 1.0f / 255.0f;
    return ImVec4 { r * inv255, g * inv255, b * inv255, a * inv255 };
};

class CheatEngine final : public Application {
public:
    explicit CheatEngine()
        : Application(WindowProps { "CheatEngine", 1280, 720, false })
    {
        ImGuiIO& io = ImGui::GetIO();
        ImGuiStyle& style = ImGui::GetStyle();

        style.WindowRounding = 5.0f;
        style.PopupRounding = 7.0f;
        style.TabRounding = 3.0f;
        style.FrameRounding = 4.0f;
        style.GrabRounding = 2.0f;
        style.ScrollbarRounding = 2.0f;

        style.WindowPadding = ImVec2(5.0f, 5.0f);

        style.TabBarBorderSize = 1.0f;
        style.WindowBorderSize = 1.0f;
        style.DockingSeparatorSize = 1.0f;

        std::array<ImVec4, EditorCol_COUNT> m_ColorPallete;
        m_ColorPallete[EditorCol_Primary1] = RGBA(77, 77, 79);
        m_ColorPallete[EditorCol_Primary2] = RGBA(70, 70, 77);
        m_ColorPallete[EditorCol_Primary3] = RGBA(30, 30, 30);
        m_ColorPallete[EditorCol_Secondary1] = RGBA(20, 20, 20);
        m_ColorPallete[EditorCol_Secondary2] = RGBA(55, 55, 61);
        m_ColorPallete[EditorCol_Accent1] = RGBA(66, 150, 250);
        m_ColorPallete[EditorCol_Accent2] = RGBA(96, 115, 181);
        m_ColorPallete[EditorCol_Text1] = RGBA(255, 255, 255);
        m_ColorPallete[EditorCol_Text2] = RGBA(128, 128, 128);
        m_ColorPallete[EditorCol_Background1] = RGBA(37, 37, 38);
        m_ColorPallete[EditorCol_Background2] = RGBA(30, 30, 30);
        m_ColorPallete[EditorCol_Background3] = RGBA(51, 51, 51);
        m_ColorPallete[EditorCol_Background4] = RGBA(0, 0, 0);
        m_ColorPallete[EditorCol_Error] = RGBA(219, 72, 115); // Errors
        m_ColorPallete[EditorCol_Warning] = RGBA(213, 152, 87); // Warnings
        m_ColorPallete[EditorCol_Success] = RGBA(174, 243, 87); // Success

        // style.Colors[ImGuiCol_WindowBg] = m_ColorPallete[EditorCol_Background1];
        // style.Colors[ImGuiCol_PopupBg] = m_ColorPallete[EditorCol_Background2];
        // style.Colors[ImGuiCol_Border] = m_ColorPallete[EditorCol_Secondary2];
        // style.Colors[ImGuiCol_Header] = m_ColorPallete[EditorCol_Primary3];
        // style.Colors[ImGuiCol_HeaderHovered] = m_ColorPallete[EditorCol_Primary2];
        // style.Colors[ImGuiCol_HeaderActive] = m_ColorPallete[EditorCol_Secondary2];
        // style.Colors[ImGuiCol_Button] = m_ColorPallete[EditorCol_Primary3];
        // style.Colors[ImGuiCol_ButtonHovered] = m_ColorPallete[EditorCol_Primary1];
        // style.Colors[ImGuiCol_ButtonActive] = m_ColorPallete[EditorCol_Primary2];
        // style.Colors[ImGuiCol_CheckMark] = m_ColorPallete[EditorCol_Text1];
        // style.Colors[ImGuiCol_SliderGrab] = m_ColorPallete[EditorCol_Secondary2];
        // style.Colors[ImGuiCol_SliderGrabActive] = m_ColorPallete[EditorCol_Accent1];
        // style.Colors[ImGuiCol_FrameBg] = m_ColorPallete[EditorCol_Primary3];
        // style.Colors[ImGuiCol_FrameBgHovered] = m_ColorPallete[EditorCol_Primary1];
        // style.Colors[ImGuiCol_FrameBgActive] = m_ColorPallete[EditorCol_Primary2];
        // style.Colors[ImGuiCol_Tab] = m_ColorPallete[EditorCol_Background2];
        // style.Colors[ImGuiCol_TabHovered] = m_ColorPallete[EditorCol_Secondary2];
        // style.Colors[ImGuiCol_TabActive] = m_ColorPallete[EditorCol_Secondary2];
        // style.Colors[ImGuiCol_TabSelectedOverline] = m_ColorPallete[EditorCol_Accent1];
        // style.Colors[ImGuiCol_TabDimmedSelectedOverline] = m_ColorPallete[EditorCol_Primary1];
        // style.Colors[ImGuiCol_TabUnfocused] = m_ColorPallete[EditorCol_Secondary2];
        // style.Colors[ImGuiCol_TabUnfocusedActive] = m_ColorPallete[EditorCol_Secondary2];
        // style.Colors[ImGuiCol_TableRowBg] = m_ColorPallete[EditorCol_Background2];
        // style.Colors[ImGuiCol_TableRowBgAlt] = m_ColorPallete[EditorCol_Background1];
        // style.Colors[ImGuiCol_TitleBg] = m_ColorPallete[EditorCol_Background2];
        // style.Colors[ImGuiCol_TitleBgActive] = m_ColorPallete[EditorCol_Background2];
        // style.Colors[ImGuiCol_TitleBgCollapsed] = m_ColorPallete[EditorCol_Background2];
        // style.Colors[ImGuiCol_ScrollbarGrab] = m_ColorPallete[EditorCol_Secondary2];
        // style.Colors[ImGuiCol_ResizeGrip] = m_ColorPallete[EditorCol_Secondary2];
        // style.Colors[ImGuiCol_ResizeGripHovered] = m_ColorPallete[EditorCol_Secondary2];
        // style.Colors[ImGuiCol_ResizeGripActive] = m_ColorPallete[EditorCol_Secondary2];
        // style.Colors[ImGuiCol_Separator] = m_ColorPallete[EditorCol_Primary2];
        // style.Colors[ImGuiCol_SeparatorHovered] = m_ColorPallete[EditorCol_Secondary2];
        // style.Colors[ImGuiCol_SeparatorActive] = m_ColorPallete[EditorCol_Secondary2];
        // style.Colors[ImGuiCol_Text] = m_ColorPallete[EditorCol_Text1];
        // style.Colors[ImGuiCol_TextDisabled] = m_ColorPallete[EditorCol_Text2];
        // style.Colors[ImGuiCol_MenuBarBg] = m_ColorPallete[EditorCol_Secondary1];

        // style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
        // style.WindowMenuButtonPosition = ImGuiDir_None;

        Fonts::LoadFonts();

        INFO("Current Working Directory: {}", std::filesystem::current_path().string());

        m_LayerStack.PushLayer<MainLayer>(*m_Window);
    }
};

Application* CreateApplication()
{
    return new CheatEngine();
}

// int main(int argc, char* argv[])
// {
//     Logger::Init();
//
//     // Assembly::Assembler assembler;
//     // assembler.Assemble("mov rax, cs:[rbx + rcx*2 + 16]");
//     // const std::vector<zasm::Instruction>& instructions = assembler.GetInstructions();
//     // for (const zasm::Instruction& instruction : instructions) {
//     //     std::cout << Formatter::Format(instruction).Text << std::endl;
//     // }
//     //
//     // zasm::Program program(zasm::MachineMode::AMD64);
//     // zasm::x86::Assembler a(program);
//     // for (const zasm::Instruction& instruction : instructions) {
//     //     a.emit(instruction);
//     // }
//     //
//     // zasm::Serializer serializer;
//     // auto res = serializer.serialize(program, 0);
//     // auto code = serializer.getCode();
//     // auto codeSize = serializer.getCodeSize();
//     //
//     // zasm::Decoder decoder(program.getMode());
//     // size_t bytesDecoded = 0;
//     // while (bytesDecoded < codeSize) {
//     //     uintptr_t curAddress = bytesDecoded;
//     //     zasm::Decoder::Result res = decoder.decode(code + bytesDecoded, codeSize - bytesDecoded, curAddress);
//     //     if (!res) {
//     //         ERR("Decoding error at address 0x{:X}: {}", curAddress, res.error().getErrorMessage());
//     //         return EXIT_FAILURE;
//     //     }
//     //
//     //     const auto& instrDetail = res.value();
//     //     const auto instr = instrDetail.getInstruction();
//     //     FormattedInstruction fmt = Formatter::Format(instr);
//     //     INFO("0x{:X}: {}", curAddress, fmt.Text);
//     //     // INFO("0x{:X}: {}", curAddress, zasm::formatter::toString(&instr));
//     //
//     //     bytesDecoded += instrDetail.getLength();
//     // }
//
//     // const uint64_t baseAddr = 0x00007FF6BC738ED4;
//     // const std::array<uint8_t, 48> code = {
//     //     0x40, 0x53, // push rbx
//     //     0x45, 0x8B, 0x18, // mov r11d, dword ptr ds:[r8]
//     //     0x48, 0x8B, 0xDA, // mov rbx, rdx
//     //     0x41, 0x83, 0xE3, 0xF8, // and r11d, 0xFFFFFFF8
//     //     0x4C, 0x8B, 0xC9, // mov r9, rcx
//     //     0x41, 0xF6, 0x00, 0x04, // test byte ptr ds:[r8], 0x4
//     //     0x4C, 0x8B, 0xD1, // mov r10, rcx
//     //     0x74, 0x13, // je 0x00007FF6BC738EFF
//     // };
//     //
//     // zasm::Program program(zasm::MachineMode::AMD64);
//     // zasm::Decoder decoder(program.getMode());
//     //
//     // size_t bytesDecoded = 0;
//     // while (bytesDecoded < code.size()) {
//     //     const auto curAddress = baseAddr + bytesDecoded;
//     //     zasm::Decoder::Result res = decoder.decode(code.data() + bytesDecoded, code.size() - bytesDecoded, curAddress);
//     //     if (!res) {
//     //         ERR("Decoding error at address 0x{:X}: {}", curAddress, res.error().getErrorMessage());
//     //         return EXIT_FAILURE;
//     //     }
//     //
//     //     const auto& instrDetail = res.value();
//     //     const auto instr = instrDetail.getInstruction();
//     //     FormattedInstruction fmt = Formatter::Format(instr);
//     //     INFO("0x{:X}: {}", curAddress, fmt.Text);
//     //     // INFO("0x{:X}: {}", curAddress, zasm::formatter::toString(&instr));
//     //
//     //     bytesDecoded += instrDetail.getLength();
//     // }
//
//     INFO("Starting Application");
//     Application app;
//     app.GetLayerStack().PushLayer<MainLayer>();
//     app.Run();
//     return EXIT_SUCCESS;
// }
