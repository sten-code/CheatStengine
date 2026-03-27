#include "PEViewer.h"

#include <CheatStengine/UI/ImGui/Fonts.h>
#include <IconsMaterialDesignIcons.h>
#include <imgui.h>
#include <imgui_stdlib.h>

#define min(a, b) ((a) < (b) ? (a) : (b))

PEViewer::PEViewer(State& state)
    : Pane(ICON_MDI_FILE_DOCUMENT " PE Viewer", state)
{
}

void PEViewer::Draw(double deltaTime)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2 { 0.0f, 0.0f });
    ImGui::Begin(m_Name.c_str(), &m_Open);
    ImGui::PopStyleVar();

    if (ImGui::BeginTable("AddressWatchTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_ScrollY)) {
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        DrawModuleList();

        ImGui::TableSetColumnIndex(1);
        DrawTabBar();

        ImGui::EndTable();
    }

    ImGui::End();
}

void PEViewer::DrawModuleList()
{
    ImGui::PushItemWidth(-1);
    ImGui::InputTextWithHint("##Search", "Search modules...", &m_SearchQuery);
    ImGui::PopItemWidth();

    ImGui::BeginChild("PEViewerModuleList");

    std::string queryLower = m_SearchQuery;
    std::ranges::transform(queryLower, queryLower.begin(), ::tolower);

    for (size_t i = 0; i < m_State.Modules.size(); ++i) {
        const MODULEENTRY32& entry = m_State.Modules[i];
        std::string moduleNameLower = entry.szModule;
        std::ranges::transform(moduleNameLower, moduleNameLower.begin(), ::tolower);

        // Skip modules that don't match the search query
        if (!queryLower.empty() && moduleNameLower.find(queryLower) == std::string::npos) {
            continue;
        }

        bool isSelected = (m_SelectedIndex == i);
        if (ImGui::Selectable(entry.szModule, isSelected)) {
            m_SelectedIndex = i;
        }
    }

    ImGui::EndChild();
}

