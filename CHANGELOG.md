# Changelog

## v1.1.0 — 2026-05-07

### Added
- **Multiple data sources**: OpenSky Network, adsb.fi, airplanes.live — selectable in plugin settings
- **Auto-fallback**: on HTTP 429 rate limit, switches to next source; retries preferred after 15 min
- **Selection ring**: clicking an aircraft shows a yellow highlight ring around it
- **Search by registration and ICAO24**: Ctrl+F now matches callsign, registration (e.g. SP-LRF) and ICAO24
- Selected aircraft label shows callsign + registration together
- Refresh interval range changed to 10–60 s
- Status label shows active source and fallback countdown timer

### Fixed
- Info panel text color now correctly follows Stellarium's configured theme
- Executable renamed to `stellairium.exe`, app name shown as **StellAIRium** everywhere

---

## v1.0.0 — 2026-05-07

### Added
- Live aircraft positions from OpenSky Network ADS-B (auto-refresh every 15 s)
- Aircraft icons per category: narrowbody, widebody, business jet, turboprop, small prop, helicopter
- Icons rotate to match heading
- Click-to-select info panel: callsign, registration, model, operator, country, squawk, altitude, speed, heading, vertical rate, distance, elevation, azimuth, last seen
- Smooth dead-reckoning between API refreshes (physical constraints: 3 °/s turn, 1 m/s² accel, 2 m/s² vert rate change)
- Corrective-offset snapback elimination (6 s fade)
- Aircraft type metadata from hexdb.io (async, rate-limited)
- Info text color inherits Stellarium theme
- Configurable: search radius, refresh interval, show on-ground toggle
- Based on Stellarium 26.1 / Qt 6.9 / Windows x64
