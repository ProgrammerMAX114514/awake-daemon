// Copyright 2026 ProgrammerMAX114514
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// AI-GENERATED SOFTWARE: this file was generated with AI assistance.
// Please review it carefully before use. The author is not liable for
// any consequences arising from the use of this software.

// =============================================================================
// logexport.cpp - minimal STORED zip writer for the daemon log files.
//
// The zip format (PKWARE APPNOTE) needs three record types per archive:
// a local file header + the raw data per entry, one central directory entry
// per file and one end-of-central-directory record. Everything is written
// as little-endian; the entries are STORED (method 0), i.e. uncompressed,
// which keeps the writer to a CRC32 as its only "compression" logic.
// =============================================================================

#include "logexport.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace logexport {

namespace {

// CRC-32 (IEEE 802.3, polynomial 0xEDB88320), the checksum the zip format
// requires for every entry.
unsigned int Crc32(const std::string& data) {
    static unsigned int table[256];
    static bool tableReady = false;
    if (!tableReady) {
        for (unsigned int i = 0; i < 256; ++i) {
            unsigned int value = i;
            for (int bit = 0; bit < 8; ++bit) {
                value = (value & 1) ? (0xEDB88320u ^ (value >> 1)) : (value >> 1);
            }
            table[i] = value;
        }
        tableReady = true;
    }
    unsigned int crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < data.size(); ++i) {
        crc = table[(crc ^ static_cast<unsigned char>(data[i])) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

// Appends a 16-bit / 32-bit little-endian value.
void Put16(std::string& out, unsigned short value) {
    out.push_back(static_cast<char>(value & 0xFF));
    out.push_back(static_cast<char>((value >> 8) & 0xFF));
}

void Put32(std::string& out, unsigned int value) {
    Put16(out, static_cast<unsigned short>(value & 0xFFFF));
    Put16(out, static_cast<unsigned short>((value >> 16) & 0xFFFF));
}

// DOS date/time (1980-based, 2-second resolution) for a file's timestamps.
void DosDateTimeFromSystem(const SYSTEMTIME& st,
                           unsigned short& date, unsigned short& time) {
    date = static_cast<unsigned short>(((st.wYear - 1980) << 9) | (st.wMonth << 5) | st.wDay);
    time = static_cast<unsigned short>((st.wHour << 11) | (st.wMinute << 5) | (st.wSecond / 2));
}

std::string ReadWholeFile(const std::string& path, bool& ok) {
    std::ifstream file(path.c_str(), std::ios::binary);
    ok = file.is_open();
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

} // namespace

std::string LogDirectory() {
    char localAppData[MAX_PATH] = { 0 };
    if (!GetEnvironmentVariableA("LOCALAPPDATA", localAppData, MAX_PATH) ||
        localAppData[0] == '\0') {
        char temp[MAX_PATH] = { 0 };
        GetTempPathA(MAX_PATH, temp);
        return std::string(temp) + "awake\\logs\\daemon";
    }
    return std::string(localAppData) + "\\awake\\logs\\daemon";
}

bool ZipLogDirectory(const std::string& zipPath, int& fileCount,
                     std::string& error) {
    fileCount = 0;
    const std::string directory = LogDirectory();

    // Collect the daemon-*.log files (timestamp names sort chronologically).
    std::vector<std::string> names;
    WIN32_FIND_DATAA found;
    const HANDLE handle = FindFirstFileA((directory + "\\daemon-*.log").c_str(), &found);
    if (handle == INVALID_HANDLE_VALUE) {
        error = "no log files found in " + directory;
        return false;
    }
    do {
        if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            names.push_back(found.cFileName);
        }
    } while (FindNextFileA(handle, &found));
    FindClose(handle);
    std::sort(names.begin(), names.end());
    if (names.empty()) {
        error = "no log files found in " + directory;
        return false;
    }

    SYSTEMTIME st;
    GetLocalTime(&st);
    unsigned short dosDate = 0;
    unsigned short dosTime = 0;
    DosDateTimeFromSystem(st, dosDate, dosTime);

    std::ofstream zip(zipPath.c_str(), std::ios::binary | std::ios::trunc);
    if (!zip.is_open()) {
        error = "cannot create " + zipPath;
        return false;
    }

    std::string centralDirectory; // central directory entries accumulate here
    int archived = 0;

    for (size_t i = 0; i < names.size(); ++i) {
        bool ok = false;
        const std::string content = ReadWholeFile(directory + "\\" + names[i], ok);
        if (!ok) {
            continue; // skip unreadable files instead of failing the export
        }
        const unsigned int crc = Crc32(content);
        const unsigned int size = static_cast<unsigned int>(content.size());
        const unsigned int offset = static_cast<unsigned int>(zip.tellp());

        // Local file header + stored content.
        std::string header;
        Put32(header, 0x04034b50);
        Put16(header, 20);        // version needed
        Put16(header, 0);         // flags
        Put16(header, 0);         // method: stored
        Put16(header, dosTime);
        Put16(header, dosDate);
        Put32(header, crc);
        Put32(header, size);      // compressed size (stored: equal)
        Put32(header, size);      // uncompressed size
        Put16(header, static_cast<unsigned short>(names[i].size()));
        Put16(header, 0);         // extra length
        header += names[i];

        zip << header << content;

        // Central directory entry for this file.
        std::string central;
        Put32(central, 0x02014b50);
        Put16(central, 20);       // version made by
        Put16(central, 20);       // version needed
        Put16(central, 0);        // flags
        Put16(central, 0);        // method: stored
        Put16(central, dosTime);
        Put16(central, dosDate);
        Put32(central, crc);
        Put32(central, size);
        Put32(central, size);
        Put16(central, static_cast<unsigned short>(names[i].size()));
        Put16(central, 0);        // extra length
        Put16(central, 0);        // comment length
        Put16(central, 0);        // disk number start
        Put16(central, 0);        // internal attributes
        Put32(central, 0);        // external attributes
        Put32(central, offset);
        central += names[i];
        centralDirectory += central;
        ++archived;
    }

    // End of central directory.
    std::string eocd;
    Put32(eocd, 0x06054b50);
    Put16(eocd, 0); // this disk
    Put16(eocd, 0); // disk with the central directory
    Put16(eocd, static_cast<unsigned short>(archived));
    Put16(eocd, static_cast<unsigned short>(archived));
    Put32(eocd, static_cast<unsigned int>(centralDirectory.size()));
    Put32(eocd, static_cast<unsigned int>(zip.tellp()));
    Put16(eocd, 0); // comment length
    zip << centralDirectory << eocd;
    zip.close();

    if (archived == 0) {
        error = "no readable log files found in " + directory;
        DeleteFileA(zipPath.c_str());
        return false;
    }
    fileCount = archived;
    return true;
}

} // namespace logexport