void PEViewer::DrawTabBar()
{
    if (m_SelectedIndex == -1) {
        ImGui::BeginChild("PEViewerEmptyState", ImVec2(0, 0), true);
        ImGui::Text("Select a module to view its PE headers.");
        ImGui::EndChild();
        return;
    }

    if (ImGui::BeginTabBar("PEViewerTabBar")) {

        if (ImGui::BeginTabItem("Headers")) {
            DrawHeadersTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Data Directories")) {
            DrawDataDirectoriesTab();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

static void DrawHeaderField(const char* fieldName, const std::function<void()>& valueRenderer)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("%s", fieldName);
    ImGui::TableSetColumnIndex(1);
    valueRenderer();
}

template <typename T>
static void DrawOptionalHeader(const T& optionalHeader)
{
    static_assert(std::is_same_v<T, IMAGE_OPTIONAL_HEADER32> || std::is_same_v<T, IMAGE_OPTIONAL_HEADER64>,
        "T must be IMAGE_OPTIONAL_HEADER32 or IMAGE_OPTIONAL_HEADER64");
    constexpr bool is32 = std::is_same_v<T, IMAGE_OPTIONAL_HEADER32>;

    if (ImGui::BeginTable("PEViewerOptionalHeader", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH)) {
        ImGui::TableSetupColumn("Field");
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();

        DrawHeaderField("Magic", [&optionalHeader]() {
            WORD magic = optionalHeader.Magic;
            const char* name = "Unknown";
            switch (magic) {
                case IMAGE_NT_OPTIONAL_HDR32_MAGIC: name = "PE32"; break;
                case IMAGE_NT_OPTIONAL_HDR64_MAGIC: name = "PE32+"; break;
                default: break;
            }
            ImGui::Text("0x%04X (%s)", magic, name);
        });
        DrawHeaderField("MajorLinkerVersion", [&optionalHeader]() {
            ImGui::Text("0x%X", optionalHeader.MajorLinkerVersion);
        });
        DrawHeaderField("MinorLinkerVersion", [&optionalHeader]() {
            ImGui::Text("0x%X", optionalHeader.MinorLinkerVersion);
        });
        DrawHeaderField("SizeOfCode", [&optionalHeader]() {
            ImGui::Text("0x%X", optionalHeader.SizeOfCode);
        });
        DrawHeaderField("SizeOfInitializedData", [&optionalHeader]() {
            ImGui::Text("0x%X", optionalHeader.SizeOfInitializedData);
        });
        DrawHeaderField("SizeOfUninitializedData", [&optionalHeader]() {
            ImGui::Text("0x%X", optionalHeader.SizeOfUninitializedData);
        });
        DrawHeaderField("AddressOfEntryPoint", [&optionalHeader]() {
            ImGui::Text("0x%X", optionalHeader.AddressOfEntryPoint);
        });
        DrawHeaderField("BaseOfCode", [&optionalHeader]() {
            ImGui::Text("0x%X", optionalHeader.BaseOfCode);
        });
        if constexpr (is32) {
            DrawHeaderField("BaseOfData", [&optionalHeader]() {
                DWORD baseOfData = optionalHeader.BaseOfData;
                ImGui::Text("0x%X", baseOfData);
            });
        }
        DrawHeaderField("ImageBase", [&optionalHeader]() {
            if constexpr (is32) {
                DWORD imageBase = optionalHeader.ImageBase;
                ImGui::Text("0x%X", imageBase);
            } else {
                ULONGLONG imageBase = optionalHeader.ImageBase;
                ImGui::Text("0x%llX", imageBase);
            }
        });
        DrawHeaderField("SectionAlignment", [&optionalHeader]() {
            ImGui::Text("0x%X", optionalHeader.SectionAlignment);
        });
        DrawHeaderField("FileAlignment", [&optionalHeader]() {
            ImGui::Text("0x%X", optionalHeader.FileAlignment);
        });
        DrawHeaderField("MajorOperatingSystemVersion", [&optionalHeader]() {
            ImGui::Text("0x%X", optionalHeader.MajorOperatingSystemVersion);
        });
        DrawHeaderField("MinorOperatingSystemVersion", [&optionalHeader]() {
            ImGui::Text("0x%X", optionalHeader.MinorOperatingSystemVersion);
        });
        DrawHeaderField("MajorImageVersion", [&optionalHeader]() {
            ImGui::Text("0x%X", optionalHeader.MajorImageVersion);
        });
        DrawHeaderField("MinorImageVersion", [&optionalHeader]() {
            ImGui::Text("0x%X", optionalHeader.MinorImageVersion);
        });
        DrawHeaderField("MajorSubsystemVersion", [&optionalHeader]() {
            ImGui::Text("0x%X", optionalHeader.MajorSubsystemVersion);
        });
        DrawHeaderField("MinorSubsystemVersion", [&optionalHeader]() {
            ImGui::Text("0x%X", optionalHeader.MinorSubsystemVersion);
        });
        DrawHeaderField("Win32VersionValue", [&optionalHeader]() {
            ImGui::Text("0x%X", optionalHeader.Win32VersionValue);
        });
        DrawHeaderField("SizeOfImage", [&optionalHeader]() {
            ImGui::Text("0x%X", optionalHeader.SizeOfImage);
        });
        DrawHeaderField("SizeOfHeaders", [&optionalHeader]() {
            ImGui::Text("0x%X", optionalHeader.SizeOfHeaders);
        });
        DrawHeaderField("CheckSum", [&optionalHeader]() {
            ImGui::Text("0x%X", optionalHeader.CheckSum);
        });
        DrawHeaderField("Subsystem", [&optionalHeader]() {
            WORD subsystem = optionalHeader.Subsystem;
            const char* name = "Unknown";
            switch (subsystem) {
                case IMAGE_SUBSYSTEM_UNKNOWN: name = "Unknown"; break;
                case IMAGE_SUBSYSTEM_NATIVE: name = "Native"; break;
                case IMAGE_SUBSYSTEM_WINDOWS_GUI: name = "Windows GUI"; break;
                case IMAGE_SUBSYSTEM_WINDOWS_CUI: name = "Windows CUI"; break;
                case IMAGE_SUBSYSTEM_OS2_CUI: name = "OS/2 CUI"; break;
                case IMAGE_SUBSYSTEM_POSIX_CUI: name = "POSIX CUI"; break;
                case IMAGE_SUBSYSTEM_NATIVE_WINDOWS: name = "Native Windows"; break;
                case IMAGE_SUBSYSTEM_WINDOWS_CE_GUI: name = "Windows CE GUI"; break;
                case IMAGE_SUBSYSTEM_EFI_APPLICATION: name = "EFI Application"; break;
                case IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER: name = "EFI Boot Service Driver"; break;
                case IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER: name = "EFI Runtime Driver"; break;
                case IMAGE_SUBSYSTEM_EFI_ROM: name = "EFI ROM"; break;
                case IMAGE_SUBSYSTEM_XBOX: name = "Xbox"; break;
                case IMAGE_SUBSYSTEM_WINDOWS_BOOT_APPLICATION: name = "Windows Boot Application"; break;
                default: break;
            }
            ImGui::Text("0x%04X (%s)", subsystem, name);
        });
        DrawHeaderField("DllCharacteristics", [&optionalHeader]() {
            WORD ch = optionalHeader.DllCharacteristics;
            std::vector<const char*> names;
            if (ch & IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE)
                names.push_back("DYNAMIC_BASE");
            if (ch & IMAGE_DLLCHARACTERISTICS_FORCE_INTEGRITY)
                names.push_back("FORCE_INTEGRITY");
            if (ch & IMAGE_DLLCHARACTERISTICS_NX_COMPAT)
                names.push_back("NX_COMPAT");
            if (ch & IMAGE_DLLCHARACTERISTICS_NO_ISOLATION)
                names.push_back("NO_ISOLATION");
            if (ch & IMAGE_DLLCHARACTERISTICS_NO_SEH)
                names.push_back("NO_SEH");
            if (ch & IMAGE_DLLCHARACTERISTICS_NO_BIND)
                names.push_back("NO_BIND");
            if (ch & IMAGE_DLLCHARACTERISTICS_APPCONTAINER)
                names.push_back("APPCONTAINER");
            if (ch & IMAGE_DLLCHARACTERISTICS_WDM_DRIVER)
                names.push_back("WDM_DRIVER");
            if (ch & IMAGE_DLLCHARACTERISTICS_GUARD_CF)
                names.push_back("GUARD_CF");
            if (ch & IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE)
                names.push_back("TERMINAL_SERVER_AWARE");

            std::string out;
            if (names.empty()) {
                out = "None";
            } else {
                for (size_t i = 0; i < names.size(); ++i) {
                    if (i)
                        out += ", ";
                    out += names[i];
                }
            }

            ImGui::Text("0x%04X (%s)", ch, out.c_str());
        });
        DrawHeaderField("SizeOfStackReserve", [&optionalHeader]() {
            ImGui::Text("0x%X", optionalHeader.SizeOfStackReserve);
        });
        DrawHeaderField("SizeOfStackCommit", [&optionalHeader]() {
            ImGui::Text("0x%X", optionalHeader.SizeOfStackCommit);
        });
        DrawHeaderField("SizeOfHeapReserve", [&optionalHeader]() {
            ImGui::Text("0x%X", optionalHeader.SizeOfHeapReserve);
        });
        DrawHeaderField("SizeOfHeapCommit", [&optionalHeader]() {
            ImGui::Text("0x%X", optionalHeader.SizeOfHeapCommit);
        });
        DrawHeaderField("LoaderFlags", [&optionalHeader]() {
            ImGui::Text("0x%X", optionalHeader.LoaderFlags);
        });
        DrawHeaderField("NumberOfRvaAndSizes", [&optionalHeader]() {
            ImGui::Text("0x%X", optionalHeader.NumberOfRvaAndSizes);
        });

        ImGui::EndTable();
    }
}

void PEViewer::DrawHeadersTab()
{
    const MODULEENTRY32& entry = m_State.Modules[m_SelectedIndex];
    IMAGE_DOS_HEADER dosHeader = m_State.Process.Read<IMAGE_DOS_HEADER>(reinterpret_cast<uintptr_t>(entry.modBaseAddr));
    uintptr_t ntAddr = reinterpret_cast<uintptr_t>(entry.modBaseAddr) + dosHeader.e_lfanew;

    DWORD ntSignature = m_State.Process.Read<DWORD>(ntAddr);
    WORD optionalMagic = m_State.Process.Read<WORD>(ntAddr + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER));

    bool is32 = (optionalMagic == IMAGE_NT_OPTIONAL_HDR32_MAGIC);
    IMAGE_NT_HEADERS32 ntHeaders32 {};
    IMAGE_NT_HEADERS64 ntHeaders64 {};
    if (is32) {
        ntHeaders32 = m_State.Process.Read<IMAGE_NT_HEADERS32>(ntAddr);
    } else {
        ntHeaders64 = m_State.Process.Read<IMAGE_NT_HEADERS64>(ntAddr);
    }
    IMAGE_FILE_HEADER& fileHeader = is32 ? ntHeaders32.FileHeader : ntHeaders64.FileHeader;

    ImGui::BeginChild("PEViewerHeadersTab", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::Text("DOS Header (IMAGE_DOS_HEADER)");
    if (ImGui::BeginTable("PEViewerDosHeaders", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH)) {
        ImGui::TableSetupColumn("Field");
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();

        DrawHeaderField("e_magic", [&dosHeader]() {
            if (dosHeader.e_magic == IMAGE_DOS_SIGNATURE) {
                ImGui::Text("0x%04X", dosHeader.e_magic);
            } else {
                ImGui::Text("0x%04X (Invalid Signature)", dosHeader.e_magic);
            }
        });
        DrawHeaderField("e_cblp", [&dosHeader]() {
            ImGui::Text("0x%04X", dosHeader.e_cblp);
        });
        DrawHeaderField("e_cp", [&dosHeader]() {
            ImGui::Text("0x%X", dosHeader.e_cp);
        });
        DrawHeaderField("e_crlc", [&dosHeader]() {
            ImGui::Text("0x%X", dosHeader.e_crlc);
        });
        DrawHeaderField("e_cparhdr", [&dosHeader]() {
            ImGui::Text("0x%X", dosHeader.e_cparhdr);
        });
        DrawHeaderField("e_minalloc", [&dosHeader]() {
            ImGui::Text("0x%X", dosHeader.e_minalloc);
        });
        DrawHeaderField("e_maxalloc", [&dosHeader]() {
            ImGui::Text("0x%X", dosHeader.e_maxalloc);
        });
        DrawHeaderField("e_ss", [&dosHeader]() {
            ImGui::Text("0x%04X", dosHeader.e_ss);
        });
        DrawHeaderField("e_sp", [&dosHeader]() {
            ImGui::Text("0x%04X", dosHeader.e_sp);
        });
        DrawHeaderField("e_csum", [&dosHeader]() {
            ImGui::Text("0x%04X", dosHeader.e_csum);
        });
        DrawHeaderField("e_ip", [&dosHeader]() {
            ImGui::Text("0x%04X", dosHeader.e_ip);
        });
        DrawHeaderField("e_cs", [&dosHeader]() {
            ImGui::Text("0x%X", dosHeader.e_cs);
        });
        DrawHeaderField("e_lfarlc", [&dosHeader]() {
            ImGui::Text("0x%04X", dosHeader.e_lfarlc);
        });
        DrawHeaderField("e_ovno", [&dosHeader]() {
            ImGui::Text("0x%X", dosHeader.e_ovno);
        });
        DrawHeaderField("e_res", [&dosHeader]() {
            ImGui::Text("0x%02X 0x%02X 0x%02X 0x%02X", dosHeader.e_res[0], dosHeader.e_res[1], dosHeader.e_res[2], dosHeader.e_res[3]);
        });
        DrawHeaderField("e_oemid", [&dosHeader]() {
            ImGui::Text("0x%04X", dosHeader.e_oemid);
        });
        DrawHeaderField("e_oeminfo", [&dosHeader]() {
            ImGui::Text("0x%04X", dosHeader.e_oeminfo);
        });
        DrawHeaderField("e_res2", [&dosHeader]() {
            ImGui::Text("0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
                dosHeader.e_res2[0], dosHeader.e_res2[1], dosHeader.e_res2[2], dosHeader.e_res2[3], dosHeader.e_res2[4],
                dosHeader.e_res2[5], dosHeader.e_res2[6], dosHeader.e_res2[7], dosHeader.e_res2[8], dosHeader.e_res2[9]);
        });
        DrawHeaderField("e_lfanew", [&dosHeader]() {
            ImGui::Text("0x%X", dosHeader.e_lfanew);
        });

        ImGui::EndTable();
    }

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);

    ImGui::Text("NT Headers (IMAGE_NT_HEADERS)");
    if (ImGui::BeginTable("PEViewerNtHeaders", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH)) {
        ImGui::TableSetupColumn("Field");
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();

        DrawHeaderField("Signature", [&ntSignature]() {
            if (ntSignature == IMAGE_NT_SIGNATURE) {
                ImGui::Text("0x%08X", ntSignature);
            } else {
                ImGui::Text("0x%08X (Invalid Signature)", ntSignature);
            }
        });

        ImGui::EndTable();
    }

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);

    ImGui::Text("File Header (IMAGE_FILE_HEADER)");
    if (ImGui::BeginTable("PEViewerFileHeader", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH)) {
        ImGui::TableSetupColumn("Field");
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();

        DrawHeaderField("Machine", [&fileHeader]() {
            WORD machine = fileHeader.Machine;
            const char* name = "Unknown";
            switch (machine) {
                case IMAGE_FILE_MACHINE_I386: name = "I386"; break;
                case IMAGE_FILE_MACHINE_AMD64: name = "AMD64"; break;
                case IMAGE_FILE_MACHINE_ARMNT: name = "ARMNT"; break;
                case IMAGE_FILE_MACHINE_ARM64: name = "ARM64"; break;
                case IMAGE_FILE_MACHINE_ARM: name = "ARM"; break;
                case IMAGE_FILE_MACHINE_IA64: name = "IA-64"; break;
                default: break;
            }
            ImGui::Text("0x%04X (%s)", machine, name);
        });
        DrawHeaderField("NumberOfSections", [&fileHeader]() {
            ImGui::Text("0x%X", fileHeader.NumberOfSections);
        });
        DrawHeaderField("TimeDateStamp", [&fileHeader]() {
            DWORD time = fileHeader.TimeDateStamp;
            std::time_t timeT = time;
            std::tm tmStruct {};
            if (localtime_s(&tmStruct, &timeT) == 0) {
                char timeStr[100];
                if (std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tmStruct) != 0) {
                    ImGui::Text("0x%X (%s)", time, timeStr);
                } else {
                    ImGui::Text("0x%X (Invalid time)", time);
                }
            } else {
                ImGui::Text("0x%X", time);
            }
        });
        DrawHeaderField("PointerToSymbolTable", [&fileHeader]() {
            ImGui::Text("0x%X", fileHeader.PointerToSymbolTable);
        });
        DrawHeaderField("NumberOfSymbols", [&fileHeader]() {
            ImGui::Text("0x%X", fileHeader.NumberOfSymbols);
        });
        DrawHeaderField("SizeOfOptionalHeader", [&fileHeader]() {
            ImGui::Text("0x%X", fileHeader.SizeOfOptionalHeader);
        });
        DrawHeaderField("Characteristics", [&fileHeader]() {
            WORD ch = fileHeader.Characteristics;
            std::vector<const char*> names;
            if (ch & IMAGE_FILE_RELOCS_STRIPPED)
                names.push_back("RELOCS_STRIPPED");
            if (ch & IMAGE_FILE_EXECUTABLE_IMAGE)
                names.push_back("EXECUTABLE_IMAGE");
            if (ch & IMAGE_FILE_LINE_NUMS_STRIPPED)
                names.push_back("LINE_NUMS_STRIPPED");
            if (ch & IMAGE_FILE_LOCAL_SYMS_STRIPPED)
                names.push_back("LOCAL_SYMS_STRIPPED");
            if (ch & IMAGE_FILE_AGGRESIVE_WS_TRIM)
                names.push_back("AGGRESIVE_WS_TRIM");
            if (ch & IMAGE_FILE_LARGE_ADDRESS_AWARE)
                names.push_back("LARGE_ADDRESS_AWARE");
            if (ch & IMAGE_FILE_BYTES_REVERSED_LO)
                names.push_back("BYTES_REVERSED_LO");
            if (ch & IMAGE_FILE_32BIT_MACHINE)
                names.push_back("32BIT_MACHINE");
            if (ch & IMAGE_FILE_DEBUG_STRIPPED)
                names.push_back("DEBUG_STRIPPED");
            if (ch & IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP)
                names.push_back("REMOVABLE_RUN_FROM_SWAP");
            if (ch & IMAGE_FILE_NET_RUN_FROM_SWAP)
                names.push_back("NET_RUN_FROM_SWAP");
            if (ch & IMAGE_FILE_SYSTEM)
                names.push_back("SYSTEM");
            if (ch & IMAGE_FILE_DLL)
                names.push_back("DLL");
            if (ch & IMAGE_FILE_UP_SYSTEM_ONLY)
                names.push_back("UP_SYSTEM_ONLY");
            if (ch & IMAGE_FILE_BYTES_REVERSED_HI)
                names.push_back("BYTES_REVERSED_HI");

            std::string out;
            if (names.empty()) {
                out = "None";
            } else {
                for (size_t i = 0; i < names.size(); ++i) {
                    if (i)
                        out += ", ";
                    out += names[i];
                }
            }

            ImGui::Text("0x%04X (%s)", ch, out.c_str());
        });

        ImGui::EndTable();
    }

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);

    if (is32) {
        ImGui::Text("Optional Header (IMAGE_OPTIONAL_HEADER32)");
        DrawOptionalHeader(ntHeaders32.OptionalHeader);
    } else {
        ImGui::Text("Optional Header (IMAGE_OPTIONAL_HEADER64)");
        DrawOptionalHeader(ntHeaders64.OptionalHeader);
    }

    ImGui::EndChild();
}

