// =============================================================================
// console.h - ANSI colored console output helpers.
//
// The whole tool uses plain ANSI escape sequences for coloring. Because the
// legacy Windows console does not interpret them by default, this module
// also provides EnableColors() which turns on the
// ENABLE_VIRTUAL_TERMINAL_PROCESSING flag for the process console.
//
// Every function in this module prints only standard ASCII characters.
// =============================================================================

#ifndef AWAKE_CONSOLE_H
#define AWAKE_CONSOLE_H

namespace console {

// ANSI SGR escape sequences used by the tool.
extern const char* const kColorReset;  // reset all attributes
extern const char* const kColorRed;    // errors / watched app not running
extern const char* const kColorGreen;  // success / watched app running
extern const char* const kColorYellow; // warnings / neutral notices
extern const char* const kColorCyan;   // informational highlights / titles

// Enables ANSI escape sequence processing on the current console.
// It is safe to call this function multiple times. When the process is not
// attached to a console (for example the detached daemon), the call simply
// does nothing and never fails.
void EnableColors();

// Prints a single colored line, appending a trailing newline.
void PrintLine(const char* color, const char* text);

// Prints colored text using printf-style formatting (no trailing newline).
// Implemented with vprintf, so the usual format specifiers apply.
void Printf(const char* color, const char* fmt, ...);

} // namespace console

#endif // AWAKE_CONSOLE_H
