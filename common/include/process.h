// =============================================================================
// process.h - helpers for enumerating running processes.
//
// Used by two callers:
//   - the daemon, to decide whether a watched application currently runs,
//   - the client, to color the entries of "awake daemon status".
// =============================================================================

#ifndef AWAKE_PROCESS_H
#define AWAKE_PROCESS_H

#include <set>
#include <string>
#include <vector>

namespace process {

// Takes one snapshot of the running processes and returns the subset of the
// given image names that are currently running. Both the input names and the
// returned names are compared/matched case-insensitively; the returned set
// contains the lower case forms of the matched names.
std::set<std::string> FindRunningImages(const std::vector<std::string>& imageNames);

// Convenience wrapper around FindRunningImages() for a single name.
bool IsImageRunning(const std::string& imageName);

} // namespace process

#endif // AWAKE_PROCESS_H