void PEViewer::DrawDataDirectories(const IMAGE_DATA_DIRECTORY* dataDirectories)
{
    ImGui::BeginChild("DataDirectoriesList");

    // Array of data directory names with descriptions
    static const struct {
        const char* name;
        const char* description;
    } directoryInfo[] = {
        { "Export Directory", "Functions exported by the module" },
        { "Import Directory", "Functions imported from other modules" },
        { "Resource Directory", "Embedded resources (icons, dialogs, etc.)" },
        { "Exception Directory", "Exception handling information" },
        { "Security Directory", "Security certificates and signatures" },
        { "Base Relocation Table", "Relocation information for ASLR" },
        { "Debug Directory", "Debugging information" },
        { "Architecture", "Architecture-specific data" },
        { "Global Ptr", "Global pointer register value" },
        { "TLS Directory", "Thread Local Storage data" },
        { "Load Config Directory", "Load configuration structure" },
        { "Bound Import Directory", "Bound import information" },
        { "IAT", "Import Address Table" },
        { "Delay Import Directory", "Delayed import information" },
        { "COM Descriptor", "COM runtime descriptors" },
        { "Reserved", "Reserved for future use" },
    };

    for (DWORD i = 0; i < IMAGE_NUMBEROF_DIRECTORY_ENTRIES; i++) {
        const IMAGE_DATA_DIRECTORY& dir = dataDirectories[i];

        bool isEmpty = (dir.VirtualAddress == 0 && dir.Size == 0);
        if (isEmpty) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        }

        bool isSelected = (m_SelectedDirectory == static_cast<int>(i));
        if (ImGui::Selectable(std::format("{}##{}", directoryInfo[i].name, i).c_str(), isSelected)) {
            m_SelectedDirectory = i;
        }

        if (ImGui::IsItemHovered() && !isEmpty) {
            if (directoryInfo[i].description) {
                ImGui::BeginTooltip();
                ImGui::Text("%s", directoryInfo[i].description);
                ImGui::Separator();
                ImGui::Text("RVA: 0x%08X", dir.VirtualAddress);
                ImGui::Text("Size: 0x%X (%d bytes)", dir.Size, dir.Size);
                ImGui::EndTooltip();
            }
        }

        if (isEmpty) {
            ImGui::PopStyleColor();
        }
    }

    ImGui::EndChild();
}

