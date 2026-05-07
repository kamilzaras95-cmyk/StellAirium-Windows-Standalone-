# Changelog

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
