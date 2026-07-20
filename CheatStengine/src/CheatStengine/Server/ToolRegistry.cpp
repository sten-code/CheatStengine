#include "ToolRegistry.h"

#include <CheatStengine/AddressEvaluator/Evaluator.h>
#include <CheatStengine/Assembly/Assembler.h>
#include <CheatStengine/Assembly/Formatter.h>
#include <CheatStengine/Tools/PatternGenerator.h>
#include <CheatStengine/Tools/PatternScanner.h>
#include <CheatStengine/Tools/StructDissect.h>
#include <Engine/Core/Log.h>

#include <zasm/zasm.hpp>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <format>
#include <optional>
#include <stdexcept>
#include <unordered_map>

namespace Server {

    namespace {

        [[noreturn]] void Fail(const std::string& message)
        {
            throw std::runtime_error(message);
        }

        Process& RequireProcess(ToolContext& context)
        {
            if (!context.AppState.Process || !context.AppState.Process->IsValid()) {
                Fail("no process is attached; call open_process first");
            }
            return *context.AppState.Process;
        }

        uintptr_t ResolveAddress(ToolContext& context, const nlohmann::json& value)
        {
            if (value.is_number_unsigned()) {
                return value.get<uintptr_t>();
            }

            if (value.is_string()) {
                Process& process = RequireProcess(context);
                AddressEvaluator::Result result = AddressEvaluator::Evaluate(value.get<std::string>(), process);
                if (result.IsError()) {
                    Fail(std::format("could not resolve address expression: {}", value.get<std::string>()));
                }
                return result.Value;
            }

            Fail("address must be a number or an expression string");
        }

        const nlohmann::json& Require(const nlohmann::json& args, const std::string& key)
        {
            auto it = args.find(key);
            if (it == args.end()) {
                Fail(std::format("missing required argument '{}'", key));
            }
            return *it;
        }

        ValueType ParseValueType(const std::string& name)
        {
            static const std::unordered_map<std::string, ValueType> map {
                { "int8", ValueType::Int8 }, { "int16", ValueType::Int16 },
                { "int32", ValueType::Int32 }, { "int64", ValueType::Int64 },
                { "uint8", ValueType::UInt8 }, { "uint16", ValueType::UInt16 },
                { "uint32", ValueType::UInt32 }, { "uint64", ValueType::UInt64 },
                { "float", ValueType::Float }, { "double", ValueType::Double }
            };
            auto it = map.find(name);
            if (it == map.end()) {
                Fail(std::format("unknown value type '{}'", name));
            }
            return it->second;
        }

        ScanType ParseScanType(const std::string& name)
        {
            static const std::unordered_map<std::string, ScanType> map {
                { "exact", ScanType::ExactValue },
                { "bigger", ScanType::BiggerThan },
                { "smaller", ScanType::SmallerThan },
                { "between", ScanType::ValueBetween },
                { "unknown", ScanType::UnknownInitialValue }
            };
            auto it = map.find(name);
            if (it == map.end()) {
                Fail(std::format("unknown scan type '{}'", name));
            }
            return it->second;
        }

    }

    ToolRegistry::ToolRegistry()
    {
        RegisterCoreTools();
        RegisterMemoryTools();
        RegisterAnalysisTools();
        RegisterScanTools();
    }

    void ToolRegistry::Register(Tool tool)
    {
        m_Tools.push_back(std::move(tool));
    }