void PEViewer::DrawDataDirectoriesTab()
{
    const MODULEENTRY32& entry = m_State.Modules[m_SelectedIndex];
    IMAGE_DOS_HEADER dosHeader = m_State.Process.Read<IMAGE_DOS_HEADER>(reinterpret_cast<uintptr_t>(entry.modBaseAddr));
    uintptr_t ntAddr = reinterpret_cast<uintptr_t>(entry.modBaseAddr) + dosHeader.e_lfanew;

    WORD optionalMagic = m_State.Process.Read<WORD>(ntAddr + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER));
    bool is32 = (optionalMagic == IMAGE_NT_OPTIONAL_HDR32_MAGIC);

    IMAGE_NT_HEADERS32 ntHeaders32 {};
    IMAGE_NT_HEADERS64 ntHeaders64 {};
    IMAGE_DATA_DIRECTORY* dataDirectories = nullptr;
    DWORD numberOfRvaAndSizes = 0;

    if (is32) {
        ntHeaders32 = m_State.Process.Read<IMAGE_NT_HEADERS32>(ntAddr);
        dataDirectories = ntHeaders32.OptionalHeader.DataDirectory;
        numberOfRvaAndSizes = ntHeaders32.OptionalHeader.NumberOfRvaAndSizes;
    } else {
        ntHeaders64 = m_State.Process.Read<IMAGE_NT_HEADERS64>(ntAddr);
        dataDirectories = ntHeaders64.OptionalHeader.DataDirectory;
        numberOfRvaAndSizes = ntHeaders64.OptionalHeader.NumberOfRvaAndSizes;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2 { 0.0f, 0.0f });
    ImGui::BeginChild("PEViewerDataDirectoriesTab", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PopStyleVar();

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2 { 0.0f, 0.0f });
    bool render = ImGui::BeginTable("DataDirectoriesSplit", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersV);
    ImGui::PopStyleVar();
    if (render) {
        ImGui::TableSetupColumn("Directory List", ImGuiTableColumnFlags_WidthFixed, 250.0f);
        ImGui::TableSetupColumn("Directory Details", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        DrawDataDirectories(dataDirectories);

        ImGui::TableSetColumnIndex(1);
        if (m_SelectedDirectory >= 0 && m_SelectedDirectory < numberOfRvaAndSizes) {
            DrawDirectoryDetails(dataDirectories[m_SelectedDirectory], m_SelectedDirectory);
        } else {
            ImGui::BeginChild("DataDirectoryEmptyState");
            ImGui::Text("Select a data directory from the left to view its contents.");
            ImGui::EndChild();
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
}

void PEViewer::DrawDirectoryDetails(const IMAGE_DATA_DIRECTORY& dir, DWORD index)
{
    if (dir.VirtualAddress == 0 || dir.Size == 0) {
        ImGui::BeginChild("EmptyDirectoryDetails");
        ImGui::Text("This data directory is empty.");
        ImGui::Text("Virtual Address: 0x%08X", dir.VirtualAddress);
        ImGui::Text("Size: 0x%X (%d bytes)", dir.Size, dir.Size);
        ImGui::EndChild();
        return;
    }

    const MODULEENTRY32& entry = m_State.Modules[m_SelectedIndex];
    uintptr_t baseAddress = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
    uintptr_t directoryAddress = baseAddress + dir.VirtualAddress;

    ImGui::BeginChild("DirectoryDetails", ImVec2(0, 0), true);

    ImGui::Text("Directory Information");
    ImGui::Separator();

    ImGui::BeginTable("DirectoryInfo", 2, ImGuiTableFlags_Resizable);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("Virtual Address (RVA)");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("0x%08X", dir.VirtualAddress);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("Virtual Address (VA)");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("0x%016llX", directoryAddress);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("Size");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("0x%X (%d bytes)", dir.Size, dir.Size);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("Directory Type");
    ImGui::TableSetColumnIndex(1);
    static const char* directoryNames[] = {
        "Export Directory", "Import Directory", "Resource Directory", "Exception Directory",
        "Security Directory", "Base Relocation Table", "Debug Directory", "Architecture",
        "Global Ptr", "TLS Directory", "Load Config Directory", "Bound Import Directory",
        "IAT", "Delay Import Directory", "COM Descriptor"
    };
    if (index < 16) {
        ImGui::Text("%s", directoryNames[index]);
    } else {
        ImGui::Text("Unknown Directory (Index %d)", index);
    }

    ImGui::EndTable();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);

    switch (index) {
        case IMAGE_DIRECTORY_ENTRY_EXPORT: DrawExportDirectory(baseAddress, dir); break;
        case IMAGE_DIRECTORY_ENTRY_IMPORT: DrawImportDirectory(baseAddress, dir); break;
        case IMAGE_DIRECTORY_ENTRY_RESOURCE: DrawResourceDirectory(baseAddress, dir); break;
        case IMAGE_DIRECTORY_ENTRY_DEBUG: DrawDebugDirectory(baseAddress, dir); break;
        case IMAGE_DIRECTORY_ENTRY_TLS: DrawTLSDirectory(baseAddress, dir); break;
        default: {
            ImGui::Text("Raw Directory Data");
            ImGui::Separator();

            std::vector<uint8_t> data(dir.Size);
            m_State.Process.ReadBuffer(directoryAddress, data.data(), data.size());

            size_t dumpSize = data.size();
            size_t rows = (dumpSize + 15) / 16;

            Fonts::Push(Fonts::Type::JetBrainsMono);
            ImGui::BeginChild("HexDump", ImVec2(0, 300), true);
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(rows));
            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                    size_t i = static_cast<size_t>(row) * 16;
                    uint32_t offset = static_cast<uint32_t>(i);

                    std::string hex;
                    hex.reserve(16 * 3);
                    std::string ascii;
                    ascii.reserve(16);
                    for (size_t j = i; j < i + 16 && j < dumpSize; ++j) {
                        char buf[4];
                        std::snprintf(buf, sizeof(buf), "%02X ", data[j]);
                        hex += buf;
                        char c = (data[j] >= 32 && data[j] <= 126) ? static_cast<char>(data[j]) : '.';
                        ascii += c;
                    }

                    ImGui::Text("%08X: ", offset);
                    ImGui::SameLine();
                    ImGui::TextUnformatted(hex.c_str());
                    ImGui::SameLine();
                    ImGui::TextUnformatted(ascii.c_str());
                }
            }
            ImGui::EndChild();
            Fonts::Pop();
            break;
        }
    }

    ImGui::EndChild();
}

