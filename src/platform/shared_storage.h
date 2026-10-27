#pragma once

#include <string>

namespace gt3r::platform {

// A folder the player can reach outside the app's private storage, for saves that should
// outlive an uninstall or "Clear storage": Android's shared storage (<storage>/GT3Revisited/,
// what a file manager or a computer over USB shows as the phone's internal storage). Writing
// there needs "All files access" (Android 11+) or the storage permission (earlier).

// Whether this platform has such a folder (Android only; desktop user data is reachable).
bool shared_storage_supported();
// The folder, with a trailing separator ("" when unsupported or not mounted).
std::string shared_storage_dir();
// Whether the app may write there now. Checked again on each call: the player can grant
// or revoke it in the system settings while the game runs.
bool shared_storage_access();
// Opens the system screen granting access (Android 11+: "All files access" for this app;
// earlier: the storage permission prompt). Returns at once; poll shared_storage_access().
void request_shared_storage_access();

} // namespace gt3r::platform
