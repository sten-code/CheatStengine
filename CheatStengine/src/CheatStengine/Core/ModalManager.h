#pragma once

#include <Engine/Core/Log.h>

#include <imgui.h>

#include <any>
#include <functional>
#include <string>
#include <unordered_map>

struct Modal {
    std::function<void(const std::string& name, const std::any& arg)> RenderFunction;
    bool Open = false; // Flag to indicate if the modal should be opened next frame
    std::any Payload;
};

class ModalManager {
public:
    ModalManager() = default;
    ~ModalManager() = default;

    void RegisterModal(const std::string& name, const std::function<void(const std::string& name, const std::any& payload)>& renderFunction)
    {
        m_Modals[name] = Modal { renderFunction, false, std::any() };
    }

    void UnregisterModal(const std::string& name)
    {
        m_Modals.erase(name);
    }

    void RenderModals()
    {
        for (auto& [name, modal] : m_Modals) {
            if (modal.Open) {
                ImGui::OpenPopup(name.c_str());
                modal.Open = false;
            }

            modal.RenderFunction(name, modal.Payload);
        }
    }

    template <typename T>
    void OpenModal(const std::string& name, const T& payload)
    {
        INFO("ModalManager::OpenModal: {}", name);
        auto& modal = m_Modals[name];
        modal.Payload = payload;
        modal.Open = true;
    }

    void OpenModal(const std::string& name) { OpenModal(name, std::any()); }

private:
    std::unordered_map<std::string, Modal> m_Modals;
};