void PEViewer::DrawExportDirectory(uintptr_t baseAddress, const IMAGE_DATA_DIRECTORY& dir)
{
    uintptr_t exportAddr = baseAddress + dir.VirtualAddress;
    IMAGE_EXPORT_DIRECTORY exportDir = m_State.Process.Read<IMAGE_EXPORT_DIRECTORY>(exportAddr);

    ImGui::Text("Export Directory Information");
    ImGui::Separator();

    ImGui::BeginTable("ExportInfo", 2, ImGuiTableFlags_Resizable);
    ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 200.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

#define ADD_EXPORT_FIELD(field)    \
    ImGui::TableNextRow();         \
    ImGui::TableSetColumnIndex(0); \
    ImGui::Text(#field);           \
    ImGui::TableSetColumnIndex(1); \
    ImGui::Text("0x%08X", exportDir.field);

    ADD_EXPORT_FIELD(Characteristics);
    ADD_EXPORT_FIELD(TimeDateStamp);
    ADD_EXPORT_FIELD(MajorVersion);
    ADD_EXPORT_FIELD(MinorVersion);
    ADD_EXPORT_FIELD(Name);
    ADD_EXPORT_FIELD(Base);
    ADD_EXPORT_FIELD(NumberOfFunctions);
    ADD_EXPORT_FIELD(NumberOfNames);
    ADD_EXPORT_FIELD(AddressOfFunctions);
    ADD_EXPORT_FIELD(AddressOfNames);
    ADD_EXPORT_FIELD(AddressOfNameOrdinals);

    ImGui::EndTable();

    // Show exported functions
    if (exportDir.NumberOfFunctions > 0) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
        ImGui::Text("Exported Functions");
        ImGui::Separator();

        ImGui::BeginChild("ExportedFunctions", ImVec2(0, 200), true);

        uintptr_t functionsAddr = baseAddress + exportDir.AddressOfFunctions;
        uintptr_t namesAddr = baseAddress + exportDir.AddressOfNames;
        uintptr_t ordinalsAddr = baseAddress + exportDir.AddressOfNameOrdinals;

        int count = exportDir.NumberOfNames > 0x7FFFFFFF ? 0x7FFFFFFF : static_cast<int>(exportDir.NumberOfNames);
        ImGuiListClipper clipper;
        clipper.Begin(count);
        while (clipper.Step()) {
            for (int ii = clipper.DisplayStart; ii < clipper.DisplayEnd; ++ii) {
                DWORD i = static_cast<DWORD>(ii);
                DWORD nameRVA = m_State.Process.Read<DWORD>(namesAddr + i * sizeof(DWORD));
                WORD ordinal = m_State.Process.Read<WORD>(ordinalsAddr + i * sizeof(WORD));
                DWORD functionRVA = m_State.Process.Read<DWORD>(functionsAddr + ordinal * sizeof(DWORD));

                std::string functionName = m_State.Process.ReadString(baseAddress + nameRVA, 128);

                ImGui::Text("%s (Ordinal: %d, RVA: 0x%08X)",
                    functionName.c_str(), ordinal + exportDir.Base, functionRVA);
            }
        }

        ImGui::EndChild();
    }
}

