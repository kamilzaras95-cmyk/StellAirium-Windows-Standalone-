# StellAIRium — Windows Standalone

**StellAIRium** is a customized build of [Stellarium 26.1](https://stellarium.org) for Windows that overlays **live air traffic** on a realistic 3D sky.

Aircraft positions are fetched in real time from ADS-B networks (OpenSky Network, adsb.fi, airplanes.live) and rendered directly on the sky view, oriented by heading and categorized by aircraft type.

---

## Download

Grab the latest release from the [Releases](../../releases) page.  
Unzip and run `stellairium.exe` — no installation required.

**Requires:** Windows 10/11 x64, internet connection for live data.

---

## First-time setup

The StellAirium plugin needs to be enabled once after first launch:

1. **Run** `stellairium.exe`
2. Open the **Configuration window** — press `F2` or click the wrench icon in the toolbar
3. Go to the **Plugins** tab
4. Find **StellAirium** in the list and click it
5. Check **"Load at startup"**
6. Click **Close** and **restart** StellAirium
7. After restart, aircraft will appear on the sky automatically

To open the plugin settings (data source, radius, refresh interval):  
Go back to **Configuration → Plugins → StellAirium → Configure**.

---

## Features

- Live aircraft positions from multiple ADS-B sources (OpenSky Network, adsb.fi, airplanes.live)
- Auto-fallback to next source on rate limit (HTTP 429) — resumes preferred source after 15 min
- Aircraft icons by category: narrowbody, widebody, business jet, turboprop, small prop, helicopter
- Icons rotate to match actual heading
- Smooth dead-reckoning between API refreshes (physically constrained: 3°/s turn, 1 m/s² accel)
- **Click any aircraft** to see: callsign, ICAO24, registration, model, operator, country, squawk, altitude, speed, heading, vertical rate, distance, elevation and azimuth from observer, last position time
- **Yellow selection ring** highlights the clicked aircraft
- **Search** via Stellarium's built-in search bar (Ctrl+F) — matches callsign, registration, and ICAO24
- Info text color follows Stellarium's configured theme
- Configurable: data source, search radius (default 50 km), refresh interval (10–60 s)
- Toggle on-ground aircraft visibility
- Inherits existing Stellarium user settings (location, sky culture, language, etc.)

---

## Data sources

| Source | Notes |
|---|---|
| **OpenSky Network** | Community ADS-B network. Anonymous: ~400 req/day per IP |
| **adsb.fi** | Community network, very liberal rate limits |
| **airplanes.live** | Community network, very liberal rate limits |

StellAirium automatically switches to the next source when rate limited, and retries the preferred source after 15 minutes.

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
git clone https://github.com/kamilzaras95-cmyk/StellAirium-Windows-Standalone-.git
cd StellAirium-Windows-Standalone-

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
