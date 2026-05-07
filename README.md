# StellAIRium — Windows Standalone

**StellAIRium** is a customized build of [Stellarium 26.1](https://stellarium.org) for Windows that overlays **live air traffic** on a realistic 3D sky.

Aircraft positions are fetched in real time from the [OpenSky Network](https://opensky-network.org) ADS-B feed and rendered directly on the sky view, oriented by heading and categorized by aircraft type.

---

## Features

- Live aircraft positions updated every 15 seconds (configurable)
- Aircraft icons by category: narrowbody, widebody, business jet, turboprop, small prop, helicopter
- Icons rotate to match actual heading
- Smooth dead-reckoning between API refreshes (physically constrained turn rate, acceleration, vertical rate)
- Click any aircraft to see: callsign, ICAO24, registration, model, operator, country, squawk, altitude, speed, heading, vertical rate, distance, elevation and azimuth from observer, last position time
- Info text color follows Stellarium's configured theme
- Configurable search radius (default 50 km) and refresh interval
- Toggle on-ground aircraft visibility
- Inherits existing Stellarium user settings (location, sky culture, language, etc.)

---

## Download

Grab the latest release from the [Releases](../../releases) page.  
Unzip and run `stellairium.exe` — no installation required.

**Requires:** Windows 10/11 x64, internet connection for live data.

---

## Data source

Aircraft positions come from **OpenSky Network** (`opensky-network.org`), a community-driven ADS-B network.

Limits for anonymous access:
- ~400 requests/day per IP
- ~5–15 s data latency
- Coverage depends on volunteer receiver locations

Create a free OpenSky account to increase the request limit.

---

## Building from source

This repo is a fork of Stellarium 26.1 with the StellAirium plugin added as a static plugin.

### Prerequisites

- Windows 10/11 x64
- Visual Studio 2022 Build Tools (MSVC)
- CMake 3.20+
- Qt 6.9 (MSVC 2022 x64)

### Steps

```bat
git clone https://github.com/kamilzaras95-cmyk/StellAirium-Windows-Standalone.git
cd StellAirium-Windows-Standalone

cmake -B build -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_PREFIX_PATH=C:/Qt/6.9.3/msvc2022_64

cmake --build build --config Release --target stellarium
```

The output is `build/src/Release/stellairium.exe`.  
Copy it alongside the data folders (`stars/`, `textures/`, `skycultures/`, `landscapes/`, `nebulae/`, `atmosphere/`, `models/`, `data/`) and Qt runtime DLLs to distribute.

---

## Changes vs upstream Stellarium 26.1

| File | Change |
|---|---|
| `plugins/StellAirium/` | New plugin (all files) |
| `src/core/StelUtils.cpp` | App name → `StellAIRium` |
| `src/CMakeLists.txt` | Executable name → `stellairium` |
| `CMakeLists.txt` | Plugin registered as static |

---

## License

StellAIRium inherits Stellarium's [GNU GPL v2+](GPL.txt) license.  
The StellAirium plugin code is original work released under the same license.