void PEViewer::DrawImportDirectory(uintptr_t baseAddress, const IMAGE_DATA_DIRECTORY& dir)
{
    uintptr_t importAddr = baseAddress + dir.VirtualAddress;

    ImGui::Text("Import Directory");
    ImGui::Separator();

    ImGui::BeginChild("ImportedModules", ImVec2(0, 0), true);

    DWORD index = 0;
    while (true) {
        IMAGE_IMPORT_DESCRIPTOR importDesc = m_State.Process.Read<IMAGE_IMPORT_DESCRIPTOR>(importAddr + index * sizeof(IMAGE_IMPORT_DESCRIPTOR));
        if (importDesc.OriginalFirstThunk == 0 && importDesc.FirstThunk == 0 && importDesc.Name == 0) {
            break;
        }

        std::string moduleName = m_State.Process.ReadString(baseAddress + importDesc.Name, 256);

        ImGui::Text("Module: %s", moduleName.c_str());
        ImGui::Indent();

        uintptr_t thunkAddr = baseAddress + importDesc.OriginalFirstThunk;
        if (thunkAddr == baseAddress) {
            thunkAddr = baseAddress + importDesc.FirstThunk;
        }

        for (DWORD i = 0;; i++) {
            uintptr_t thunkValue = m_State.Process.Read<uint64_t>(thunkAddr + i * sizeof(uint64_t));
            if (thunkValue == 0) {
                break;
            }

            if (thunkValue & IMAGE_ORDINAL_FLAG) {
                DWORD ordinal = thunkValue & 0xFFFF;
                ImGui::Text("Ordinal: %d (RVA: 0x%08X)", ordinal, thunkValue);
            } else {
                uintptr_t importByNameAddr = baseAddress + thunkValue;
                IMAGE_IMPORT_BY_NAME importByName = m_State.Process.Read<IMAGE_IMPORT_BY_NAME>(importByNameAddr);
                std::string functionName = m_State.Process.ReadString(importByNameAddr + 2, 128);
                ImGui::Text("Function: %s (Hint: %d)", functionName.c_str(), importByName.Hint);
            }
        }

        ImGui::Unindent();
        ImGui::Separator();
        index++;
    }

    ImGui::EndChild();
}

