#pragma once

#include "app/screen_touch_overlay.h"
#include "app/ui.h"
#include "audio/audio_engine.h"
#include "base/config.h"
#include "disc/disc.h"
#include "input/input_manager.h"
#include "platform/display_insets.h"
#include "render/dynamic_scale.h"
#include "render/gpu_context.h"
#include "vfs/vfs.h"
#include "xr/xr_session.h"

#include <SDL3/SDL.h>

#include <memory>
#include <string>
#include <vector>

namespace gt3r {

class App;

class Screen {
public:
    virtual ~Screen() = default;
    virtual void update(App& app, double dt) = 0;
    virtual void render(App& app, Ui& ui) = 0;
    virtual void on_event(App&, const SDL_Event&) {}
    // Screens that consume raw input (rebinding) disable menu navigation.
    virtual bool wants_menu_input() const { return true; }
    // On-screen touch controls for this screen (phones/tablets): menu
    // navigation by default; screens that drive a car return Driving.
    virtual TouchLayout touch_layout() const { return TouchLayout::Menu; }
    // Our own frontend screens (setup, hub, settings) lay out to the whole
    // window instead of the letterboxed 640x360 space: Ui::width()/height()
    // vary, Ui::safe() is the area clear of display cutouts and rounded
    // corners, and Ui::touch() asks for finger-sized targets (see App::layout_ui).
    virtual bool adaptive_ui() const { return false; }
    // Screens that show the game's own 4:3 picture (its menus, races and movies) in the middle of
    // the 640x360 space. With the 4:3 aspect (App::four_three) the window shows only that 4:3
    // box (Ui::kFrame43X .. + kFrame43W); other screens keep the 16:9 space.
    virtual bool game_frame() const { return false; }
    // Game-frame screens whose picture can fill a 6:5 box too (the race: its 3D view and HUD
    // follow the box). Others keep the 4:3 box under the 6:5 aspect.
    virtual bool fills_frame() const { return false; }
    // The startup update dialog (app/updates.h) may open over this screen: the main menu, the
    // first-run screen and the game's title menu.
    virtual bool update_prompts() const { return false; }
};

class App {
public:
    bool init(int argc, char** argv);
    SDL_AppResult iterate();
    SDL_AppResult event(const SDL_Event& e);
    void shutdown();

    // Opens a disc image and remembers it in the config. Returns an error
    // message on failure, empty on success.
    std::string open_disc(const std::string& path);
    void close_disc();
    void show_open_dialog();
    // Opens `path` on the next frame, like a file picked in the dialog: on
    // success the screen stack restarts (hub + game start) for the new disc.
    void request_disc(const std::string& path) { set_dialog_result(path); }
    // [game] recent1..5, most recent first (see platform/recent_discs.h).
    std::vector<std::string> recent_discs() const;
    void forget_recent_disc(const std::string& path);
    // The path of the last disc image that failed to open (empty after a success).
    const std::string& failed_disc_path() const { return failed_disc_path_; }

    // Desktop window mode, remembered as [video] fullscreen (F11 toggles).
    bool fullscreen() const;
    void set_fullscreen(bool on);

    // [video] limit_60fps = on: on high-refresh displays draw about 60 frames a second (the game
    // runs at 60 Hz); steadier when the computer is busy. Off (default): every display refresh.
    bool limit_60fps() const;
    void set_limit_60fps(bool on);
    // [video] dynamic_resolution = on | off (default on phones and tablets): the 3D resolution
    // drops (to 50% at most) when the GPU cannot keep up and climbs while it does, up to [video]
    // render_scale, or with auto the screen's full resolution (render::DynamicScale). Fixed for
    // --shot and --render-scale runs.
    bool dynamic_resolution() const;
    void set_dynamic_resolution(bool on);
    // The dynamic scale now in use, or null when off.
    const render::DynamicScale* dynamic_scale() const { return dyn_on_ ? &dyn_ : nullptr; }
    // Races: the dynamic scale a race reaches is where the next launch starts ([video]
    // dynamic_start), so a phone that holds full resolution starts there and a slow one does
    // not start too high. Only with 3D resolution on Auto (a manual setting starts at its own
    // value); cleared when 3D resolution or dynamic resolution change. note_race_scale: every
    // race frame; remember_race_scale: when a race ends and when the app goes to the background.
    void note_race_scale();
    void remember_race_scale();
    // Applies [video] render_scale (after the setting changed).
    void apply_render_scale();
    // [video] aspect = auto | 16:9 | 4:3 | 6:5: the shape of the game area for
    // Screen::game_frame() screens. 16:9: the 640x360 space letterboxed to the window (as
    // before). 4:3: only its middle 4:3 box, so 4:3-ish screens (tablets, unfolded foldables)
    // are filled at their full resolution. 6:5: a 432x360 box for Screen::fills_frame()
    // screens (the race), the 4:3 box for the others. auto: 4:3 on windows narrower than 3:2,
    // otherwise 16:9.
    std::string aspect() const;
    void set_aspect(const std::string& v);
    // This frame shows a box narrower than 16:9 (4:3 or 6:5; set by layout_ui).
    bool four_three() const { return four_three_; }
    // The game area this frame shows, in UI units: the whole 640x360 space, or the middle box.
    SDL_FRect game_box() const { return {float(Ui::kWidth - box_w_) / 2, 0, float(box_w_), float(Ui::kHeight)}; }
    // [video] picture_top: with bars above and below the picture, both go below it.
    bool picture_top() const;
    void set_picture_top(bool on);
    // [video] hud_scale: the race HUD's size in percent (25..200, default 100).
    int hud_scale() const;
    void set_hud_scale(int percent);

