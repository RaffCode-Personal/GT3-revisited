#include "input/touch_controls.h"

#include "base/config.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace gt3r {

namespace {

// UI logical resolution (Ui::kWidth/kHeight; src/app is not visible here).
constexpr float kW = 640.0f;
constexpr float kH = 360.0f;
constexpr float kMargin = 14.0f;
constexpr float kGap = 10.0f;
constexpr float kHitScale = 1.2f; // circles take touches out to 1.2 x their radius: fingers are bigger

float parse_float(const std::optional<std::string>& s, float def, float lo, float hi) {
    if (!s) return def;
    char* end = nullptr;
    float v = std::strtof(s->c_str(), &end);
    if (end == s->c_str()) return def;
    return std::clamp(v, lo, hi);
}

std::string format_float(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", v);
    return buf;
}

void press(PadState& pad, PadButton b, float v) {
    size_t i = static_cast<size_t>(b);
    pad.buttons = u16(pad.buttons | (1u << i));
    u8 p = is_pressure_sensitive(b) ? u8(std::clamp(std::lround(v * 255.0f), 1L, 255L)) : u8(255);
    pad.pressure[i] = std::max(pad.pressure[i], p);
}

} // namespace

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

TouchConfig TouchConfig::load(const IniDocument& ini) {
    TouchConfig c;
    auto get = [&](const char* key) { return ini.get("touch", key); };
    if (auto v = get("mode")) {
        std::string m = to_lower(*v);
        c.mode = m == "on" || m == "1" ? Mode::On : m == "off" || m == "0" ? Mode::Off : Mode::Auto;
    }
    if (auto v = get("steering")) c.steering = to_lower(*v) == "tilt" ? Steering::Tilt : Steering::Drag;
    if (auto v = get("pedals")) {
        std::string m = to_lower(*v);
        c.pedals = m == "area" ? Pedals::Area : m == "full" ? Pedals::Full : Pedals::Position;
    }
    c.pedal_min = parse_float(get("pedal_min"), c.pedal_min, 0.0f, 1.0f);
    c.steer_range = parse_float(get("steer_range"), c.steer_range, 0.03f, 0.5f);
    c.tilt_deadzone = parse_float(get("tilt_deadzone"), c.tilt_deadzone, 0.0f, 0.5f);
    c.opacity = parse_float(get("opacity"), c.opacity, 0.05f, 1.0f);
    c.size = parse_float(get("size"), c.size, 0.6f, 1.6f);
    if (auto v = get("left_handed")) c.left_handed = to_lower(*v) == "on" || *v == "1";
    return c;
}

