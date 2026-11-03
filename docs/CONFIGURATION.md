# Configuration

Dethrace looks for a `dethrace.ini` file in the [system preferences directory](https://wiki.libsdl.org/SDL2/SDL_GetPrefPath):
| Platform | Example Path                                                                 |
|----------|----------------------------------------------------------------------|
| Windows  | `C:\Users\<NAME>\AppData\Roaming\dethrace\dethrace.ini`                |
| macOS    | `/Users/<NAME>/Library/Application Support/dethrace/dethrace.ini`      |
| Linux    | `/home/<NAME>/.local/share/dethrace/dethrace.ini`                      |


If this file is not present, dethrace will run with default configuration and attempt to discover the correct Carmageddon directory to use (see below).

## Example dethrace.ini file
```ini
[General]
; Enable original CD check
CdCheck = 0

; Enable original censorship check
GoreCheck = 0

; set to 0 to disable
FPSLimit = 60

; Full screen or window
Windowed = 1

; 3dfx mode (via OpenGL)
Emulate3DFX = 0

; Only used in 3dfx mode. "nearest" for crisp, unfiltered textures (like the software renderer),
; "sharp" for nearest-style texels with anti-aliased edges (no shimmer), otherwise "linear"
TextureFiltering = linear

; Only used in 3dfx mode. Extend the 3d view to fill widescreen windows. The interface stays 4:3, centered
Widescreen = 1

; Multiply the draw distance chosen in the game's options. Fog and darkness move out with it
DrawDistance = 1

; Always draw the track up to the full draw distance (whatever the "track detail" option says),
; and always use the most detailed car models
MaxDetail = 0

; Only used in 3dfx mode. Multisample anti-aliasing sample count (0 to disable, typically 2, 4 or 8)
MSAA = 4

; Only used in 3dfx mode. Magnify textures 2x at load time with a pixel-art upscaler: "none" (disabled),
; "mmpx2x" (smooths diagonals and curves using only each texture's own colours)
; or "xbr2x" (smoother, blended edges)
TextureUpscaler = none

; Only used in 3dfx mode, needs MSAA. Shade every MSAA sample (supersampling): also smooths texture
; detail and cut-out edges, at the cost of MSAA-times more pixel shading
Supersampling = 0

; Censored zombie/robots mode
BoringMode = 0

; Play cut scenes on startup and between races
Cutscenes = 0

; "hires" mode is 640x480, otherwise default 320x200
Hires = 1

; Only used in 'demo' mode. Default demo time out is 240s (4 mins)
DemoTimeout = 240

; Which directory in the [Games] section to run
DefaultGame = c1

; Original physics updates every 40ms, causing noticable camera juddering when cornering at slow speed
; Enabling this runs physics every frame, smoothing movement out
PhysicsPerFrame = 1

[Games]
c1 = /opt/carma/c1
c1demo = /opt/carma/c1demo
sp = /opt/carma/splatpack

[Cheats]
EditMode = 0
FreezeTimer = 0
GameCompleted = 0

[Sound]
Enabled = 1
SoundOptionsScreen = 1
VolumeMultiplier = 1

[Network]
AdapterName = ""
```

## Order of precedence for game directory detection:
1. Directory pointed to by DefaultGame
2. First game in the list if at least 1 game dir is specified
3. `--dir` command line argument
4. Current working directory (if `DATA/GENERAL.TXT` exists)
5. `SDL_GetPrefPath` directory

## Gamepad

Xbox-style controllers (and anything SDL maps as a game controller) are supported. The first connected controller is used.

| Control | Driving | Menus | Action replay |
|---|---|---|---|
| Left stick | Steer (analog) | Navigate | |
| Right trigger | Accelerate (analog) | | Fast forward |
| Left trigger | Brake / reverse (analog) | | Rewind |
| A | Handbrake | Select | Play / pause |
| B | Repair (hold) | Back | Return to race |
| X | Recover car | Delete character | Reverse direction |
| Y | Cockpit / external view | | Change camera |
| LB | Action replay | | Return to race |
| RB | Horn | | |
| View | Map | | |
| Menu | Pause menu | Back to race (in race) / select | Menu |
| Left stick click | Toggle Prat-cam | | |
| Right stick click | Toggle rear view mirror | | |
| Right stick left/right | Look left/right (hold) | | |
| D-pad | Move external camera | Navigate | Step frame (left/right), start/end (up/down) |

While driving, buttons trigger whatever key is bound to the action, so they keep working if the keyboard controls are remapped.
