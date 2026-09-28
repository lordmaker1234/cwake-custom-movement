# Cwake 2.0

Cwake is a client-side plugin for ClassiCube that introduces customizable physics overrides, camera tilting, custom audio triggers and a built-in speedometer.

## Features

- **Physics Overrides**
  - Configurable Gravity, Friction, Ground Speed/Acceleration, and Air Speed/Acceleration/Cap.
- **Camera Tilt**
  - 2 types of screen tilt, classic Quake style and flight sim-esque delta-yaw.
- **Profiles System**
  - In-game GUI (accessible via a hotkey, defaulting to `Home`) to configure.
  - Modular profile saving and loading for Physics, UI, and Sounds.
- **Speedometer**
  - Customizable on-screen speedometer overlay.
- **Custom Audio**
  - Ability to play custom sounds upon Jump, Land, Bounce, and Ricochet events.

## Usage

1. Place the Cwake binary (`.dll` or `.so`) into your ClassiCube `plugins` directory.
2. Press `Home` (default) to open the Cwake Configuration menu.
3. Settings can be saved to individual profiles and hot-swapped. Profiles are stored in `plugins/cwake/profiles/`.
4. Sounds support `.mp3` and `.wav`. They should be stored in `plugins/cwake/sounds`.

## MOTD Flags
You can set a map's MOTD using these options to enforce certain cwake settings. 
```
mode=         - Camera Tilt Mode
friction=
gravity=
groundspeed=
groundaccel=
bounce=       - Ground bounce momentum transfer (>1 will bounce you higher than you originally started)
airspeed=
airaccel=
aircap=
ricochet_hor= - Horizontal ricochet momentum transfer
ricochet_ver= - Vertical ricochet momentum transfer  
ricochets=    - Maximum ricochet before needing to land
```

## Building

**To Be Finished**

