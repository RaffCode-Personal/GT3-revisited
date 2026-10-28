#include "input/bindings.h"

#include "base/config.h"

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>

#include <algorithm>
#include <charconv>
#include <cstdlib>

namespace gt3r {

namespace {

struct ButtonAlias {
    const char* name;
    SDL_GamepadButton button;
};

// Positional names, so bindings mean the same on Xbox, PlayStation, Switch
// and generic pads. SDL's own names ("a", "dpup", ...) are accepted too.
constexpr ButtonAlias kButtonAliases[] = {
    {"south", SDL_GAMEPAD_BUTTON_SOUTH},    {"east", SDL_GAMEPAD_BUTTON_EAST},
    {"west", SDL_GAMEPAD_BUTTON_WEST},      {"north", SDL_GAMEPAD_BUTTON_NORTH},
    {"cross", SDL_GAMEPAD_BUTTON_SOUTH},    {"circle", SDL_GAMEPAD_BUTTON_EAST},
    {"square", SDL_GAMEPAD_BUTTON_WEST},    {"triangle", SDL_GAMEPAD_BUTTON_NORTH},
};

std::string with_underscores(std::string_view s) {
    std::string out(s);
    for (char& c : out)
        if (c == ' ') c = '_';
    return out;
}

std::string with_spaces(std::string_view s) {
    std::string out(s);
    for (char& c : out)
        if (c == '_') c = ' ';
    return out;
}

bool parse_int(std::string_view s, int& out) {
    auto r = std::from_chars(s.data(), s.data() + s.size(), out);
    return r.ec == std::errc() && r.ptr == s.data() + s.size();
}

// Splits a trailing '+' / '-' half selector.
InputSource::Half take_half(std::string_view& s) {
    if (!s.empty() && s.back() == '+') {
        s.remove_suffix(1);
        return InputSource::Half::Positive;
    }
    if (!s.empty() && s.back() == '-') {
        s.remove_suffix(1);
        return InputSource::Half::Negative;
    }
    return InputSource::Half::Full;
}

const char* half_suffix(InputSource::Half h) {
    return h == InputSource::Half::Positive ? "+" : h == InputSource::Half::Negative ? "-" : "";
}

constexpr const char* kHatDirs[] = {"up", "right", "down", "left"};

} // namespace

std::string InputSource::to_string() const {
    switch (kind) {
    case Kind::None: return "none";
    case Kind::Key: return "key." + with_underscores(SDL_GetScancodeName(static_cast<SDL_Scancode>(code)));
    case Kind::PadButton:
        for (const auto& a : kButtonAliases)
            if (a.button == code) return std::string("pad.") + a.name;
        return std::string("pad.") + SDL_GetGamepadStringForButton(static_cast<SDL_GamepadButton>(code));
    case Kind::PadAxis:
        return std::string("pad.") + SDL_GetGamepadStringForAxis(static_cast<SDL_GamepadAxis>(code)) + half_suffix(half);
    case Kind::JoyButton: return "joy.button" + std::to_string(code);
    case Kind::JoyAxis: return "joy.axis" + std::to_string(code) + half_suffix(half);
    case Kind::JoyHat: {
        std::string dir = "up";
        for (int i = 0; i < 4; ++i)
            if (hat_dir == (1 << i)) dir = kHatDirs[i];
        return "joy.hat" + std::to_string(code) + "." + dir;
    }
    case Kind::Tilt: return std::string("tilt") + half_suffix(half);
    case Kind::Mouse:
        return code == SDL_BUTTON_RIGHT ? "mouse.right" : code == SDL_BUTTON_MIDDLE ? "mouse.middle" : "mouse.left";
    }
    return "none";
}

bool InputSource::parse(std::string_view tok, InputSource& out) {
    out = InputSource{};
    std::string t = to_lower(tok);
    std::string_view s = t;
    if (s.rfind("key.", 0) == 0) {
        // Scancode names are case-insensitive in SDL; keep original case.
        SDL_Scancode sc = SDL_GetScancodeFromName(with_spaces(tok.substr(4)).c_str());
        if (sc == SDL_SCANCODE_UNKNOWN) return false;
        out.kind = Kind::Key;
        out.code = sc;
        return true;
    }
    if (s.rfind("pad.", 0) == 0) {
        s.remove_prefix(4);
        for (const auto& a : kButtonAliases)
            if (s == a.name) {
                out.kind = Kind::PadButton;
                out.code = a.button;
                return true;
            }
        std::string name(s);
        SDL_GamepadButton b = SDL_GetGamepadButtonFromString(name.c_str());
        if (b != SDL_GAMEPAD_BUTTON_INVALID) {
            out.kind = Kind::PadButton;
            out.code = b;
            return true;
        }
        out.half = take_half(s);
        name = std::string(s);
        SDL_GamepadAxis ax = SDL_GetGamepadAxisFromString(name.c_str());
        if (ax == SDL_GAMEPAD_AXIS_INVALID) return false;
        out.kind = Kind::PadAxis;
        out.code = ax;
        return true;
    }
    if (s.rfind("joy.button", 0) == 0) {
        out.kind = Kind::JoyButton;
        return parse_int(s.substr(10), out.code);
    }
    if (s.rfind("joy.axis", 0) == 0) {
        s.remove_prefix(8);
        out.half = take_half(s);
        out.kind = Kind::JoyAxis;
        return parse_int(s, out.code);
    }
    if (s.rfind("joy.hat", 0) == 0) {
        s.remove_prefix(7);
        size_t dot = s.find('.');
        if (dot == std::string_view::npos || !parse_int(s.substr(0, dot), out.code)) return false;
        std::string_view dir = s.substr(dot + 1);
        for (int i = 0; i < 4; ++i)
            if (dir == kHatDirs[i]) {
                out.kind = Kind::JoyHat;
                out.hat_dir = u8(1 << i); // matches SDL_HAT_UP/RIGHT/DOWN/LEFT
                return true;
            }
        return false;
    }
    if (s.rfind("tilt", 0) == 0) {
        s.remove_prefix(4);
        out.half = take_half(s);
        out.kind = Kind::Tilt;
        return s.empty();
    }
    if (s.rfind("mouse.", 0) == 0) {
        s.remove_prefix(6);
        out.kind = Kind::Mouse;
        out.code = s == "right" ? SDL_BUTTON_RIGHT : s == "middle" ? SDL_BUTTON_MIDDLE : SDL_BUTTON_LEFT;
        return s == "left" || s == "right" || s == "middle";
    }
    return false;
}

std::string PadTarget::to_string() const {
    if (kind == Kind::Button) return std::string(pad_button_name(button));
    if (kind == Kind::Axis) return std::string(pad_axis_name(axis)) + half_suffix(half);
    return "none";
}

bool PadTarget::parse(std::string_view tok, PadTarget& out) {
    out = PadTarget{};
    if (auto b = pad_button_from_name(tok)) {
        out.kind = Kind::Button;
        out.button = *b;
        return true;
    }
    out.half = take_half(tok);
    if (auto a = pad_axis_from_name(tok)) {
        out.kind = Kind::Axis;
        out.axis = *a;
        return true;
    }
    return false;
}

std::vector<Binding> Binding::parse_line(std::string_view target_tok, std::string_view sources) {
    std::vector<Binding> out;
    PadTarget target;
    if (!PadTarget::parse(trim(target_tok), target)) return out;
    for (std::string_view alt : split(sources, '|')) {
        Binding b;
        b.target = target;
        bool first = true;
        bool ok = true;
        for (std::string_view word : split(trim(alt), ' ')) {
            word = trim(word);
            if (word.empty()) continue;
            if (first) {
                ok = InputSource::parse(word, b.source);
                first = false;
                continue;
            }
            size_t eq = word.find('=');
            std::string key = to_lower(word.substr(0, eq));
            float v = eq == std::string_view::npos ? 1.0f : std::strtof(std::string(word.substr(eq + 1)).c_str(), nullptr);
            if (key == "deadzone") b.options.deadzone = v;
            else if (key == "saturation") b.options.saturation = v;
            else if (key == "curve") b.options.curve = v;
            else if (key == "scale") b.options.scale = v;
            else if (key == "ramp") b.options.ramp = v;
            else if (key == "threshold") b.options.threshold = v;
            else if (key == "invert") b.options.invert = v != 0.0f;
            else if (key == "pedal") b.options.pedal = v != 0.0f;
        }
        if (ok && !first) out.push_back(b);
    }
    return out;
}

std::string Binding::source_with_options() const {
    std::string s = source.to_string();
    const BindingOptions def;
    auto num = [](float f) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%g", f);
        return std::string(buf);
    };
    if (options.deadzone != def.deadzone) s += " deadzone=" + num(options.deadzone);
    if (options.saturation != def.saturation) s += " saturation=" + num(options.saturation);
    if (options.curve != def.curve) s += " curve=" + num(options.curve);
    if (options.scale != def.scale) s += " scale=" + num(options.scale);
    if (options.ramp != def.ramp) s += " ramp=" + num(options.ramp);
    if (options.threshold != def.threshold) s += " threshold=" + num(options.threshold);
    if (options.invert) s += " invert";
    if (options.pedal) s += " pedal";
    return s;
}

