#pragma once

// The GT-mode save library (docs/dev/GT_MODE.md "Saves"): any number of native saves
// (gt_save.h text files, "<name>.sav") in one folder, <user data>/saves/, and which of
// them is the game being played (current.txt in that folder holds its file name). The
// menus' save page (menu/flow/save_page.h) lists them; the game autosaves into the
// current one.
//
// A save's name is its GameState::file_comment (the original's file name, shown and
// edited in File Operation) or, when that is empty, its file name without ".sav". File
// names follow the names (sanitised for every file system, made unique with " 2", " 3").
// No operation writes over another file: new files take free names, and an unreadable save
// is listed (state nullopt) but never replaced; when it is the current one, the session
// moves it aside before playing a new game in its place (GtModeSession::load).
//
// Earlier versions kept one save, <user data>/gtmode.sav, and a second "memory card
// slot" next to it, gtmode_2.sav: migrate() copies them into the library the first time
// it is created (the originals stay where they are).

#include "game/gt_mode.h"

#include <optional>
#include <string>
#include <vector>

namespace gt3r::game {

struct SaveInfo {
    std::string path;               // the file
    std::string name;               // display name
    s64 mtime = 0;                  // last written (seconds since 1970)
    std::optional<GameState> state; // nullopt: unreadable (`error`)
    std::string error;
};

class SaveLibrary {
public:
    // `dir`: the library folder (created on the first write). `tracked`: whether the
    // current save is remembered in current.txt (off for GT3R_GTMODE_SAVE).
    explicit SaveLibrary(std::string dir, bool tracked = true);
    // <user data>/saves/ (the user data folder: SDL_GetPrefPath, or the executable's
    // folder in portable mode), or the folder set_user_dir chose.
    static std::string user_dir();
    // The folder user_dir() returns from now on ("" = <user data>/saves/). The app sets it
    // from [storage] saves_dir (Settings: a folder the player can reach, e.g. Android's
    // shared storage, which an uninstall leaves alone).
    static void set_user_dir(std::string dir);
    // The single save of earlier versions: <user data>/gtmode.sav.
    static std::string legacy_path();

    const std::string& dir() const { return dir_; }
    // First run: when the library folder does not exist yet it is created and `legacy`
    // and its second slot (<legacy stem>_2.sav) are copied in (the first one becomes the
    // current save). Returns the number of saves copied.
    int migrate(const std::string& legacy);

    // Copies in the saves of the folder `from` this library does not have: each *.sav whose
    // file name is free here, and current.txt when this library has none. Nothing is
    // overwritten or deleted. Returns the number of saves copied.
    int import_missing(const std::string& from);

    // Every *.sav in the folder, newest first.
    std::vector<SaveInfo> list() const;
    // The current save's path ("" when none is recorded or its file is gone).
    std::string current() const;
    bool set_current(const std::string& path);
    bool tracked() const { return tracked_; }

    // A name no save in the library has: `base` (cut to `max` characters) or
    // "<base> 2", "<base> 3"... `except` (a path) does not count.
    std::string unique_name(const std::string& base, size_t max = 16, const std::string& except = {}) const;
    // A free file path for a save called `name` (`except`: a path that may be reused).
    std::string path_for(const std::string& name, const std::string& except = {}) const;

    // Writes a new save called `name` (file_comment = name) holding `s`; its path in `path`.
    bool create(GameState s, const std::string& name, std::string* path, std::string* error = nullptr);
    // Renames a save: its name is set to `name` and its file follows. `live`: the state to
    // write instead of the file's own (the game being played). The current save stays
    // current. New path in `new_path`.
    bool rename(const std::string& path, const std::string& name, std::string* new_path,
                const GameState* live = nullptr, std::string* error = nullptr);
    // A copy named "<name> 2" (...); its path in `new_path`.
    bool copy(const std::string& path, std::string* new_path, std::string* error = nullptr);
    bool remove(const std::string& path);

    static std::string display_name(const GameState& s, const std::string& path);
    // A name usable as a file name on every system ("" -> "save").
    static std::string file_stem(const std::string& name);

private:
    std::string dir_;
    bool tracked_ = true;
};

// A save the player picked anywhere: a native save, or a PS2 save in any of the
// containers import_ps2_save_path reads (a .psu or card image is also recognised by its
// contents, for paths without an extension such as Android's content:// URIs). Throws
// FormatError.
GameState read_any_save(const std::string& path, const GtData& data);

} // namespace gt3r::game
