// =============================================================================
// process.cpp - implementation of the process enumeration helpers.
// =============================================================================

#include "process.h"

#include <windows.h>
#include <tlhelp32.h>

#include "config.h"

namespace process {

std::set<std::string> FindRunningImages(const std::vector<std::string>& imageNames) {
    std::set<std::string> found;

    // Lower case copies of the watch list, so the per-process comparison
    // below stays a simple string equality test.
    std::vector<std::string> lowered;
    lowered.reserve(imageNames.size());
    for (size_t i = 0; i < imageNames.size(); ++i) {
        lowered.push_back(config::ToLowerAscii(imageNames[i]));
    }

    // One snapshot lists every process currently running on the system.
    // The Toolhelp API only offers the wide variants on current Windows
    // SDKs, so process names are converted from UTF-16 to ASCII here.
    // (Watch entries are validated to be plain ASCII anyway.)
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return found; // snapshot failed; report "nothing running"
    }

    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            char exeName[MAX_PATH] = { 0 };
            WideCharToMultiByte(CP_ACP, 0, entry.szExeFile, -1,
                                exeName, MAX_PATH, NULL, NULL);
            const std::string exeLower = config::ToLowerAscii(exeName);
            for (size_t i = 0; i < lowered.size(); ++i) {
                if (exeLower == lowered[i]) {
                    found.insert(lowered[i]);
                    break; // this process cannot match another name
                }
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return found;
}

bool IsImageRunning(const std::string& imageName) {
    std::vector<std::string> names;
    names.push_back(imageName);
    return !FindRunningImages(names).empty();
}

} // namespace process
