// =============================================================================
// config.h - reading and writing of the awake.ini configuration file.
//
// The configuration file (awake.ini) is stored in the same directory as the
// executable itself, which keeps the tool fully portable. It uses a very
// small INI-like subset:
//
//   - Lines starting with '#' or ';' are comments and are ignored.
//   - Empty lines are ignored.
//   - Section headers such as [watch] are ignored (kept for readability).
//   - Every other non-empty line holds one image name (for example
//     "notepad.exe") that the daemon watches.
//
// When the file does not exist it is created automatically with a comment
// header explaining the format.
// =============================================================================

#ifndef AWAKE_CONFIG_H
#define AWAKE_CONFIG_H

#include <string>
#include <vector>

namespace config {

// Returns the absolute path of the configuration file:
// "<directory of the running executable>\awake.ini".
std::string GetConfigPath();

// Returns the directory that contains the running executable.
std::string GetExeDirectory();

// Creates the configuration file with a descriptive comment header if it
// does not exist yet. Does nothing when the file is already present.
// Returns true when the file exists afterwards.
bool EnsureConfigExists();

// Overwrites the configuration file with the initial template content,
// discarding every existing watch list entry. Manual edits and comments
// are not preserved. Returns false when the file cannot be written.
bool ResetConfig();

// Reads the watch list (the image names currently configured).
// Comment lines, empty lines and section headers are skipped.
std::vector<std::string> ReadWatchList();

// Appends one image name to the watch list. The entry is validated with
// IsValidImageName() first and duplicates (case-insensitive) are rejected.
// Returns false when the name is invalid or already present.
bool AddWatchEntry(const std::string& imageName);

// Removes one image name (case-insensitive) from the watch list. All other
// lines, including comments, are preserved. Returns false when the name is
// not present in the list.
bool RemoveWatchEntry(const std::string& imageName);

// Validates a user supplied image name: it must be a plain file name
// (no path separators, no wildcards, printable ASCII, reasonable length).
bool IsValidImageName(const std::string& imageName);

// Converts a string to lower case (ASCII only). Used for case-insensitive
// comparisons of image names.
std::string ToLowerAscii(const std::string& text);

} // namespace config

#endif // AWAKE_CONFIG_H
