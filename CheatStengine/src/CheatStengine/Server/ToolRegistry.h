#pragma once

#include <CheatStengine/Core/State.h>
#include <CheatStengine/Server/JobManager.h>
#include <CheatStengine/Tools/MemoryScanner.h>

#include <nlohmann/json.hpp>

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Server {

    struct ToolContext {
        State& AppState;
        JobManager& Jobs;
        std::unique_ptr<MemoryScanner>& Scanner;
    };

    struct Tool {
        using Handler = std::function<nlohmann::json(ToolContext&, const nlohmann::json&)>;

        std::string Name;
        std::string Description;
        nlohmann::json InputSchema;
        Handler Invoke;
    };

    class ToolRegistry {
    public:
        ToolRegistry();

        [[nodiscard]] nlohmann::json ListTools() const;
        [[nodiscard]] nlohmann::json CallTool(ToolContext& context, const std::string& name, const nlohmann::json& arguments) const;
        [[nodiscard]] nlohmann::json DescribeAll() const;

        bool SetEnabled(const std::string& name, bool enabled);
        [[nodiscard]] bool IsEnabled(const std::string& name) const;

    private:
        void Register(Tool tool);

        void RegisterCoreTools();     // process list / attach / modules / job status
        void RegisterMemoryTools();   // read / write / query / resolve
        void RegisterAnalysisTools(); // disassemble / assemble / dissect / pattern gen
        void RegisterScanTools();     // pattern scan / value scan

        std::vector<Tool> m_Tools;

        mutable std::mutex m_Mutex;
        std::unordered_map<std::string, bool> m_Disabled;
    };

}
