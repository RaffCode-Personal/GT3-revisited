#pragma once

#include "input/virtual_pad.h"

#include <string>
#include <vector>

namespace gt3r {

class IniDocument;

// On-screen touch controls for phones and tablets without a controller.
//
// TouchControls is pure logic: it tracks fingers in UI coordinates
// (Ui::kWidth x Ui::kHeight, the same space screens draw in) and produces a
// virtual DualShock 2 state that InputManager merges into player 1 (see
// InputManager::set_overlay). Drawing lives in src/app/screen_touch_overlay.*.
//
// Configuration, [touch] in config.ini (see docs/INPUT.md):
//   mode       = auto      ; auto: shown after a touch, hidden on pad/keyboard input; on; off
//   steering   = drag      ; drag (left half of the screen) or tilt (device accelerometer)
//   pedals     = position  ; position: higher on the pedal = more pressure; area: touch
//                          ; pressure/size from the screen; full: always 100 %
//   pedal_min  = 0.35      ; pressure at the bottom of a pedal (position mode)
//   steer_range = 0.14     ; drag distance for full lock, as a fraction of screen width
//   tilt_deadzone = 0.04   ; tilt steering deadzone (fraction of full lock)
//   opacity    = 0.5       ; 0..1
//   size       = 1.0       ; control size multiplier (0.6 .. 1.6)
//   left_handed = off      ; mirror the layout (pedals on the left)
enum class TouchLayout : u8 { None, Menu, Driving };

// Rectangle in UI units (Ui::kWidth x Ui::kHeight space).
struct TouchRect {
    float x = 0, y = 0, w = 0, h = 0;
};

struct TouchConfig {
    enum class Mode : u8 { Auto, On, Off };
    enum class Steering : u8 { Drag, Tilt };
    enum class Pedals : u8 { Position, Area, Full };

    Mode mode = Mode::Auto;
    Steering steering = Steering::Drag;
    Pedals pedals = Pedals::Position;
    float pedal_min = 0.35f;
    float steer_range = 0.14f;
    float tilt_deadzone = 0.04f;
    float opacity = 0.5f;
    float size = 1.0f;
    bool left_handed = false;

    static TouchConfig load(const IniDocument& ini);
    void save(IniDocument& ini) const;
};

class TouchControls {
public:
    // What the overlay draws. Coordinates are UI units; circles use (x, y) as
    // the centre and w as the radius.
    struct Widget {
        enum class Kind : u8 { Button, DPad, Pedal, Steering };
        enum class Shape : u8 { Rect, Circle };
        Kind kind = Kind::Button;
        Shape shape = Shape::Circle;
        float x = 0, y = 0, w = 0, h = 0;
        PadButton button = PadButton::Cross; // Button / Pedal
        std::string label;                   // short text; empty for face-button symbols
        bool active = false;                 // currently held
        float value = 0.0f;                  // pedal pressure / steering -1..1
        // Steering: where the finger went down (knob origin), valid while active.
        float origin_x = 0, origin_y = 0, knob_x = 0;
    };

    void configure(const TouchConfig& cfg);
    const TouchConfig& config() const { return cfg_; }
    void set_layout(TouchLayout layout);
    // The whole window in UI units; extends past 0..kWidth / 0..kHeight where
    // the game area is letterboxed. Controls anchor to its edges, so they sit
    // in the side bars of wide phones. Default: the game area itself.
    // `area_x0` .. `area_x1`: the part of 0..kWidth the window shows across (all of it, or the
    // 4:3 box with the 4:3 aspect, App::four_three); the screen is never narrower.
    void set_screen(float x0, float y0, float x1, float y1, float area_x0 = 0, float area_x1 = 640);
    // Parts of the screen edges (UI units) that controls must stay clear of:
    // display cutouts and rounded corners. Controls keep at least their
    // normal margin from the screen edge, or this inset plus a little.
    void set_insets(float left, float top, float right, float bottom);
    // Part of the game area (0,0)-(kWidth,kHeight) not covered by visible
    // controls: HUD elements inside it stay clear of the thumbs. The full
    // area when the controls are hidden. The steering zone doesn't count.
    TouchRect safe_area() const;
    // The visible controls' drawn bounds (UI units), the steering zone left
    // out; empty while the controls are hidden.
    std::vector<TouchRect> control_bounds() const;
    // `r` (UI units) moved up until it is clear of every visible control's
    // touch area, so a button a screen draws there can still be tapped.
    // Unchanged while the controls are hidden. The steering zone doesn't count.
    TouchRect clear_above(TouchRect r) const;
    TouchLayout layout() const { return layout_; }

    // Finger input in UI coordinates. `pressure` is 0..1 as reported by the
    // touchscreen. Returns true when the finger landed on a control (the
    // caller should then not pass the touch on to the screen).
    bool finger_down(u64 id, float x, float y, float pressure);
    void finger_move(u64 id, float x, float y, float pressure);
    void finger_up(u64 id);
    void release_all();
    // True if (x, y) is on a control of the current layout (used to swallow
    // the mouse events SDL synthesizes from touches).
    bool hit(float x, float y) const;

    // Visibility in auto mode: a touch shows the controls, gamepad or
    // keyboard input hides them.
    void note_touch() { touch_active_ = true; }
    void note_other_input();
    bool visible() const;

    // `tilt` is the device roll, -1..1 = full lock (InputManager::tilt).
    void update(double dt, float tilt);
    // Contribution to player 1's pad; neutral when hidden.
    const PadState& pad() const { return pad_; }
    const std::vector<Widget>& widgets() const { return widgets_; }

private:
    struct Finger {
        u64 id = 0;
        int widget = -1;
        float x = 0, y = 0, pressure = 1.0f;
        float origin_x = 0, origin_y = 0; // steering: touch-down point
        bool seen = false;     // reported by at least one update()
        bool released = false; // lifted before any update() saw it: report once, then drop
    };

    void build_layout();
    int widget_at(float x, float y) const;
    Finger* find(u64 id);
    float pedal_value(const Widget& w, const Finger& f) const;
    int dpad_direction(const Widget& w, float x, float y) const; // -1 none, else PadButton

    TouchConfig cfg_;
    float screen_x0_ = 0, screen_y0_ = 0, screen_x1_ = 640, screen_y1_ = 360;
    float inset_l_ = 0, inset_t_ = 0, inset_r_ = 0, inset_b_ = 0;
    TouchLayout layout_ = TouchLayout::None;
    bool touch_active_ = false;
    std::vector<Widget> widgets_;
    std::vector<Finger> fingers_;
    PadState pad_;
};

} // namespace gt3r
