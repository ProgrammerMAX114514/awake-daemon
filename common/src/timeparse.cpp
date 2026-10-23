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
// timeparse.cpp - implementation of the duration and clock time parsers.
// =============================================================================

#include "timeparse.h"

#include <cctype>
#include <cstdlib>

namespace timeparse {

const unsigned int kMaxDurationSeconds = 365u * 24u * 60u * 60u;

namespace {

const unsigned int kSecondsPerDay = 24u * 60u * 60u;

// Returns the multiplier for a unit character, or 0 when it is not a unit.
unsigned int UnitMultiplier(char unit) {
    switch (unit) {
        case 's': return 1u;
        case 'm': return 60u;
        case 'h': return 60u * 60u;
        case 'd': return kSecondsPerDay;
        default:  return 0u;
    }
}

} // namespace

bool ParseDuration(const std::string& text, unsigned int& seconds) {
    seconds = 0;
    if (text.empty()) {
        return false;
    }

    unsigned long long total = 0;
    size_t i = 0;
    while (i < text.size()) {
        // A group always starts with digits.
        if (!std::isdigit(static_cast<unsigned char>(text[i]))) {
            return false;
        }
        unsigned long long value = 0;
        while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
            value = value * 10 + static_cast<unsigned long long>(text[i] - '0');
            // Reject early instead of overflowing on absurd input like
            // "999999999999999999999h".
            if (value > kMaxDurationSeconds) {
                return false;
            }
            ++i;
        }
        // ... and ends with a unit.
        if (i >= text.size()) {
            return false;
        }
        const unsigned int multiplier = UnitMultiplier(
            static_cast<char>(std::tolower(static_cast<unsigned char>(text[i]))));
        if (multiplier == 0) {
            return false;
        }
        total += value * multiplier;
        if (total > kMaxDurationSeconds) {
            return false;
        }
        ++i;
    }

    if (total == 0) {
        return false; // "0m" makes no sense as a keep-awake period
    }
    seconds = static_cast<unsigned int>(total);
    return true;
}

bool ParseUntil(const std::string& text, unsigned int nowTodaySeconds,
                unsigned int& secondsFromNow, bool& rolledOver) {
    secondsFromNow = 0;
    rolledOver = false;

    const size_t colon = text.find(':');
    if (colon == std::string::npos) {
        return false;
    }
    const std::string hourText = text.substr(0, colon);
    const std::string minuteText = text.substr(colon + 1);
    if (hourText.empty() || minuteText.empty() || hourText.size() > 2 ||
        minuteText.size() != 2) {
        return false;
    }
    for (size_t i = 0; i < hourText.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(hourText[i]))) {
            return false;
        }
    }
    for (size_t i = 0; i < minuteText.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(minuteText[i]))) {
            return false;
        }
    }

    const unsigned int hours = static_cast<unsigned int>(std::atoi(hourText.c_str()));
    const unsigned int minutes = static_cast<unsigned int>(std::atoi(minuteText.c_str()));
    if (hours > 23 || minutes > 59) {
        return false;
    }

    const unsigned int target = hours * 60u * 60u + minutes * 60u;
    if (target <= nowTodaySeconds) {
        // Already gone today: the user almost certainly means tomorrow.
        rolledOver = true;
        secondsFromNow = kSecondsPerDay - nowTodaySeconds + target;
    } else {
        secondsFromNow = target - nowTodaySeconds;
    }
    return secondsFromNow > 0 && secondsFromNow <= kSecondsPerDay;
}

std::string FormatDuration(unsigned int seconds) {
    if (seconds == 0) {
        return "0s";
    }
    std::string text;
    const unsigned int days = seconds / kSecondsPerDay;
    const unsigned int hours = (seconds % kSecondsPerDay) / 3600u;
    const unsigned int minutes = (seconds % 3600u) / 60u;
    const unsigned int rest = seconds % 60u;
    if (days > 0) {
        text += std::to_string(days) + "d";
    }
    if (hours > 0) {
        text += std::to_string(hours) + "h";
    }
    if (minutes > 0) {
        text += std::to_string(minutes) + "m";
    }
    // Seconds are only shown when nothing larger was printed (a "30m" reads
    // better than "30m0s") or when they are the only component.
    if (rest > 0 && (text.empty() || seconds < 3600u)) {
        text += std::to_string(rest) + "s";
    }
    return text;
}

} // namespace timeparse