void Profile::add(std::string_view target, std::string_view sources) {
    for (Binding& b : Binding::parse_line(target, sources)) bindings.push_back(b);
}

void Profile::remove_target(const PadTarget& target) {
    std::erase_if(bindings, [&](const Binding& b) { return b.target == target; });
}

bool Profile::keyboard_only() const {
    return std::none_of(bindings.begin(), bindings.end(), [](const Binding& b) {
        return b.source.kind != InputSource::Kind::Key && b.source.kind != InputSource::Kind::Mouse &&
               b.source.kind != InputSource::Kind::None;
    });
}

const std::vector<std::string_view>& preset_names() {
#if GT3R_XR
    static const std::vector<std::string_view> names = {"quest", "racing", "classic", "wheel", "keyboard"};
#else
    static const std::vector<std::string_view> names = {"racing", "classic", "wheel", "keyboard"};
#endif
    return names;
}

std::string_view default_preset() {
#if GT3R_XR
    return "quest";
#else
    return "racing";
#endif
}

namespace {

void add_keyboard(Profile& p) {
    p.add("Up", "key.Up");
    p.add("Down", "key.Down");
    p.add("Left", "key.Left");
    p.add("Right", "key.Right");
    p.add("Cross", "key.Return | key.W ramp=0.12");
    p.add("Square", "key.S ramp=0.08");
    p.add("Circle", "key.Space");
    p.add("Triangle", "key.Escape | key.Backspace | key.AC_Back"); // AC_Back = Android back
    p.add("L1", "key.Q");
    p.add("R1", "key.E");
    p.add("L2", "key.Z");
    p.add("R2", "key.C");
    p.add("L3", "key.F");
    p.add("R3", "key.G");
    p.add("Start", "key.P | key.Pause");
    p.add("Select", "key.Tab");
    p.add("LX-", "key.A ramp=0.2");
    p.add("LX+", "key.D ramp=0.2");
    p.add("RX-", "key.J");
    p.add("RX+", "key.L");
    p.add("RY-", "key.I");
    p.add("RY+", "key.K");
}

// `b_back`: the right face button (B on Xbox and Quest controllers) is Triangle (back) and the
// top one Circle, as Quest players expect; otherwise the DualShock 2's positions.
void add_pad_common(Profile& p, bool b_back = false) {
    p.add("Up", "pad.dpup");
    p.add("Down", "pad.dpdown");
    p.add("Left", "pad.dpleft");
    p.add("Right", "pad.dpright");
    p.add("Circle", b_back ? "pad.north" : "pad.east");
    p.add("Triangle", b_back ? "pad.east" : "pad.north");
    p.add("L1", "pad.leftshoulder");
    p.add("R1", "pad.rightshoulder");
    p.add("L3", "pad.leftstick");
    p.add("R3", "pad.rightstick");
    p.add("Start", "pad.start");
    p.add("Select", "pad.back");
    p.add("LX", "pad.leftx deadzone=0.08");
    p.add("LY", "pad.lefty deadzone=0.08");
    p.add("RX", "pad.rightx deadzone=0.08");
    p.add("RY", "pad.righty deadzone=0.08");
}

void add_wheel(Profile& p) {
    // Typical USB wheel: axis 0 steering, pedals on axes 1/2 resting at +1.
    // Wheels differ; remap in the controller screen if needed.
    p.add("LX", "joy.axis0");
    p.add("Cross", "joy.axis1 pedal invert");
    p.add("Square", "joy.axis2 pedal invert");
    p.add("R1", "joy.button4");
    p.add("L1", "joy.button5");
    p.add("Circle", "joy.button1");
    p.add("Triangle", "joy.button3");
    p.add("Start", "joy.button9");
    p.add("Select", "joy.button8");
    p.add("Up", "joy.hat0.up");
    p.add("Down", "joy.hat0.down");
    p.add("Left", "joy.hat0.left");
    p.add("Right", "joy.hat0.right");
}

// Modern race layout, targets in GT3's race meaning (see race_action_name).
void add_modern_race_pad(Profile& p) {
    p.add("Cross", "pad.righttrigger");             // accelerate
    p.add("Square", "pad.lefttrigger");             // brake
    p.add("R2", "pad.south | pad.paddle1");         // shift up
    p.add("L2", "pad.west | pad.paddle2");          // shift down
    p.add("Circle", "pad.east");                    // handbrake
    p.add("R1", "pad.north");                       // change view
    p.add("L1", "pad.rightstick | pad.righty+ threshold=0.5"); // look back (R3, or the right stick pulled back)
    p.add("Triangle", "pad.leftshoulder");          // reverse
    p.add("Start", "pad.start");                    // pause
    p.add("Left", "pad.dpleft");
    p.add("Right", "pad.dpright");
    p.add("Up", "pad.dpup");
    p.add("Down", "pad.dpdown");
    p.add("LX", "pad.leftx deadzone=0.08");
    p.add("LY", "pad.lefty deadzone=0.08");
    p.add("RX", "pad.rightx deadzone=0.08");
    p.add("RY", "pad.righty deadzone=0.08");
}

void add_modern_race_keyboard(Profile& p) {
    p.add("Cross", "key.W ramp=0.12 | key.Up ramp=0.12");
    p.add("Square", "key.S ramp=0.08 | key.Down ramp=0.08");
    p.add("LX-", "key.A ramp=0.2");
    p.add("LX+", "key.D ramp=0.2");
    p.add("Left", "key.Left");
    p.add("Right", "key.Right");
    p.add("Circle", "key.Space");
    p.add("R2", "key.E");
    p.add("L2", "key.Q");
    p.add("Triangle", "key.R");
    p.add("R1", "key.C");
    p.add("L1", "key.X");
    p.add("Select", "key.Backspace");
    p.add("Start", "key.Escape | key.P | key.Pause | key.AC_Back");
    p.add("RX-", "key.J");
    p.add("RX+", "key.L");
    p.add("RY-", "key.I");
    p.add("RY+", "key.K");
}

} // namespace

