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
// timeparse.h - parsing of the "for <duration>" and "until <HH:MM>" arguments.
//
// The client converts both forms into a relative number of seconds before it
// talks to the daemon: the daemon only ever sees "expire in N seconds" and
// measures that with a monotonic clock, so changing the system time or the
// daylight saving offset cannot move an expiry point.
//
// Both functions are pure, which keeps them unit testable without a daemon.
// =============================================================================

#ifndef AWAKE_TIMEPARSE_H
#define AWAKE_TIMEPARSE_H

#include <string>

namespace timeparse {

// Longest accepted duration (365 days). Anything above is rejected instead of
// silently overflowing the internal arithmetic.
extern const unsigned int kMaxDurationSeconds;

// Parses a duration made of one or more <number><unit> groups, where the unit
// is s, m, h or d: "30m", "2h", "1h30m", "90s", "1d12h". A bare number is not
// accepted (the unit is what makes it unambiguous). Returns false when the
// text is empty, malformed, zero or above kMaxDurationSeconds.
bool ParseDuration(const std::string& text, unsigned int& seconds);

// Parses a wall clock time of the form "HH:MM" (H:MM is accepted as well) and
// returns how many seconds from now that is. When the time has already passed
// today, it is interpreted as tomorrow and rolledOver is set; the caller shows
// a warning in that case because the user may have meant something else.
// nowTodaySeconds is the number of seconds since local midnight.
bool ParseUntil(const std::string& text, unsigned int nowTodaySeconds,
                unsigned int& secondsFromNow, bool& rolledOver);

// Formats a number of seconds the way the CLI shows it: "45s", "30m",
// "1h30m", "1d2h". Used by the confirmation message and by "awake status".
std::string FormatDuration(unsigned int seconds);

} // namespace timeparse

#endif // AWAKE_TIMEPARSE_H
