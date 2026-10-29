# Input and controllers

## Model

GT3 was written for the **DualShock 2**: 16 buttons, 12 of them
pressure-sensitive (D-pad, face buttons, L1/R1/L2/R2, each reporting
0–255), two 8-bit analog sticks, and two rumble motors (one on/off, one
variable). The game uses pressure for throttle and brake by default.

The engine always reads a **virtual DualShock 2** per player
(`src/input/virtual_pad.h`). Physical devices are mapped onto it through
*bindings*. So original behaviour is preserved exactly, and any device can
drive any control. In particular, an **analog trigger can drive a
pressure-sensitive button**: the default modern race layout maps the right
trigger to the race's accelerate (Cross pressure) and the left trigger to
brake (Square pressure).

Each player has two such pads, each with its own bindings:

- the **menu pad**: the DualShock 2 itself, used by every menu (and, with the
  `gt3` race layout, by the race too);
- the **race pad**: what the race reads while driving. Its buttons always mean
  what they meant in GT3 (see [Race controls](#race-controls)); the player's
  race layout says which physical inputs drive them.

All devices go through SDL3, which gives native support for Xbox
(360/One/Series), PlayStation (DS4/DualSense), Nintendo Switch Pro and Joy-Con,
8BitDo, Steam Controller/Deck, Android and Apple MFi controllers, plus
generic HID joysticks, wheels and pedals. Devices hot-plug at any time.

## Native features

| Feature | How it is used |
|---|---|
| Rumble | DS2 large motor → low-frequency motor, small motor → high-frequency motor |
| Phone vibration (Android, off by default) | `[player1] phone_vibration = on` (Settings → Controls → Phone vibration): player 1's rumble on the device's own motor, only while the controls driving them have no rumble motor (built-in handheld controls, touch, a controller without motors). A controller that rumbles keeps it; the phone stays still |
| Impulse triggers (Xbox One/Series) | `InputManager::set_trigger_rumble`, for brake/ABS feedback |
| Player LEDs / light bars | Set to the player number |
| Button labels | The UI shows Cross/Circle on PlayStation pads and A/B on Xbox pads |
| Motion sensors | `tilt` source: steer by rolling the controller (DualSense, DS4, Switch Pro, or a phone's own accelerometer on Android) |
| Wheels and pedals | Raw joystick axes with `pedal` and `invert` options; the `wheel` preset |
| Keyboard "analog" | `ramp=` smooths digital keys into analog throttle/steering |

## Presets

| Preset | Description |
|---|---|
| `racing` (default) | DualShock 2 layout 1:1 (triggers → L2/R2 pressure), plus back paddles → R2/L2 |
| `classic` | Exact DualShock 2 layout: triggers → L2/R2 |
| `wheel` | Axis 0 steering, axes 1/2 pedals (resting at +1); typical Logitech layout |
| `keyboard` | Keyboard only |
| `quest` (VR builds only, their default) | `racing` with B as Triangle (back) and Y as Circle |

These are menu pad presets. Every preset also includes the keyboard layout, so
player 1 can always use the keyboard: arrows = D-pad, W/S = Cross/Square
(ramped), A/D = left stick, Enter = Cross, Space = Circle, Esc/Backspace =
Triangle, Q/E = L1/R1, Z/C = L2/R2, F/G = L3/R3, P = Start, Tab = Select, IJKL
= right stick. (While driving with the modern layout the keyboard has its own
race keys, below.) On any screen, F11 toggles fullscreen and F12 saves a
screenshot.

`keyboard = off` stops the keyboard driving a player, except player 1 with a
profile that has no controller bindings (the `keyboard` preset, or custom
bindings with only keys): their keyboard stays live, so the game can't be left
with no way to control it.

## Remapping in the app

**Controller setup** has two pages per player, **menu buttons** and **race
controls** (L2/R2 switches page, L1/R1 switches player). Each row has a live
meter (pressure values are shown numerically). Select a row, then:

- **Cross**: press the new input to replace that control's bindings
- **Square**: add an additional input
- **Select**: clear the control
- **Left/Right** on the preset / layout row: load a preset or race layout
- **Start** (hold): test rumble
- **L1/R1**: switch player

Any edit turns that page's profile into a custom one for that player only,
saved to `config.ini` (`[playerN.bindings]` or `[playerN.race]`). Each player
(controller) is remapped independently. Editing a race control under the `gt3`
layout starts from that player's menu bindings.

The **Settings** screen (Controls, Force feedback and Touch pages) edits the
`[playerN]` preset/device/keyboard/race keys (and `[player1] phone_vibration`
on Android), every `[ffb]` key
and every `[touch]` key described below, applies them immediately and opens
this rebinding screen from **Button mapping...**.

## Config syntax

```ini
[player1]
preset = racing        ; used when there is no [player1.bindings] section
device = auto          ; or part of a device name, e.g. "DualSense" or "G29"
keyboard = on
race = modern          ; race layout (modern | gt3), used when there is no [player1.race]
phone_vibration = off  ; Android, player 1 only: rumble on the device's own motor (see Native features)

[player1.bindings]     ; the menu pad
Cross  = pad.south | key.Return
LX     = pad.leftx deadzone=0.08 | tilt curve=1.5

[player1.race]         ; the race pad: targets are GT3's buttons (Cross = accelerate ...)
Cross  = pad.righttrigger curve=1.4 | key.W ramp=0.12
Square = pad.lefttrigger | key.S ramp=0.08
LX     = pad.leftx deadzone=0.08
LX-    = key.A ramp=0.2
LX+    = key.D ramp=0.2
```

**Targets:** `Select L3 R3 Start Up Right Down Left L2 R2 L1 R1 Triangle
Circle Cross Square`, and the axes `LX LY RX RY`, optionally as halves
(`LX-`, `LX+`).

**Sources** (alternatives separated by `|`):

| Source | Meaning |
|---|---|
| `key.<name>` | Keyboard scancode by SDL name; spaces become `_` (`key.Left_Shift`) |
| `pad.<button>` | `south east west north` (or `cross circle square triangle`), `dpup dpdown dpleft dpright`, `leftshoulder rightshoulder`, `leftstick rightstick`, `start back guide`, `paddle1..4`, `misc1`, `touchpad` |
| `pad.<axis>[+/-]` | `leftx lefty rightx righty lefttrigger righttrigger`; `+`/`-` selects a half |
| `joy.axis<N>[+/-]` | Raw joystick axis (wheels and pedals) |
| `joy.button<N>` | Raw joystick button |
| `joy.hat<N>.<up/down/left/right>` | Joystick hat direction |
| `tilt[+/-]` | Controller roll from the accelerometer (±45° = full lock; turning clockwise = positive). On phones and tablets the reading is remapped to the current screen orientation |
| `mouse.<left/right/middle>` | Mouse button |

**Options** (after a source):

| Option | Effect |
|---|---|
| `deadzone=0.1` | Inner deadzone, as a fraction of the range |
| `saturation=0.9` | Input level that already counts as full |
| `curve=1.5` | Response exponent; >1 gives finer control near rest |
| `scale=0.8` | Output multiplier |
| `invert` | Flip the axis |
| `pedal` | Map a −1..1 axis to 0..1 (combine with `invert` for pedals resting at +1) |
| `ramp=0.2` | Seconds to reach full value (makes digital inputs analog) |
| `threshold=0.3` | Pressure at which an analog source counts as a digital press |

When several bindings target the same button, the strongest value wins.
Axis contributions are summed and clamped.

## Race controls

The race pad's buttons keep GT3's meaning, and the race layout (per player:
`[playerN] race`, or **Race controls** in Settings) chooses what drives them:

| Action (race pad button) | `modern` (default) | `gt3` (the original's DS2 layout) | Modern keyboard |
|---|---|---|---|
| Steer (LX, D-pad) | Left stick / D-pad | Left stick / D-pad | A/D (ramped), ←/→ |
| Accelerate (Cross, pressure) | Right trigger | Cross | W / ↑ (ramped) |
| Brake (Square, pressure) | Left trigger | Square | S / ↓ (ramped) |
| Handbrake (Circle) | Circle / B | Circle | Space |
| Shift up / down (R2 / L2) | Cross / Square (A / X), back paddles | R2 / L2 | E / Q |
| Reverse (Triangle, pressure) | L1 (held) | Triangle | R |
| Change view (R1) | Triangle / Y | R1 | C |
| Look back, held (L1) | R3 | L1 | X |
| Respawn (Select, ours) | unbound (keyboard / touch only) | unbound | Backspace |
| Pause (Start) | Start | Start | Esc, P |
| Performance overlay (R3, ours; developer tool) | unbound | unbound | unbound |

While driving, the right stick also looks (held, any layout): pulled back = behind, pushed
left / right = to that side. A direction needs the stick past half way and clearly along it;
diagonals and pushing it forward do nothing.

Respawn and the performance overlay are off controllers by default (under `gt3`
the menu pad's Select and R3 do nothing in a race); bind them in a custom race
layout in **Controller setup** if wanted. Respawn works offline only.

Change view cycles in-car (with mirror), in-car without mirror, chase and far
chase; `[race] views = gt3` (Settings: Race views) keeps GT3's own two, in-car
and chase. Races start in the view last used, in-car at first.

With `gt3` the race reads the menu pad itself, so every button does what it
did on a DualShock 2. In the modern layout shifting down at a standstill also
selects reverse (as in GT3). Wheels (`preset = wheel`) keep their pedals and
buttons in both layouts. Configs from before per-player layouts used
`[race] buttons = gt3 | ours | auto`; it is read once as the default
(`ours` → `modern`; `auto` → `gt3` with the `classic` preset). The touch
controls press the race pad's buttons directly.

Replays, the pause window and the quick menus read the menu pad, as in GT3.
Start opens the pause, in replays too. In a replay, Cross switches between the
replay cameras and the view you drove with, and Up/Down change the followed car
(TARGET MANUAL); in the Video view mode (REPLAY OPTIONS) Cross, Circle, L1 and R1
cut to a new shot instead (presentation §G3). On a quick menu's main page,
Triangle or Square leave at once as its EXIT, except in championships
(presentation §C2).

## Force feedback (`ffb`)

Beyond DS2 rumble, the race sends a per-frame driving state
(`InputManager::set_force_feedback`: speed, brake, ABS active, steering
force, impacts) and each player's device turns it into native feedback:

| Device | Effect |
|---|---|
| DualSense (PS5) | Adaptive trigger on the trigger bound to the brake (Square): resistance after 20 % of travel, lighter at low speed; an 18 Hz vibration while ABS is working. Sent with `SDL_SendGamepadEffect` only when the effect changes |
| Xbox One/Series | Impulse trigger on the brake trigger pulses at ~16 Hz while ABS is working (`SDL_RumbleGamepadTriggers`) |
| Wheels with force feedback | SDL haptics: a self-centring spring that firms up with speed (light when stopped, full from 120 km/h) and a constant force from the front tyres' lateral load (self-aligning feel) plus a kick on impacts. The wheel's own autocentre is switched off while driving |

Effects stop by themselves ~0.25 s after the last update (pause, menus).
The brake trigger is found from the race bindings: whichever of
`pad.lefttrigger` / `pad.righttrigger` drives Square (brake).

```ini
[ffb]
enabled = on
triggers = on          ; DualSense / Xbox trigger effects
wheel = on             ; wheel spring and steering force
strength = 1.00        ; overall wheel force, 0..2
spring = 0.60          ; centring spring at speed, 0..1
trigger_force = 0.60   ; DualSense brake resistance, 0..1
invert = off           ; flip the wheel's steering force if it pulls the wrong way
```

Implementation: `src/input/force_feedback.*` (effect shaping is unit-tested in
`tests/test_platform_ffb.cpp`; on a DualSense, SDL accepts every effect we send; Xbox impulse triggers and wheel
forces are not yet tested on hardware, see docs/dev/PLATFORMS.md).

## Touch controls (phones and tablets)

Without a controller, an on-screen overlay drives player 1's virtual
DualShock 2. It is merged into the pad like any other device, so everything
above (pressure, menus, rumble targets) behaves the same.

The overlay appears on the first touch (on Android and iOS it is shown at
start unless a gamepad is connected) and hides as soon as a gamepad or the
keyboard is used. The Android back and volume keys do not hide it.

Each screen picks a layout:

| Layout | Controls |
|---|---|
| Menu (default) | D-pad (slide between directions), Cross / Circle / Square / Triangle, L1, R1, Select, Start. Taps away from the controls go to the screen underneath |
| Driving (race screen, while racing; its pause and quick menus use Menu) | Steering zone on the left half: put a finger down anywhere and drag sideways (the touch-down point is centre). Gas (Cross) and brake (Square) pedals on the right: the higher the finger on the pedal, the more pressure; a finger can slide from brake to gas. Shift up / down above the pedals, handbrake (Circle), and pause (Start), camera and respawn at the top; they press the race pad's buttons, so they work with any [race layout](#race-controls) (respawn: not under `gt3`) |

```ini
[touch]
mode = auto            ; auto | on | off
steering = drag        ; drag | tilt (roll the device like a wheel, ±45° = full lock)
pedals = position      ; position | area (touch pressure/size, where the screen reports it) | full
pedal_min = 0.35       ; pressure at the bottom of a pedal (position mode)
steer_range = 0.14     ; drag distance for full lock, fraction of the screen width
tilt_deadzone = 0.04   ; fraction of full lock
opacity = 0.50
size = 1.00            ; 0.6 .. 1.6
left_handed = off      ; mirror the layout (pedals on the left)
```

Controls anchor to the edges of the whole screen, not the letterboxed
16:9 game area, so on wide (20:9) phones they move into the side bars and on
16:10 tablets into the top and bottom bars. They keep clear of the display
cutout (`SDL_GetWindowSafeArea`) and, on phones, of the rounded screen
corners (`TouchControls::set_insets`, fed by `App::safe_area_window()`).
`opacity`, `size` and `left_handed` are previewed live on the Settings →
Touch page.

The frontend's own screens (first-run setup, main menu, Developer options, Settings) are
operated by tapping and dragging directly (tap a row, tab or button; drag a
list to scroll; drag a slider; the Android back key goes back), so they use
no on-screen controls (`TouchLayout::None`). `App::touch().safe_area()`
returns the part of the 640×360 game area that the visible controls leave
free (the whole area when they are hidden), so HUDs can inset themselves,
`App::touch().control_bounds()` lists the visible controls' drawn bounds (the
race HUD keeps its corner blocks in place unless one would cover them), and
`App::touch().clear_above()` moves a screen's own button up clear of them (the
title menu's "GT3R menu" button goes above the D-pad).

In GT3's own menus a tap on an item moves the cursor there and a second tap chooses it; a
mouse (or a VR controller's pointer) focuses the item it hovers over, so one click chooses
(a scrolling row such as the tune shop's parts holds still meanwhile; vertical scrolling
lists, which centre the cursor's item, still take a tap to move and one to choose). In the arcade course, class and
car pickers, tap an icon of the strip to move there and tap it again to choose; taps elsewhere
on the left / right third step through the entries.

Screens choose their layout by overriding `Screen::touch_layout()`
(`TouchLayout::Menu`, `Driving` or `None`). On desktop, `GT3R_PHONE=1`
opens a window shaped like a 20:9 phone (cutout, rounded corners, phone UI
sizing, whole-window `--shot`s) and `GT3R_TOUCH_SHOW=1` shows the controls
without a touch, for screenshots. The logic is in
`src/input/touch_controls.*` (unit-tested in `tests/test_platform_touch.cpp`),
drawing and SDL event routing in `src/app/screen_touch_overlay.*`.

## Windows: Xbox controllers without XInput

Windows reads Xbox controllers through its Xbox driver (`xinputhid`, for Bluetooth pads).

**What happened.** The case seen: an Xbox Series X controller over Bluetooth LE on Windows
11 (045e:0b13, firmware 5.17).
- `xinputhid` was installed and attached to the controller's Bluetooth device.
- When the controller connected, `xinputhid` created its XInput device (`HID\…&IG_00`),
  configured it, then removed it and restarted the controller (setupapi.dev.log; Kernel-PnP
  event 430, "requires further installation").
- The controller came back as a plain `HID-compliant game controller` (`input.inf`) and
  stayed that way until it reconnected.

**Fix on the PC:** turn the controller off and on (or re-pair it). XInput then sees it
(`XInput#0`), and SDL treats it as a normal Xbox gamepad.

**Symptoms in that state:**
- the controller works as a generic HID game controller (`joy.cpl` shows it);
- XInput reports "not connected" (1167);
- SDL skips Xbox-branded devices on its other paths because it expects XInput to handle
  them, so the game sees no controller at all.

The game does not work around this state (a DirectInput fallback was tried and removed:
the triggers never reach DirectInput, and SDL's HIDAPI Xbox driver refuses `\\?\HID#` paths
on Windows). Reconnecting the controller is the fix.

If reconnecting does not help, re-pair it or update its firmware in Microsoft's Xbox
Accessories app.

Diagnostics:
- `gt3tool input <iso> [seconds]` lists what SDL sees, with presses and raw axes.
- Any XInput test tool (for example a short PowerShell script calling `XInputGetState`)
  shows whether XInput itself sees the controller.