    nlohmann::json ToolRegistry::ListTools() const
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        nlohmann::json tools = nlohmann::json::array();
        for (const Tool& tool : m_Tools) {
            if (auto it = m_Disabled.find(tool.Name); it != m_Disabled.end() && it->second) {
                continue; // user switched this one off
            }
            tools.push_back({
                { "name", tool.Name },
                { "description", tool.Description },
                { "inputSchema", tool.InputSchema },
            });
        }
        return tools;
    }

    nlohmann::json ToolRegistry::DescribeAll() const
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        nlohmann::json tools = nlohmann::json::array();
        for (const Tool& tool : m_Tools) {
            bool disabled = false;
            if (auto it = m_Disabled.find(tool.Name); it != m_Disabled.end()) {
                disabled = it->second;
            }
            tools.push_back({
                { "name", tool.Name },
                { "description", tool.Description },
                { "enabled", !disabled },
            });
        }
        return tools;
    }

    bool ToolRegistry::SetEnabled(const std::string& name, bool enabled)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto known = std::ranges::find_if(m_Tools, [&name](const Tool& t) { return t.Name == name; });
        if (known == m_Tools.end()) {
            return false;
        }
        m_Disabled[name] = !enabled;
        return true;
    }

    bool ToolRegistry::IsEnabled(const std::string& name) const
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_Disabled.find(name);
        return it == m_Disabled.end() || !it->second;
    }

    nlohmann::json ToolRegistry::CallTool(ToolContext& context, const std::string& name, const nlohmann::json& arguments) const
    {
        for (const Tool& tool : m_Tools) {
            if (tool.Name != name) {
                continue;
            }

            {
                std::lock_guard<std::mutex> lock(m_Mutex);
                if (auto it = m_Disabled.find(name); it != m_Disabled.end() && it->second) {
                    Fail(std::format("tool '{}' is disabled by the operator", name));
                }
            }

            const nlohmann::json args = arguments.is_object() ? arguments : nlohmann::json::object();
            return tool.Invoke(context, args);
        }

        Fail(std::format("unknown tool '{}'", name));
    }

    void ToolRegistry::RegisterCoreTools()
    {
        Register(Tool {
            .Name = "list_processes",
            .Description = "List running processes as { pid, name }. Use this to find a target to attach to.",
            .InputSchema = { { "type", "object" }, { "properties", nlohmann::json::object() } },
            .Invoke = [](ToolContext&, const nlohmann::json&) {
                nlohmann::json list = nlohmann::json::array();
                for (const PROCESSENTRY32& entry : Process::EnumerateProcesses()) {
                    list.push_back({ { "pid", entry.th32ProcessID }, { "name", entry.szExeFile } });
                }
                return nlohmann::json { { "processes", list }, { "count", list.size() } };
            },
        });

        Register(Tool {
            .Name = "open_process",
            .Description = "Attach to a process by pid. Replaces any currently attached process.",
            .InputSchema = {
                { "type", "object" },
                { "properties", { { "pid", { { "type", "integer" }, { "description", "Target process id" } } } } },
                { "required", { "pid" } } },
            .Invoke = [](ToolContext& context, const nlohmann::json& args) {
                uint32_t pid = Require(args, "pid").get<uint32_t>();

                if (context.Jobs.HasActiveJobs()) {
                    Fail("a scan job is still running; wait for it to finish (poll job_status) before attaching");
                }
                if (context.Scanner && context.Scanner->IsScanning()) {
                    Fail("a value scan is still running; wait for scan_results to report scanning=false before attaching");
                }

                context.Scanner.reset();

                context.AppState.Process = Process::Create(pid, ProcessMode::WinAPI);
                if (!context.AppState.Process || !context.AppState.Process->IsValid()) {
                    Fail(std::format("failed to attach to pid {}", pid));
                }

                context.AppState.Modules = context.AppState.Process->GetModuleEntries(true);
                return nlohmann::json {
                    { "attached", true },
                    { "pid", pid },
                    { "name", context.AppState.Process->GetName() },
                    { "moduleCount", context.AppState.Modules.size() },
                };
            },
        });

        Register(Tool {
            .Name = "get_status",
            .Description = "Report whether a process is attached and its pid/name/module count.",
            .InputSchema = { { "type", "object" }, { "properties", nlohmann::json::object() } },
            .Invoke = [](ToolContext& context, const nlohmann::json&) {
                bool attached = context.AppState.Process && context.AppState.Process->IsValid();
                nlohmann::json out { { "attached", attached } };
                if (attached) {
                    out["pid"] = context.AppState.Process->GetPid();
                    out["name"] = context.AppState.Process->GetName();
                    out["moduleCount"] = context.AppState.Modules.size();
                }
                return out;
            },
        });

        Register(Tool {
            .Name = "list_modules",
            .Description = "List loaded modules of the attached process as { name, base, size }.",
            .InputSchema = { { "type", "object" }, { "properties", nlohmann::json::object() } },
            .Invoke = [](ToolContext& context, const nlohmann::json&) {
                Process& process = RequireProcess(context);
                std::vector<MODULEENTRY32> modules = process.GetModuleEntries(true);
                context.AppState.Modules = modules;

                nlohmann::json list = nlohmann::json::array();
                for (const MODULEENTRY32& mod : modules) {
                    list.push_back({
                        { "name", mod.szModule },
                        { "base", std::format("0x{:X}", reinterpret_cast<uintptr_t>(mod.modBaseAddr)) },
                        { "size", mod.modBaseSize },
                    });
                }
                return nlohmann::json { { "modules", list }, { "count", list.size() } };
            },
        });

        Register(Tool {
            .Name = "job_status",
            .Description = "Query a long-running job (e.g. pattern_scan) by id, or list all jobs when id is omitted.",
            .InputSchema = {
                { "type", "object" },
                { "properties", { { "id", { { "type", "integer" }, { "description", "Job id from a scan tool" } } } } } },
            .Invoke = [](ToolContext& context, const nlohmann::json& args) {
                auto it = args.find("id");
                if (it == args.end()) {
                    return nlohmann::json { { "jobs", context.Jobs.DescribeAll() } };
                }
                return context.Jobs.Describe(it->get<uint64_t>());
            },
        });
    }

    void ToolRegistry::RegisterMemoryTools()
    {
        Register(Tool {
            .Name = "resolve_address",
            .Description = "Evaluate an address expression like \"module.dll+0x1234\" or a pointer chain into a numeric address.",
            .InputSchema = {
                { "type", "object" },
                { "properties", { { "expression", { { "type", "string" } } } } },
                { "required", { "expression" } } },
            .Invoke = [](ToolContext& context, const nlohmann::json& args) {
                Process& process = RequireProcess(context);
                std::string expr = Require(args, "expression").get<std::string>();
                AddressEvaluator::Result result = AddressEvaluator::Evaluate(expr, process);
                if (result.IsError()) {
                    Fail(std::format("could not resolve '{}'", expr));
                }
                return nlohmann::json {
                    { "expression", expr },
                    { "address", std::format("0x{:X}", result.Value) },
                    { "value", result.Value },
                };
            },
        });

        Register(Tool {
            .Name = "read_memory",
            .Description = "Read up to 4096 bytes at an address (number or expression). Returns hex plus common integer interpretations.",
            .InputSchema = {
                { "type", "object" },
                { "properties", {
                                    { "address", { { "description", "Numeric address or expression string" } } },
                                    { "size", { { "type", "integer" }, { "description", "Byte count (1-4096, default 8)" } } } } },
                { "required", { "address" } } },
            .Invoke = [](ToolContext& context, const nlohmann::json& args) {
                Process& process = RequireProcess(context);
                uintptr_t address = ResolveAddress(context, Require(args, "address"));

                size_t size = 8;
                if (auto it = args.find("size"); it != args.end()) {
                    size = it->get<size_t>();
                }
                if (size == 0 || size > 4096) {
                    Fail("size must be between 1 and 4096");
                }

                std::vector<uint8_t> bytes = process.ReadBytes(address, size);
                if (bytes.empty()) {
                    Fail(std::format("read failed at 0x{:X} (unreadable region?)", address));
                }

                std::string hex;
                hex.reserve(bytes.size() * 2);
                for (uint8_t byte : bytes) {
                    hex += std::format("{:02X}", byte);
                }

                nlohmann::json out {
                    { "address", std::format("0x{:X}", address) },
                    { "size", bytes.size() },
                    { "hex", hex },
                };

                auto decode = [&bytes](auto sample) {
                    std::memcpy(&sample, bytes.data(), sizeof(sample));
                    return sample;
                };

                if (bytes.size() >= 1) out["u8"] = bytes[0];
                if (bytes.size() >= 2) out["u16"] = decode(uint16_t {});
                if (bytes.size() >= 4) {
                    out["u32"] = decode(uint32_t {});
                    out["f32"] = decode(float {});
                }
                if (bytes.size() >= 8) {
                    out["u64"] = decode(uint64_t {});
                    out["f64"] = decode(double {});
                }
                return out;
            },
        });

        Register(Tool {
            .Name = "write_memory",
            .Description = "Write raw bytes (hex string) at an address (number or expression). Length is taken from the hex.",
            .InputSchema = {
                { "type", "object" },
                { "properties", {
                                    { "address", { { "description", "Numeric address or expression string" } } },
                                    { "hex", { { "type", "string" }, { "description", "Bytes as hex, e.g. \"90 90\" or \"9090\"" } } } } },
                { "required", { "address", "hex" } } },
            .Invoke = [](ToolContext& context, const nlohmann::json& args) {
                Process& process = RequireProcess(context);
                uintptr_t address = ResolveAddress(context, Require(args, "address"));
                std::string hex = Require(args, "hex").get<std::string>();

                std::vector<uint8_t> bytes;
                std::string nibble;
                for (char c : hex) {
                    if (std::isspace(static_cast<unsigned char>(c))) continue;
                    if (!std::isxdigit(static_cast<unsigned char>(c))) {
                        Fail(std::format("invalid hex digit '{}' in byte string", c));
                    }
                    nibble += c;
                    if (nibble.size() == 2) {
                        bytes.push_back(static_cast<uint8_t>(std::stoul(nibble, nullptr, 16)));
                        nibble.clear();
                    }
                }
                if (!nibble.empty()) {
                    Fail("hex string has an odd number of digits");
                }
                if (bytes.empty()) {
                    Fail("no bytes to write");
                }

                if (!process.WriteBuffer(address, bytes.data(), bytes.size())) {
                    Fail(std::format("write failed at 0x{:X} (protection?)", address));
                }
                return nlohmann::json {
                    { "address", std::format("0x{:X}", address) },
                    { "written", bytes.size() },
                };
            },
        });

        Register(Tool {
            .Name = "query_memory",
            .Description = "Query the memory region containing an address: base, size, state, protection, type.",
            .InputSchema = {
                { "type", "object" },
                { "properties", { { "address", { { "description", "Numeric address or expression string" } } } } },
                { "required", { "address" } } },
            .Invoke = [](ToolContext& context, const nlohmann::json& args) {
                Process& process = RequireProcess(context);
                uintptr_t address = ResolveAddress(context, Require(args, "address"));

                std::optional<MEMORY_BASIC_INFORMATION> mbi = process.Query(address);
                if (!mbi) {
                    Fail(std::format("query failed at 0x{:X}", address));
                }
                return nlohmann::json {
                    { "baseAddress", std::format("0x{:X}", reinterpret_cast<uintptr_t>(mbi->BaseAddress)) },
                    { "regionSize", mbi->RegionSize },
                    { "state", std::format("0x{:X}", mbi->State) },
                    { "protect", std::format("0x{:X}", mbi->Protect) },
                    { "type", std::format("0x{:X}", mbi->Type) },
                };
            },
        });
    }

    void ToolRegistry::RegisterAnalysisTools()
    {
        Register(Tool {
            .Name = "disassemble",
            .Description = "Disassemble up to 64 instructions at an address (number or expression). Returns address, bytes and text per line.",
            .InputSchema = {
                { "type", "object" },
                { "properties", {
                                    { "address", { { "description", "Numeric address or expression string" } } },
                                    { "count", { { "type", "integer" }, { "description", "Instruction count (1-64, default 10)" } } } } },
                { "required", { "address" } } },
            .Invoke = [](ToolContext& context, const nlohmann::json& args) {
                Process& process = RequireProcess(context);
                uintptr_t address = ResolveAddress(context, Require(args, "address"));

                size_t count = 10;
                if (auto it = args.find("count"); it != args.end()) {
                    count = it->get<size_t>();
                }
                count = std::clamp<size_t>(count, 1, 64);

                std::vector<uint8_t> code = process.ReadBytes(address, count * 16);
                if (code.empty()) {
                    Fail(std::format("read failed at 0x{:X}", address));
                }

                zasm::Decoder decoder(zasm::MachineMode::AMD64);
                nlohmann::json lines = nlohmann::json::array();

                size_t offset = 0;
                for (size_t i = 0; i < count && offset < code.size(); ++i) {
                    uintptr_t lineAddress = address + offset;
                    zasm::Decoder::Result res = decoder.decode(code.data() + offset, code.size() - offset, lineAddress);
                    if (!res) {
                        break;
                    }

                    const zasm::InstructionDetail& detail = res.value();
                    FormattedInstruction formatted = Formatter::Format(detail.getInstruction());

                    std::string bytes;
                    for (size_t b = 0; b < detail.getLength(); ++b) {
                        bytes += std::format("{:02X}", code[offset + b]);
                    }

                    lines.push_back({
                        { "address", std::format("0x{:X}", lineAddress) },
                        { "bytes", bytes },
                        { "text", formatted.Text },
                    });
                    offset += detail.getLength();
                }
                return nlohmann::json { { "instructions", lines }, { "count", lines.size() } };
            },
        });

        Register(Tool {
            .Name = "assemble",
            .Description = "Assemble x86-64 source into machine code bytes. If write=true and a process is attached, patches them at address.",
            .InputSchema = {
                { "type", "object" },
                { "properties", {
                                    { "source", { { "type", "string" }, { "description", "Assembly, newline separated" } } },
                                    { "address", { { "description", "Base address for encoding / patch target" } } },
                                    { "write", { { "type", "boolean" }, { "description", "Write bytes to the process (default false)" } } } } },
                { "required", { "source" } } },
            .Invoke = [](ToolContext& context, const nlohmann::json& args) {
                std::string source = Require(args, "source").get<std::string>();

                uintptr_t base = 0;
                bool write = args.value("write", false);
                if (auto it = args.find("address"); it != args.end()) {
                    base = ResolveAddress(context, *it);
                }

                Assembly::Assembler assembler;
                assembler.Assemble(source);

                zasm::Program program(zasm::MachineMode::AMD64);
                zasm::x86::Assembler a(program);
                for (const zasm::Instruction& instruction : assembler.GetInstructions()) {
                    a.emit(instruction);
                }

                zasm::Serializer serializer;
                if (serializer.serialize(program, base).getCode() != zasm::ErrorCode::None) {
                    Fail("assembler failed to serialize the given source");
                }

                const uint8_t* code = serializer.getCode();
                size_t codeSize = serializer.getCodeSize();
                if (codeSize == 0) {
                    Fail("assembled to zero bytes; check the source");
                }

                std::string hex;
                for (size_t i = 0; i < codeSize; ++i) {
                    hex += std::format("{:02X}", code[i]);
                }

                nlohmann::json out { { "hex", hex }, { "size", codeSize } };

                if (write) {
                    Process& process = RequireProcess(context);
                    if (!process.WriteBuffer(base, code, codeSize)) {
                        Fail(std::format("assembled ok but write failed at 0x{:X}", base));
                    }
                    out["written"] = true;
                    out["address"] = std::format("0x{:X}", base);
                }
                return out;
            },
        });

        Register(Tool {
            .Name = "dissect_struct",
            .Description = "Interpret memory at an address as a struct: for each 8-byte slot report the raw qword and, if it points into readable memory, that it looks like a pointer.",
            .InputSchema = {
                { "type", "object" },
                { "properties", {
                                    { "address", { { "description", "Numeric address or expression string" } } },
                                    { "size", { { "type", "integer" }, { "description", "Struct size in bytes (8-1024, default 64)" } } } } },
                { "required", { "address" } } },
            .Invoke = [](ToolContext& context, const nlohmann::json& args) {
                Process& process = RequireProcess(context);
                uintptr_t address = ResolveAddress(context, Require(args, "address"));

                size_t size = 64;
                if (auto it = args.find("size"); it != args.end()) {
                    size = it->get<size_t>();
                }
                size = std::clamp<size_t>(size, 8, 1024);

                nlohmann::json fields = nlohmann::json::array();
                for (size_t offset = 0; offset + 8 <= size; offset += 8) {
                    uintptr_t slot = address + offset;
                    uintptr_t qword = process.Read<uintptr_t>(slot);

                    nlohmann::json field {
                        { "offset", std::format("0x{:X}", offset) },
                        { "value", std::format("0x{:X}", qword) },
                    };
                    if (qword != 0 && process.IsAddressReadable(qword)) {
                        field["isPointer"] = true;
                    }
                    fields.push_back(field);
                }
                return nlohmann::json {
                    { "address", std::format("0x{:X}", address) },
                    { "size", size },
                    { "fields", fields },
                };
            },
        });

        Register(Tool {
            .Name = "generate_pattern",
            .Description = "Generate a unique byte signature (IDA-style) for the instruction(s) at an address, wildcarding volatile operands.",
            .InputSchema = {
                { "type", "object" },
                { "properties", { { "address", { { "description", "Numeric address or expression string" } } } } },
                { "required", { "address" } } },
            .Invoke = [](ToolContext& context, const nlohmann::json& args) {
                uintptr_t address = ResolveAddress(context, Require(args, "address"));
                RequireProcess(context);

                PatternGenerator generator;
                std::optional<PatternResult> result = generator.Generate(
                    context.AppState.Process, address, 0x0, 0x7FFFFFFFFFFF);
                if (!result) {
                    Fail(std::format("could not generate a pattern at 0x{:X}", address));
                }
                return nlohmann::json {
                    { "address", std::format("0x{:X}", address) },
                    { "pattern", result->pattern },
                    { "byteCount", result->byteCount },
                    { "instrCount", result->instrCount },
                    { "wildcards", result->wildcards },
                };
            },
        });
    }

    void ToolRegistry::RegisterScanTools()
    {
        Register(Tool {
            .Name = "pattern_scan",
            .Description = "Scan the attached process for a byte pattern (\"48 8B ?? C3\"). Long-running: returns a jobId, then poll job_status.",
            .InputSchema = {
                { "type", "object" },
                { "properties", {
                                    { "pattern", { { "type", "string" }, { "description", "Space-separated hex with ?? wildcards" } } },
                                    { "module", { { "type", "string" }, { "description", "Optional module to limit the scan to" } } } } },
                { "required", { "pattern" } } },
            .Invoke = [](ToolContext& context, const nlohmann::json& args) {
                RequireProcess(context);
                std::string pattern = Require(args, "pattern").get<std::string>();
                std::string module = args.value("module", std::string {});

                std::unique_ptr<Process>* processSlot = &context.AppState.Process;
                std::string label = std::format("pattern_scan {}", pattern);

                uint64_t jobId = context.Jobs.Submit(label, [processSlot, pattern, module]() -> nlohmann::json {
                    PatternScanner scanner(*processSlot);
                    std::vector<uintptr_t> hits = module.empty()
                        ? scanner.PatternScan(pattern)
                        : scanner.PatternScan(pattern, module);

                    nlohmann::json addresses = nlohmann::json::array();
                    for (uintptr_t hit : hits) {
                        addresses.push_back(std::format("0x{:X}", hit));
                    }
                    return nlohmann::json { { "matches", addresses }, { "count", addresses.size() } };
                });

                return nlohmann::json {
                    { "jobId", jobId },
                    { "status", "running" },
                    { "hint", "poll job_status with this id" },
                };
            },
        });

        Register(Tool {
            .Name = "scan_value",
            .Description = "Start a value scan over the attached process. Fire-and-poll: returns immediately, then use scan_results.",
            .InputSchema = {
                { "type", "object" },
                { "properties", {
                                    { "valueType", { { "type", "string" }, { "description", "int8/16/32/64, uint8/16/32/64, float, double" } } },
                                    { "scanType", { { "type", "string" }, { "description", "exact, bigger, smaller, between, unknown" } } },
                                    { "value", { { "type", "string" }, { "description", "Target value for exact/bigger/smaller" } } },
                                    { "upper", { { "type", "string" }, { "description", "Upper bound for 'between'" } } },
                                    { "next", { { "type", "boolean" }, { "description", "Refine the previous scan instead of starting fresh" } } } } },
                { "required", { "valueType", "scanType" } } },
            .Invoke = [](ToolContext& context, const nlohmann::json& args) {
                RequireProcess(context);

                ValueType valueType = ParseValueType(Require(args, "valueType").get<std::string>());
                ScanType scanType = ParseScanType(Require(args, "scanType").get<std::string>());
                std::string value = args.value("value", std::string {});
                std::string upper = args.value("upper", std::string {});
                bool next = args.value("next", false);

                if (next) {
                    if (!context.Scanner) {
                        Fail("no active scan to refine; run scan_value without next=true first");
                    }
                    if (context.Scanner->IsScanning()) {
                        Fail("a scan is already in progress");
                    }
                    context.Scanner->NextScan(valueType, scanType, 0x0, 0x7FFFFFFFFFFF, value, upper);
                } else {
                    if (context.Scanner && context.Scanner->IsScanning()) {
                        Fail("a scan is already in progress; poll scan_results until scanning=false");
                    }
                    context.Scanner = std::make_unique<MemoryScanner>(context.AppState.Process);
                    if (!context.Scanner->FirstScan(valueType, scanType, 0x0, 0x7FFFFFFFFFFF, value, upper)) {
                        context.Scanner.reset();
                        Fail("failed to start scan (invalid value for the chosen type?)");
                    }
                }

                return nlohmann::json {
                    { "status", "scanning" },
                    { "hint", "poll scan_results until scanning is false" },
                };
            },
        });

        Register(Tool {
            .Name = "scan_results",
            .Description = "Report the current value-scan state: whether it's still scanning, the match count, and up to 100 result addresses.",
            .InputSchema = {
                { "type", "object" },
                { "properties", { { "limit", { { "type", "integer" }, { "description", "Max addresses to return (1-1000, default 100)" } } } } } },
            .Invoke = [](ToolContext& context, const nlohmann::json& args) {
                if (!context.Scanner) {
                    return nlohmann::json { { "scanning", false }, { "count", 0 }, { "results", nlohmann::json::array() } };
                }

                size_t limit = 100;
                if (auto it = args.find("limit"); it != args.end()) {
                    limit = std::clamp<size_t>(it->get<size_t>(), 1, 1000);
                }

                std::lock_guard lock(context.Scanner->GetMutex());
                const std::vector<ScannedAddress>& results = context.Scanner->GetResults();

                nlohmann::json addresses = nlohmann::json::array();
                for (size_t i = 0; i < results.size() && i < limit; ++i) {
                    addresses.push_back(std::format("0x{:X}", results[i].Address));
                }
                return nlohmann::json {
                    { "scanning", context.Scanner->IsScanning() },
                    { "count", results.size() },
                    { "results", addresses },
                };
            },
        });
    }

}




