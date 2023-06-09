#include "State.h"
#include <fstream>
#include "Process.h"
#include <ranges>
#include <algorithm>
#include <filesystem>
#include "Logger.h"

using namespace std;
namespace state {
    DllFile& getCurrentDll() {
        return dlls[dllIdx];
    }
    void save()
    {
        ofstream out;
        out.open("state.ini");
        lastProcess = process::GetProcessList()[processIdx].name;
        if (!dlls.empty()) {
            lastProcess = dlls[dllIdx].lastProcess;
        }
        out << "LAST_PROCESS:" << lastProcess << "\n";
        if (!dlls.empty()) {
            out << "LAST_DLL:" << dlls[dllIdx].full << "\n";
            for (auto& dll : dlls) {
                out << "DLL:" << dll.full;
                if (!dll.lastProcess.empty()) {
                    out << "|" << dll.lastProcess;
                }
                out << "\n";
            }
        }
    }

    void load() {
        dlls.clear();
        ifstream in("state.ini");
        if (!in) {
            return;
        }
        string line;
        string lastDll;
        while (getline(in, line)) {
            if (line.starts_with("LAST_PROCESS:")) {
                size_t idx = line.find_first_of(":");
                lastProcess = line.substr(idx + 1, line.length());
                auto& processList = process::GetProcessList();
                bool set = false;
                for (size_t i{ 0 }; i < processList.size(); i++) {
                    auto& process = processList[i];
                    if (process.name == lastProcess) {
                        processIdx = i;
                        set = true;
                        break;
                    }
                }
                if (!set)
                    processIdx = 0;
            }
            else if (line.starts_with("LAST_DLL:")) {
                size_t idx = line.find_first_of(":");
                lastDll = line.substr(idx + 1, line.length());
            }
            else if (line.starts_with("DLL:")) {
                size_t idx = line.find_first_of(":");
                auto file = line.substr(idx + 1, line.length());

                string lastP;
                idx = file.find("|");
                if (idx != string::npos) {
                    lastP = file.substr(idx + 1, file.length());
                    file = file.substr(0, idx);
                }

                auto found = std::ranges::any_of(dlls, [&file](const auto& dll) {return dll.full == file; });
                if (found) {
                    LOG_DEBUG("%s is duplicate", file.c_str());
                    continue;
                }

                auto filename = std::filesystem::path(file).filename().string();
                //auto name = strrchr(file.c_str(), '\\');
                //name++;
                auto exists = std::filesystem::exists(file);
                dlls.emplace_back(filename, file, lastP, exists);
            }
        }
        if (!lastDll.empty()) {
            for (size_t i{ 0 }; i < dlls.size(); i++) {
                if (dlls[i].full == lastDll && dlls[i].exists) {
                    dllIdx = i;
                    break;
                }
            }
        }
    }
}