Profile make_preset(std::string_view name) {
    Profile p;
    p.name = std::string(name);
    if (name == "wheel") {
        add_wheel(p);
    } else if (name != "keyboard") { // "racing" (default), "classic", "quest" (VR headsets)
        // The DualShock 2 1:1: analog triggers drive the pressure-sensitive L2/R2.
        add_pad_common(p, name == "quest");
        p.add("Cross", "pad.south");
        p.add("Square", "pad.west");
        p.add("L2", "pad.lefttrigger");
        p.add("R2", "pad.righttrigger");
        if (name != "classic") {
            p.add("R2", "pad.paddle1"); // back paddles shift, like the triggers
            p.add("L2", "pad.paddle2");
        }
    }
    add_keyboard(p);
    return p;
}

const std::vector<std::string_view>& race_layout_names() {
    static const std::vector<std::string_view> names = {"modern", "gt3"};
    return names;
}

Profile make_race_preset(std::string_view layout, std::string_view device_preset) {
    Profile p;
    p.name = layout == "gt3" ? "gt3" : "modern";
    if (p.name == "gt3") return p; // the menu pad itself
    if (device_preset == "wheel") { // pedals and buttons keep their wheel layout
        add_wheel(p);
        PadTarget respawn;
        PadTarget::parse("Select", respawn);
        p.remove_target(respawn); // respawn stays off controllers (keyboard / touch only)
    } else if (device_preset != "keyboard") {
        add_modern_race_pad(p);
    }
    add_modern_race_keyboard(p);
    return p;
}

std::string_view race_action_name(const PadTarget& t) {
    if (t.kind == PadTarget::Kind::Axis) return t.axis == PadAxis::LX ? "Steer" : "";
    if (t.kind != PadTarget::Kind::Button) return "";
    switch (t.button) {
    case PadButton::Cross: return "Accelerate";
    case PadButton::Square: return "Brake";
    case PadButton::Circle: return "Handbrake";
    case PadButton::Triangle: return "Reverse";
    case PadButton::R2: return "Shift up";
    case PadButton::L2: return "Shift down";
    case PadButton::R1: return "Change view";
    case PadButton::L1: return "Look back";
    case PadButton::Select: return "Respawn";
    case PadButton::Start: return "Pause";
    case PadButton::R3: return "Perf. overlay";
    case PadButton::Left: return "Steer left";
    case PadButton::Right: return "Steer right";
    default: return "";
    }
}

} // namespace gt3r