void PEViewer::DrawResourceDirectory(uintptr_t baseAddress, const IMAGE_DATA_DIRECTORY& dir)
{
    uintptr_t resourceAddr = baseAddress + dir.VirtualAddress;

    ImGui::Text("Resource Directory");
    ImGui::Separator();
    ImGui::Text("Resource data is present at RVA 0x%08X, size 0x%X", dir.VirtualAddress, dir.Size);
    ImGui::Text("Use a resource viewer for detailed resource inspection.");

    // Simple resource tree display
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
    ImGui::Text("Resource Types:");
    ImGui::Separator();

    IMAGE_RESOURCE_DIRECTORY rootDir = m_State.Process.Read<IMAGE_RESOURCE_DIRECTORY>(resourceAddr);

    ImGui::BeginChild("ResourceTree", ImVec2(0, 200), true);

    for (DWORD i = 0; i < rootDir.NumberOfNamedEntries + rootDir.NumberOfIdEntries; i++) {
        IMAGE_RESOURCE_DIRECTORY_ENTRY entry = m_State.Process.Read<IMAGE_RESOURCE_DIRECTORY_ENTRY>(
            resourceAddr + sizeof(IMAGE_RESOURCE_DIRECTORY) + i * sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY));

        if (entry.NameIsString) {
            // Named entry
            IMAGE_RESOURCE_DIR_STRING_U nameStr = m_State.Process.Read<IMAGE_RESOURCE_DIR_STRING_U>(
                resourceAddr + entry.NameOffset);
            ImGui::Text("Named: %s", nameStr.NameString);
        } else {
            // ID entry
            ImGui::Text("ID: %d", entry.Id);
        }
    }

    ImGui::EndChild();
}