void TouchConfig::save(IniDocument& ini) const {
    ini.set("touch", "mode", mode == Mode::On ? "on" : mode == Mode::Off ? "off" : "auto");
    ini.set("touch", "steering", steering == Steering::Tilt ? "tilt" : "drag");
    ini.set("touch", "pedals", pedals == Pedals::Area ? "area" : pedals == Pedals::Full ? "full" : "position");
    ini.set("touch", "pedal_min", format_float(pedal_min));
    ini.set("touch", "steer_range", format_float(steer_range));
    ini.set("touch", "tilt_deadzone", format_float(tilt_deadzone));
    ini.set("touch", "opacity", format_float(opacity));
    ini.set("touch", "size", format_float(size));
    ini.set("touch", "left_handed", left_handed ? "on" : "off");
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

void TouchControls::configure(const TouchConfig& cfg) {
    cfg_ = cfg;
    release_all();
    build_layout();
}

void TouchControls::set_layout(TouchLayout layout) {
    if (layout == layout_) return;
    layout_ = layout;
    release_all(); // never carry a held control into another context
    build_layout();
}

void TouchControls::build_layout() {
    widgets_.clear();
    const float s = cfg_.size;
    const bool mirror = cfg_.left_handed;
    // Anchor to the whole screen, not just the 16:9 game area, so wide phones
    // get the controls in the side bars and tablets in the top/bottom bars.
    const float L = screen_x0_, R = screen_x1_, T = screen_y0_, B = screen_y1_, cx_mid = (L + R) / 2;
    // Edges of the area the controls sit in: kMargin from the screen edge, or
    // clear of a cutout / rounded corner inset.
    // The layout is built right-handed and mirrored afterwards, so mirrored
    // layouts swap the left and right insets.
    const float il = mirror ? inset_r_ : inset_l_, ir = mirror ? inset_l_ : inset_r_;
    const float mL = L + std::max(kMargin, il + 4), mR = R - std::max(kMargin, ir + 4);
    const float mT = T + std::max(kMargin, inset_t_ + 4), mB = B - std::max(kMargin, inset_b_ + 4);
    auto add = [&](Widget w) {
        if (mirror) w.x = w.shape == Widget::Shape::Circle ? L + R - w.x : L + R - w.x - w.w;
        widgets_.push_back(std::move(w));
    };
    auto button = [&](float cx, float cy, float r, PadButton b, std::string label) {
        Widget w;
        w.kind = Widget::Kind::Button;
        w.shape = Widget::Shape::Circle;
        w.x = cx, w.y = cy, w.w = r, w.h = r;
        w.button = b;
        w.label = std::move(label);
        return w;
    };
    auto rect_button = [&](float x, float y, float w_, float h, PadButton b, std::string label) {
        Widget w = button(0, 0, 0, b, std::move(label));
        w.shape = Widget::Shape::Rect;
        w.x = x, w.y = y, w.w = w_, w.h = h;
        return w;
    };

    if (layout_ == TouchLayout::Driving) {
        // Steering zone first: everything after it takes priority in widget_at.
        Widget steer;
        steer.kind = Widget::Kind::Steering;
        steer.shape = Widget::Shape::Rect;
        steer.x = L, steer.y = kH * 0.22f, steer.w = cx_mid - L, steer.h = B - steer.y;
        add(steer);

        const float pw = 62 * s, ph = 132 * s;
        Widget gas;
        gas.kind = Widget::Kind::Pedal;
        gas.shape = Widget::Shape::Rect;
        gas.x = mR - pw, gas.y = mB - ph, gas.w = pw, gas.h = ph;
        gas.button = PadButton::Cross;
        gas.label = "GAS";
        Widget brake = gas;
        brake.x = gas.x - kGap - pw;
        brake.button = PadButton::Square;
        brake.label = "BRAKE";
        add(gas);
        add(brake);

        const float r = 22 * s;
        // The race pad's buttons in GT3's meaning (race/controls.h), whatever the race layout.
        add(button(gas.x + pw / 2, gas.y - kGap - r, r, PadButton::R2, "UP"));
        add(button(brake.x + pw / 2, brake.y - kGap - r, r, PadButton::L2, "DOWN"));
        const float hb = 18 * s;
        add(button(brake.x - kGap - hb, mB - hb, hb, PadButton::Circle, "HB"));

        const float sr = 15 * s;
        add(button(cx_mid, mT + sr, sr, PadButton::Start, "||"));
        add(button(cx_mid + 2.6f * sr, mT + sr, sr, PadButton::R1, "CAM"));
        add(button(cx_mid - 2.6f * sr, mT + sr, sr, PadButton::Select, "RST"));
    } else if (layout_ == TouchLayout::Menu) {
        const float half = 54 * s;
        Widget pad;
        pad.kind = Widget::Kind::DPad;
        pad.shape = Widget::Shape::Rect;
        pad.x = mL, pad.y = mB - 2 * half, pad.w = 2 * half, pad.h = 2 * half;
        add(pad);

        const float r = 19 * s, off = 34 * s;
        const float cx = mR - half, cy = mB - half;
        add(button(cx, cy + off, r, PadButton::Cross, ""));
        add(button(cx + off, cy, r, PadButton::Circle, ""));
        add(button(cx - off, cy, r, PadButton::Square, ""));
        add(button(cx, cy - off, r, PadButton::Triangle, ""));

        const float bw = 40 * s, bh = 20 * s;
        add(rect_button(mL, mT, bw, bh, PadButton::L1, "L1"));
        add(rect_button(mR - bw, mT, bw, bh, PadButton::R1, "R1"));
        add(rect_button(cx_mid - bw - 4, mT, bw, bh, PadButton::Select, "SEL"));
        add(rect_button(cx_mid + 4, mT, bw, bh, PadButton::Start, "START"));
    }
}

void TouchControls::set_screen(float x0, float y0, float x1, float y1, float area_x0, float area_x1) {
    // Never smaller than the game area; ignore sub-unit jitter.
    x0 = std::min(x0, area_x0), y0 = std::min(y0, 0.0f), x1 = std::max(x1, area_x1), y1 = std::max(y1, kH);
    if (std::fabs(x0 - screen_x0_) < 0.5f && std::fabs(y0 - screen_y0_) < 0.5f && std::fabs(x1 - screen_x1_) < 0.5f &&
        std::fabs(y1 - screen_y1_) < 0.5f)
        return;
    screen_x0_ = x0, screen_y0_ = y0, screen_x1_ = x1, screen_y1_ = y1;
    // Same widgets in the same order, so held fingers stay valid.
    build_layout();
}

void TouchControls::set_insets(float left, float top, float right, float bottom) {
    left = std::max(left, 0.0f), top = std::max(top, 0.0f), right = std::max(right, 0.0f), bottom = std::max(bottom, 0.0f);
    if (std::fabs(left - inset_l_) < 0.5f && std::fabs(top - inset_t_) < 0.5f && std::fabs(right - inset_r_) < 0.5f &&
        std::fabs(bottom - inset_b_) < 0.5f)
        return;
    inset_l_ = left, inset_t_ = top, inset_r_ = right, inset_b_ = bottom;
    build_layout(); // same widgets in the same order: held fingers stay valid
}

std::vector<TouchRect> TouchControls::control_bounds() const {
    std::vector<TouchRect> out;
    if (!visible()) return out;
    for (const Widget& w : widgets_) {
        if (w.kind == Widget::Kind::Steering) continue; // invisible zone, only a hint
        if (w.shape == Widget::Shape::Circle) out.push_back({w.x - w.w, w.y - w.h, 2 * w.w, 2 * w.h});
        else out.push_back({w.x, w.y, w.w, w.h});
    }
    return out;
}

TouchRect TouchControls::safe_area() const {
    TouchRect full{0, 0, kW, kH};
    if (!visible()) return full;
    float left = 0, right = kW, top = 0;
    for (const TouchRect& b : control_bounds()) {
        float x0 = b.x, x1 = b.x + b.w, y0 = b.y, y1 = b.y + b.h;
        float cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
        if (cy < kH * 0.25f) top = std::max(top, y1);
        else if (cx < kW / 2) left = std::max(left, x1);
        else right = std::min(right, x0);
    }
    left = std::clamp(left, 0.0f, kW / 2), right = std::clamp(right, kW / 2, kW), top = std::clamp(top, 0.0f, kH / 2);
    return {left, top, right - left, kH - top};
}

TouchRect TouchControls::clear_above(TouchRect r) const {
    if (!visible()) return r;
    // Each move goes up, so once above a control `r` stays above it.
    for (bool moved = true; moved;) {
        moved = false;
        for (const Widget& w : widgets_) {
            if (w.kind == Widget::Kind::Steering) continue;
            const bool round = w.shape == Widget::Shape::Circle;
            const float hr = w.w * kHitScale;
            const float x0 = round ? w.x - hr : w.x, x1 = round ? w.x + hr : w.x + w.w;
            const float y0 = round ? w.y - hr : w.y, y1 = round ? w.y + hr : w.y + w.h;
            if (r.x < x1 && x0 < r.x + r.w && r.y < y1 && y0 < r.y + r.h) {
                r.y = y0 - kGap - r.h;
                moved = true;
            }
        }
    }
    return r;
}

int TouchControls::widget_at(float x, float y) const {
    for (int i = int(widgets_.size()) - 1; i >= 0; --i) {
        const Widget& w = widgets_[size_t(i)];
        if (w.shape == Widget::Shape::Circle) {
            float r = w.w * kHitScale;
            float dx = x - w.x, dy = y - w.y;
            if (dx * dx + dy * dy <= r * r) return i;
        } else if (x >= w.x && x < w.x + w.w && y >= w.y && y < w.y + w.h) {
            return i;
        }
    }
    return -1;
}

bool TouchControls::hit(float x, float y) const { return visible() && widget_at(x, y) >= 0; }

int TouchControls::dpad_direction(const Widget& w, float x, float y) const {
    float dx = x - (w.x + w.w / 2), dy = y - (w.y + w.h / 2);
    float dead = w.w * 0.12f;
    if (std::fabs(dx) < dead && std::fabs(dy) < dead) return -1;
    if (std::fabs(dx) > std::fabs(dy)) return int(dx > 0 ? PadButton::Right : PadButton::Left);
    return int(dy > 0 ? PadButton::Down : PadButton::Up);
}

float TouchControls::pedal_value(const Widget& w, const Finger& f) const {
    switch (cfg_.pedals) {
    case TouchConfig::Pedals::Full: return 1.0f;
    case TouchConfig::Pedals::Area:
        // Touchscreens without pressure report a constant 1 (or 0): full.
        if (f.pressure <= 0.0f || f.pressure >= 1.0f) return 1.0f;
        return std::max(cfg_.pedal_min, f.pressure);
    case TouchConfig::Pedals::Position: break;
    }
    // Bottom of the pedal = pedal_min, top 20 % = full.
    float t = std::clamp((w.y + w.h - f.y) / (w.h * 0.8f), 0.0f, 1.0f);
    return cfg_.pedal_min + (1.0f - cfg_.pedal_min) * t;
}

// ---------------------------------------------------------------------------
// Fingers
// ---------------------------------------------------------------------------

TouchControls::Finger* TouchControls::find(u64 id) {
    for (Finger& f : fingers_)
        if (f.id == id) return &f;
    return nullptr;
}

bool TouchControls::finger_down(u64 id, float x, float y, float pressure) {
    note_touch();
    if (!visible()) return false;
    std::erase_if(fingers_, [&](const Finger& f) { return f.id == id; });
    int w = widget_at(x, y);
    if (w < 0) return false;
    Finger f;
    f.id = id;
    f.widget = w;
    f.x = f.origin_x = x;
    f.y = f.origin_y = y;
    f.pressure = pressure;
    fingers_.push_back(f);
    return true;
}

void TouchControls::finger_move(u64 id, float x, float y, float pressure) {
    Finger* f = find(id);
    if (!f) return;
    f->x = x, f->y = y, f->pressure = pressure;
    // Buttons and pedals follow a sliding finger (brake -> gas, D-pad -> face
    // buttons); the steering zone keeps its finger.
    if (widgets_[size_t(f->widget)].kind == Widget::Kind::Steering) return;
    int w = widget_at(x, y);
    if (w >= 0 && widgets_[size_t(w)].kind != Widget::Kind::Steering) f->widget = w;
}

void TouchControls::finger_up(u64 id) {
    // A tap shorter than a frame must still register as one press.
    if (Finger* f = find(id); f && !f->seen) {
        f->released = true;
        return;
    }
    std::erase_if(fingers_, [&](const Finger& f) { return f.id == id; });
}

void TouchControls::release_all() {
    fingers_.clear();
    pad_ = PadState{};
}

void TouchControls::note_other_input() {
    if (!touch_active_) return;
    touch_active_ = false;
    release_all();
}

bool TouchControls::visible() const {
    if (layout_ == TouchLayout::None || cfg_.mode == TouchConfig::Mode::Off) return false;
    return cfg_.mode == TouchConfig::Mode::On || touch_active_;
}

// ---------------------------------------------------------------------------
// Pad output
// ---------------------------------------------------------------------------

void TouchControls::update(double, float tilt) {
    pad_ = PadState{};
    for (Widget& w : widgets_) {
        w.active = false;
        w.value = 0.0f;
    }
    if (!visible()) {
        fingers_.clear();
        return;
    }
    pad_.connected = true;
    float lx = 0.0f;
    for (const Finger& f : fingers_) {
        Widget& w = widgets_[size_t(f.widget)];
        switch (w.kind) {
        case Widget::Kind::Button:
            press(pad_, w.button, 1.0f);
            w.active = true;
            break;
        case Widget::Kind::Pedal: {
            float v = pedal_value(w, f);
            press(pad_, w.button, v);
            w.active = true;
            w.value = std::max(w.value, v);
            break;
        }
        case Widget::Kind::DPad:
            if (int d = dpad_direction(w, f.x, f.y); d >= 0) {
                press(pad_, PadButton(d), 1.0f);
                w.active = true;
                w.button = PadButton(d);
            }
            break;
        case Widget::Kind::Steering:
            if (cfg_.steering != TouchConfig::Steering::Drag) break;
            {
                float range = cfg_.steer_range * kW;
                lx = std::clamp((f.x - f.origin_x) / range, -1.0f, 1.0f);
                w.active = true;
                w.value = lx;
                w.origin_x = f.origin_x;
                w.origin_y = f.origin_y;
                w.knob_x = f.origin_x + lx * range;
            }
            break;
        }
    }
    if (layout_ == TouchLayout::Driving && cfg_.steering == TouchConfig::Steering::Tilt) {
        float m = std::fabs(tilt);
        float dz = cfg_.tilt_deadzone;
        m = m <= dz ? 0.0f : std::min((m - dz) / (1.0f - dz), 1.0f);
        lx = std::copysign(m, tilt);
        for (Widget& w : widgets_)
            if (w.kind == Widget::Kind::Steering) w.value = lx;
    }
    pad_.axes[size_t(PadAxis::LX)] = u8(std::lround((std::clamp(lx, -1.0f, 1.0f) + 1.0f) * 127.5f));
    std::erase_if(fingers_, [](const Finger& f) { return f.released; });
    for (Finger& f : fingers_) f.seen = true;
}

} // namespace gt3r
