# StellAirium — Build Instructions

## Requirements

| Tool | Version |
|------|---------|
| Stellarium source | 26.x (matching installed version) |
| Qt | 6.4+ (Qt6 build of Stellarium) |
| CMake | 3.16+ |
| MSVC | 2019 or 2022 |

## Steps

### 1. Download Stellarium source
```
git clone https://github.com/Stellarium/stellarium.git
cd stellarium
git checkout v26.1      # match your installed version tag
```

### 2. Copy plugin into the source tree
```
xcopy /E /I <path_to_StellAirium_folder> stellarium\plugins\StellAirium
```

### 3. Enable the plugin in CMake
Edit `stellarium/plugins/CMakeLists.txt` and add near the end:
```cmake
OPTION(ENABLE_STELLAIRIUM "Enable StellAirium live aircraft plugin." ON)
IF(ENABLE_STELLAIRIUM)
    ADD_SUBDIRECTORY(StellAirium)
ENDIF()
```

### 4. Configure and build
Open the **x64 Native Tools Command Prompt for VS 2022** and run:
```cmd
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64 ^
         -DCMAKE_PREFIX_PATH="C:\Qt\6.x.x\msvc2019_64" ^
         -DENABLE_STELLAIRIUM=ON
cmake --build . --config Release --target StellAirium
```

The DLL will be at:
```
build\plugins\StellAirium\src\Release\libStellAirium.dll
```

### 5. Install
Copy the DLL to:
```
%APPDATA%\Stellarium\modules\StellAirium\libStellAirium.dll
```
(Create the `StellAirium` folder if it does not exist.)

### 6. Enable in Stellarium
`Configuration → Plugins → StellAirium → check "Load at startup"`

---

## Data source — OpenSky Network

Aircraft data is fetched from the **OpenSky Network** free REST API
(https://opensky-network.org).

* Anonymous access: ~100 requests / 24 h at 10-second minimum intervals.
* Free registered account: higher rate limits.

No credentials are needed for a 50 km radius at 15-second refresh
during typical use (6 requests/hour).

---

## Features

| Feature | Details |
|---------|---------|
| Radius | 5 – 500 km from observer location |
| Refresh | 5 – 120 s (default 15 s) |
| Interpolation | Position extrapolated every frame using heading + speed + vertical rate |
| Click info | Callsign, ICAO24, altitude (ft + m), speed (kts + km/h), heading, vertical rate |
| Colours | Purple = high altitude, Cyan = medium, Green = low, Yellow = on ground |

---

## License

GPLv2 or later — same as Stellarium.
