﻿#include "DirectX.h"
#include "Window.h"
#include <imgui.h>
#include "State.h"
#include "Util.h"
#include <algorithm>
#include <ranges>
#include <filesystem>
#include "Logger.h"
using namespace std;


void Window::DropFile(const std::string& file) {
    if (!util::isFileDll(file)) return;
    auto filename = std::filesystem::path(file).filename().string();
    auto found = std::ranges::any_of(state::dlls, [&file](const auto& dll) {return dll.full == file; });
    if (!found) {
        state::dlls.emplace_back(filename, file, "", true);
        state::save();
    }
}


void DirectX::Render()
{
    auto& io = ImGui::GetIO();
    static bool injected = false;
    const char* lastProcess = state::lastProcess.c_str();
    if (!state::dlls.empty() && !state::getCurrentDll().lastProcess.empty())
        lastProcess = state::getCurrentDll().lastProcess.c_str();
    ImGui::Begin("Main Window", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);                          // Create a window called "Hello, world!" and append into it.
    ImGui::Text("Process List | Last Process : %s", lastProcess);               // Display some text (you can use a format strings too)
    //ImGui::ShowDemoWindow();
    auto& processList = util::GetProcessList();
    auto currentProcess = processList.at(state::processIdx);
    state::selectedProcess = currentProcess.name.c_str();
    if (ImGui::BeginCombo("##process_combo", state::selectedProcess)) // The second parameter is the label previewed before opening the combo.
    {
        for (auto n = 0u; n < processList.size(); n++)
        {
            auto generateId = [](ProcessInfo& process) {
                return process.name + "##" + to_string((int)process.hwnd);
            };
            auto id = generateId(processList[n]);
            auto currentId = generateId(currentProcess);
            bool is_selected = (id == currentId); // You can store your selection however you want, outside or inside your objects
            if (ImGui::Selectable(id.c_str(), is_selected)) {
                state::selectedProcess = processList[n].name.c_str();
                state::processIdx = n;
            }
            if (ImGui::IsItemHovered()) {
                auto utf = util::WideToUTF8(processList[n].title);
                ImGui::SetTooltip("%s", utf.c_str());
            }
            if (is_selected)
                ImGui::SetItemDefaultFocus();   // You may set the initial focus when opening the combo (scrolling + for keyboard navigation support)
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", currentProcess.fullPath.c_str());
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        util::RefreshProcessList();
        for (size_t i{ 0 }; i < processList.size(); i++) {
            auto& process = processList[i];
            if (process.name.compare(lastProcess) == 0) {
                state::processIdx = i;
                break;
            }
        }
    }
    if (ImGui::BeginListBox("##dll_list"))
    {
        for (size_t n = 0; n < state::dlls.size(); n++)
        {
            const bool is_selected = (state::dllIdx == n);
            auto fileExists = state::dlls[n].exists;
            if (ImGui::Selectable(state::dlls[n].name.c_str(), is_selected, fileExists ? 0 : ImGuiSelectableFlags_Disabled)) {
                state::dllIdx = n;
            }
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::Button("Remove?")) {
                    state::dlls.erase(state::dlls.begin() + n);
                    while (state::dllIdx > 0 && state::dllIdx >= state::dlls.size()) {
                        state::dllIdx--;
                    }
                    state::save();
                    ImGui::CloseCurrentPopup();
                    n--; // decrement n by 1 to account for the removed element
                }
                ImGui::EndPopup();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                if (fileExists)
                    ImGui::SetTooltip("%s", state::dlls[n].full.c_str());
                else
                    ImGui::SetTooltip("[X] File does not exists! [X]");
            }
            // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndListBox();
    }

    if (!state::dlls.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("Clear"))
            ImGui::OpenPopup("Clear?");
    }
    if (ImGui::BeginPopupModal("Clear?", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::Text("The DLL List will be cleared\nContinue?");
        if (ImGui::Button("Clear All", ImVec2(120, 0))) {
            state::dlls.clear();
            state::save();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear not exists", ImVec2(120, 0))) {
            //state::dlls.clear();
            state::dlls.erase(
                std::remove_if(
                    state::dlls.begin(),
                    state::dlls.end(),
                    [](const DllFile& dll) { return !dll.exists; }),
                state::dlls.end());
            state::save();
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }



    ImGui::Text("%-10s : %s", "Process", currentProcess.name.c_str());
    if (ImGui::IsItemHovered()) {
        auto utf = util::WideToUTF8(currentProcess.title);
        ImGui::SetTooltip("%s", utf.c_str());
    }
    if (!state::dlls.empty()) {
        static bool autoInject = false;
        ImGui::Text("%-10s : %s", "DLL", state::getCurrentDll().name.c_str());
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", state::getCurrentDll().full.c_str());
        }
        if (ImGui::Button("Inject", { ImGui::GetContentRegionAvail().x,40 })) {
            if (util::CheckProcessModule(currentProcess.id, state::getCurrentDll().name.c_str())) {
                //TODO: request confirmation
                LOG_DEBUG("%s already loaded in the process!", state::getCurrentDll().name.c_str());
            }
            else {
                //util::Inject(currentProcess.id, state::getCurrentDll().full);
                if (util::Inject(currentProcess.id, state::getCurrentDll().full)) {
                    LOG_INFO("%s Injected to %s", state::getCurrentDll().name.c_str(), currentProcess.name.c_str());
                }
                else {
                    LOG_ERROR("%s Injection failed into %s", state::getCurrentDll().name.c_str(), currentProcess.name.c_str());
                }
                state::getCurrentDll().lastProcess = currentProcess.name;
                state::save();
            }
        }
        //TODO: use ms to delay injection
        static float ms = 1.f;
        //ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0)); // Set the item spacing to zero
        static float aotWidth = 0.f;
        if (ImGui::Checkbox("Auto", &autoInject)) {
            if (autoInject) {
                LOG_DEBUG("Auto Inject enabled");
            }
            else {
                LOG_DEBUG("Auto Inject disabled");
                injected = false;
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Automatically inject to Last Process");
        }
        ImGui::SameLine();
        static bool topMost = false;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - aotWidth));
        if (ImGui::Checkbox("Always On Top", &topMost)) {
            auto hwnd = Window::GetHwnd();
            if (topMost) {
                SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            }
            else {
                SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Keep window on top");
        }
        aotWidth = ImGui::GetItemRectSize().x;
        //ImGui::PopStyleVar();
        if (autoInject) {
            ImGui::DragFloat("ms", &ms, 0.1f, 0.1f, 10.f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
        }
        //TODO: Check process module
        if (autoInject && currentProcess.name == lastProcess && !injected) {
            injected = true;
            if (util::Inject(currentProcess.id, state::getCurrentDll().full)) {
                LOG_INFO("%s Injected to %s", state::getCurrentDll().name.c_str(), currentProcess.name.c_str());
            }
            else {
                LOG_ERROR("%s Injection failed into %s", state::getCurrentDll().name.c_str(), currentProcess.name.c_str());
            }
            state::save();
        }
    }
    ImGui::Separator();
    logger::Draw("Log");

    ImGui::End();
}
void CALLBACK HandleWinEvent(HWINEVENTHOOK hook, DWORD event, HWND hwnd,
    LONG idObject, LONG idChild,
    DWORD dwEventThread, DWORD dwmsEventTime) {
    if (event == EVENT_SYSTEM_FOREGROUND && !state::dlls.empty()) {
        // The foreground window has changed, do something here
        auto& processList = util::RefreshProcessList();
        for (size_t i{ 0 }; i < processList.size(); i++) {
            auto& process = processList[i];
            if (process.name == state::getCurrentDll().lastProcess) {
                state::processIdx = i;
                break;
            }
        }
    }
}
int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd
) {  // Set the hook
    logger::Clear();
    auto hook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, NULL, HandleWinEvent, 0, 0, WINEVENT_OUTOFCONTEXT);
    if (hook == NULL) {
        // Failed to set hook
        auto err = GetLastError();
        return 1;
    }
    HWND hwnd = Window::Create("Simple DLL Injector", "simple_dll_injector", hInstance);
    if (!hwnd) {
        UnhookWinEvent(hook);
        return 1;
    }
    // Initialize Direct3D
    if (!DirectX::Init(hwnd))
    {
        DirectX::Destroy();
        Window::Destroy();
        return 1;
    }
    DirectX::ImGuiInit();

    util::RefreshProcessList();
    state::load();
    auto lastHwnd = hwnd;
    // Main loop
    while (Window::PumpMsg())
    {
        DirectX::Begin();
        DirectX::Render();
        DirectX::End();
    }
    DirectX::ImGuiDestroy();
    // Cleanup
    Window::Destroy();
    DirectX::Destroy();

    // Unhook when done
    UnhookWinEvent(hook);

    return 0;
}
