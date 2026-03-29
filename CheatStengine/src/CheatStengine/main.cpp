#include <CheatStengine/Assembly/Assembler.h>
#include <CheatStengine/Assembly/Formatter.h>
#include <CheatStengine/MainLayer.h>
#include <CheatStengine/Process/Process.h>
#include <CheatStengine/UI/ImGui/Fonts.h>
#include <Engine/Core/Application.h>
#include <Engine/Core/EntryPoint.h>
#include <Engine/Core/Log.h>

class CheatStengine final : public Application {
public:
    explicit CheatStengine()
        : Application(WindowProps { "Cheat Stengine", 1280, 720, false })
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

        Fonts::LoadFonts();

        INFO("Current Working Directory: {}", std::filesystem::current_path().string());

        m_LayerStack.PushLayer<MainLayer>(*m_Window);
    }
};

Application* CreateApplication()
{
    // std::unique_ptr<Process> proc = Process::Create("Notepad.exe", ProcessMode::Kernel);

    // std::optional<uint32_t> oldProtect = proc->Protect(0x24A299A0000, 0x1000, PAGE_EXECUTE_READWRITE);
    // if (oldProtect) {
    //     INFO("Old protection: 0x{:X}", *oldProtect);
    // } else {
    //     ERR("Failed to change protection");
    // }

    // uintptr_t allocatedMemory = proc->Allocate(0x1000, PAGE_EXECUTE_READWRITE);
    // INFO("Allocated memory at 0x{:X}", allocatedMemory);

    // proc->Free(0x1B97EBA0000);

    // std::optional<MEMORY_BASIC_INFORMATION> mbi = proc->Query(0x7FF705CD1132);
    // if (mbi) {
    //     INFO("Memory at 0x7FF77D7C1132: BaseAddress=0x{:X}, RegionSize=0x{:X}, State=0x{:X}, Protect=0x{:X}, Type=0x{:X}",
    //         reinterpret_cast<uintptr_t>(mbi->BaseAddress), mbi->RegionSize, mbi->State, mbi->Protect, mbi->Type);
    // }

    // proc->Write<uintptr_t>(0x7FF705CD1132, 0xCCCCCCCC69CCCCCC);
    //
    // uintptr_t value = proc->Read<uintptr_t>(0x7FF705CD1132);
    // INFO("Value at 0x7FF705CD1132: 0x{:X}", value);

    return new CheatStengine();
}

// int main(int argc, char* argv[])
// {
//     Logger::Init();
// zasm::Program program(zasm::MachineMode::AMD64);
// zasm::Decoder decoder(program.getMode());
//
// std::vector<uint8_t> code {
//     // 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, 0xD0, 0xF1, 0xB3, 0x5F, 0xFD, 0x7F, 0x00, 0x00,
//     0x48, 0x8B, 0x05, 0xF9, 0xFF, 0xFF, 0xFF, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC
//
// };
//
// size_t bytesDecoded = 0;
// while (bytesDecoded < code.size()) {
//     uintptr_t curAddress = bytesDecoded;
//     zasm::Decoder::Result res = decoder.decode(code.data() + bytesDecoded, code.size() - bytesDecoded, curAddress);
//     if (!res) {
//         ERR("Decoding error at address 0x{:X}: {}", curAddress, res.error().getErrorMessage());
//         break;
//     }
//
//     zasm::InstructionDetail& instrDetail = res.value();
//     zasm::Instruction instr = instrDetail.getInstruction();
//
//     zasm::Mem* mem0 = instr.getOperand(0).getIf<zasm::Mem>();
//     zasm::Mem* mem1 = instr.getOperand(1).getIf<zasm::Mem>();
//     if (mem0 && static_cast<ZydisRegister>(mem0->getBase().getId()) == ZYDIS_REGISTER_RIP && mem0->getDisplacement() == instrDetail.getLength()) {
//         uint64_t targetAddr = *reinterpret_cast<uint64_t*>(code.data() + bytesDecoded + instrDetail.getLength());
//         instr.setOperand(0, zasm::Imm { targetAddr });
//         bytesDecoded += 8;
//     } else if (mem1 && static_cast<ZydisRegister>(mem1->getBase().getId()) == ZYDIS_REGISTER_RIP && mem1->getDisplacement() == instrDetail.getLength()) {
//         uint64_t targetAddr = *reinterpret_cast<uint64_t*>(code.data() + bytesDecoded + instrDetail.getLength());
//         instr.setOperand(1, zasm::Imm { targetAddr });
//         bytesDecoded += 8;
//     }
//     FormattedInstruction fmt = Formatter::Format(instr);
//     INFO("0x{:X}: {}", curAddress, fmt.Text);
//     // INFO("0x{:X}: {}", curAddress, zasm::formatter::toString(&instr));
//
//     bytesDecoded += instrDetail.getLength();
// }
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