void PEViewer::DrawDebugDirectory(uintptr_t baseAddress, const IMAGE_DATA_DIRECTORY& dir)
{
    uintptr_t debugAddr = baseAddress + dir.VirtualAddress;
    DWORD numEntries = dir.Size / sizeof(IMAGE_DEBUG_DIRECTORY);

    ImGui::Text("Debug Directory");
    ImGui::Separator();
    ImGui::Text("Number of debug entries: %d", numEntries);

    ImGui::BeginChild("DebugEntries", ImVec2(0, 0), true);

    for (DWORD i = 0; i < numEntries; i++) {
        IMAGE_DEBUG_DIRECTORY debugDir = m_State.Process.Read<IMAGE_DEBUG_DIRECTORY>(
            debugAddr + i * sizeof(IMAGE_DEBUG_DIRECTORY));

        ImGui::Text("Entry %d:", i);
        ImGui::Indent();

        const char* type = "Unknown";
        switch (debugDir.Type) {
            case IMAGE_DEBUG_TYPE_UNKNOWN: type = "Unknown"; break;
            case IMAGE_DEBUG_TYPE_COFF: type = "COFF"; break;
            case IMAGE_DEBUG_TYPE_CODEVIEW: type = "CodeView"; break;
            case IMAGE_DEBUG_TYPE_FPO: type = "FPO"; break;
            case IMAGE_DEBUG_TYPE_MISC: type = "Misc"; break;
            case IMAGE_DEBUG_TYPE_EXCEPTION: type = "Exception"; break;
            case IMAGE_DEBUG_TYPE_FIXUP: type = "Fixup"; break;
            case IMAGE_DEBUG_TYPE_OMAP_TO_SRC: type = "OmapToSrc"; break;
            case IMAGE_DEBUG_TYPE_OMAP_FROM_SRC: type = "OmapFromSrc"; break;
            case IMAGE_DEBUG_TYPE_BORLAND: type = "Borland"; break;
            case IMAGE_DEBUG_TYPE_RESERVED10: type = "Reserved10"; break;
            case IMAGE_DEBUG_TYPE_CLSID: type = "CLSID"; break;
        }

        ImGui::Text("Type: %s (0x%04X)", type, debugDir.Type);
        ImGui::Text("Size of data: 0x%X", debugDir.SizeOfData);
        ImGui::Text("Address of raw data: 0x%08X", debugDir.AddressOfRawData);
        ImGui::Text("Pointer to raw data: 0x%08X", debugDir.PointerToRawData);

        if (debugDir.TimeDateStamp) {
            std::time_t timeT = debugDir.TimeDateStamp;
            std::tm tmStruct {};
            if (localtime_s(&tmStruct, &timeT) == 0) {
                char timeStr[100];
                if (std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tmStruct) != 0) {
                    ImGui::Text("Timestamp: %s", timeStr);
                }
            }
        }

        ImGui::Unindent();
        ImGui::Separator();
    }

    ImGui::EndChild();
}

void PEViewer::DrawTLSDirectory(uintptr_t baseAddress, const IMAGE_DATA_DIRECTORY& dir)
{
    uintptr_t tlsAddr = baseAddress + dir.VirtualAddress;

    ImGui::Text("TLS Directory");
    ImGui::Separator();

    bool is64Bit = (sizeof(uintptr_t) == 8);

    if (is64Bit) {
        IMAGE_TLS_DIRECTORY64 tlsDir = m_State.Process.Read<IMAGE_TLS_DIRECTORY64>(tlsAddr);

        ImGui::BeginTable("TLSInfo64", 2, ImGuiTableFlags_Resizable);
        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 200.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

#define ADD_TLS64_FIELD(field)     \
    ImGui::TableNextRow();         \
    ImGui::TableSetColumnIndex(0); \
    ImGui::Text(#field);           \
    ImGui::TableSetColumnIndex(1); \
    ImGui::Text("0x%016llX", tlsDir.field);

        ADD_TLS64_FIELD(StartAddressOfRawData);
        ADD_TLS64_FIELD(EndAddressOfRawData);
        ADD_TLS64_FIELD(AddressOfIndex);
        ADD_TLS64_FIELD(AddressOfCallBacks);
        ADD_TLS64_FIELD(SizeOfZeroFill);

        ImGui::EndTable();
    } else {
        IMAGE_TLS_DIRECTORY32 tlsDir = m_State.Process.Read<IMAGE_TLS_DIRECTORY32>(tlsAddr);

        ImGui::BeginTable("TLSInfo32", 2, ImGuiTableFlags_Resizable);
        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 200.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

#define ADD_TLS32_FIELD(field)     \
    ImGui::TableNextRow();         \
    ImGui::TableSetColumnIndex(0); \
    ImGui::Text(#field);           \
    ImGui::TableSetColumnIndex(1); \
    ImGui::Text("0x%08X", tlsDir.field);

        ADD_TLS32_FIELD(StartAddressOfRawData);
        ADD_TLS32_FIELD(EndAddressOfRawData);
        ADD_TLS32_FIELD(AddressOfIndex);
        ADD_TLS32_FIELD(AddressOfCallBacks);
        ADD_TLS32_FIELD(SizeOfZeroFill);

        ImGui::EndTable();
    }

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
    ImGui::Text("Note: TLS callbacks are executed before the entry point.");
}