    void push(std::unique_ptr<Screen> s) { pending_.push_back(std::move(s)); }
    void pop() { ++pops_; }
    void quit() { quit_ = true; }
    void save_config();

    Disc* disc() { return disc_.get(); }
    Vfs* vfs() { return vfs_.get(); }
    InputManager& input() { return *input_; }
    AudioEngine& audio() { return *audio_; }
    IniDocument& config() { return config_; }
    TouchOverlay& touch() { return touch_; }
    const MenuInput& menu() const { return menu_; }
    MenuInput& menu() { return menu_; }
    SDL_Renderer* renderer() { return renderer_; }
    SDL_Window* window() { return window_; }
    // Frames drawn so far (counted after each one).
    int frame() const { return frame_; }
    // Shared SDL_GPU device for 3D views; nullptr when running 2D-only.
    render::GpuContext* gpu() { return gpu_.get(); }
#if GT3R_XR
    // The VR headset session (docs/dev/VR.md); nullptr without a headset. The 2D renderer then
    // draws into its canvases: screens may draw a stereo frame while driving.
    xr::Session* xr() { return xr_.get(); }
    void set_xr_settings(const xr::Settings& s);
#endif
    const std::string& data_dir() const { return data_dir_; }
    std::string mods_dir() const { return data_dir_ + "mods"; }
    // Set by the async file dialog callback; consumed on the main thread.
    void set_dialog_result(std::string path);
    const std::string& last_error() const { return last_error_; }

private:
    void apply_screen_changes();
    void save_screenshot(const std::string& path);
    void push_game_start(); // boot screen: opening movie, then the title menu
    // Sets the renderer's logical presentation and the Ui layout for the frame.
    void layout_ui(bool adaptive, bool game_frame, bool fills_frame = false);
    // Window area (window coordinates) clear of display cutouts and, on
    // phones, the rounded screen corners.
    SDL_FRect safe_area_window() const;
    void draw_phone_mask(); // GT3R_PHONE: cutout and rounded corners, for screenshots
    void open_saved_disc(const std::string& saved); // [game] disc, with fallbacks for old relative paths

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;     // owned by gpu_ when it exists
    std::unique_ptr<render::GpuContext> gpu_;
#if GT3R_XR
    std::unique_ptr<xr::Session> xr_;
    s64 xr_last_display_ = 0; // previous frame's display time (frame step)
    // The right controller's pointer on the virtual screen: mouse events to the screens.
    void xr_pointer();
    bool xr_pointer_on_ = false, xr_pointer_down_ = false;
    float xr_pointer_x_ = 0, xr_pointer_y_ = 0;
#endif
    std::unique_ptr<Ui> ui_;
    std::unique_ptr<InputManager> input_;
    std::unique_ptr<AudioEngine> audio_;
    std::unique_ptr<Disc> disc_;
    std::unique_ptr<Vfs> vfs_;
    IniDocument config_;
    std::string data_dir_;
    std::string config_path_;
    MenuInput menu_;
    TouchOverlay touch_;
    std::vector<std::unique_ptr<Screen>> screens_;
    std::vector<std::unique_ptr<Screen>> pending_;
    int pops_ = 0;
    bool quit_ = false;
    Uint64 last_ticks_ = 0;

    SDL_Mutex* dialog_mutex_ = nullptr;
    std::string dialog_result_;
    std::string last_error_;
    std::string failed_disc_path_;

    bool screenshot_requested_ = false;
    std::string auto_shot_path_; // --shot: capture after a few frames, then exit
    int frame_ = 0;
    // 3D resolution: [video] render_scale (or --render-scale) and the dynamic scale under it.
    float configured_scale_ = 1;
    bool scale_auto_ = true; // [video] render_scale = auto (no --render-scale)
    bool render_scale_arg_ = false;
    render::DynamicScale dyn_;
    bool dyn_on_ = false;
    void reset_dynamic_scale();
    float race_scale_ = 0; // note_race_scale: the dynamic scale in the last race frame (0: none)
    static double dyn_cycle_s();
    // The main loop rate set by App::apply_frame_limit (0: as fast as vsync allows).
    double frame_limit_rate_ = 0;
    void apply_frame_limit();
    bool four_three_ = false; // this frame shows the 4:3 box ([video] aspect)
    int box_w_ = Ui::kWidth;  // its width in UI units (game_box)
    bool touch_ui_ = false;  // phone/tablet sizing (mobile builds, or GT3R_PHONE)
    bool phone_sim_ = false; // GT3R_PHONE=1: desktop window shaped like a 20:9 phone
    std::optional<platform::DisplayInsets> display_insets_; // phones: cutout and corner radius (pixels)
    int insets_retry_ = 0;                                  // frames until the next query (0: now)
};

// Screen factories (one file each).
std::unique_ptr<Screen> make_setup_screen();
std::unique_ptr<Screen> make_hub_screen();       // home menu under the game
std::unique_ptr<Screen> make_developer_screen(); // Developer options (tools)
std::unique_ptr<Screen> make_browser_screen(const std::string& open_path = "");
std::unique_ptr<Screen> make_jukebox_screen();
std::unique_ptr<Screen> make_input_screen();
std::unique_ptr<Screen> make_mods_screen();
// Settings; `tab` picks the first page shown (SettingsTab order: 0 Display .. 5 Game disc).
std::unique_ptr<Screen> make_settings_screen(int tab = 0);
constexpr int kSettingsTabDisc = 5;

} // namespace gt3r
