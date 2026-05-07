#include "Aircraft.hpp"
#include "StelCore.hpp"
#include "StelUtils.hpp"
#include "StelLocation.hpp"
#include "StelApp.hpp"

#include <cmath>
#include <QDebug>
#include <QDateTime>

// ---------------------------------------------------------------------------
AircraftObj::AircraftObj(const QString& icao24, const QString& callsign,
                         double lat, double lon, double baroAlt, double geoAlt,
                         double velocity, double heading, double vertRate, bool onGround)
    : icao24_(icao24), callsign_(callsign)
    , lat_(lat), lon_(lon)
    , baroAlt_(baroAlt), geoAlt_(geoAlt)
    , heading_(heading), velocity_(velocity), vertRate_(vertRate)
    , tgtHeading_(heading), tgtVelocity_(velocity), tgtVertRate_(vertRate)
    , onGround_(onGround)
{}

QString AircraftObj::getEnglishName() const
{
    QString cs = callsign_.trimmed();
    return cs.isEmpty() ? icao24_ : cs;
}

QString AircraftObj::getCallsign() const
{
    return callsign_.trimmed();
}

// ---------------------------------------------------------------------------
// Smooth-step: ease-in/out curve on [0,1]
// ---------------------------------------------------------------------------
double AircraftObj::smoothStep(double t)
{
    return t * t * (3.0 - 2.0 * t);
}

// ---------------------------------------------------------------------------
// Correction blend weight: 1 right after API snap, fades to 0 as corrT_→1
// ---------------------------------------------------------------------------
double AircraftObj::corrWeight() const
{
    double tc = std::min(corrT_, 1.0);
    return 1.0 - smoothStep(tc);
}

// ---------------------------------------------------------------------------
// Dead-reckoning + kinematic smoothing update
// ---------------------------------------------------------------------------
void AircraftObj::update(double deltaSecs)
{
    if (deltaSecs <= 0.0)
        return;

    // Advance the correction fade timer regardless of flight state
    corrT_ = std::min(corrT_ + deltaSecs / CORR_DUR, 1.0);

    if (onGround_ || velocity_ < 0.1)
        return;

    // --- Layer 2: rate-limited kinematic smoothing toward API targets ---

    // Heading (shortest arc, wraparound-safe)
    {
        double diff = tgtHeading_ - heading_;
        while (diff >  180.0) diff -= 360.0;
        while (diff < -180.0) diff += 360.0;
        double maxDelta = MAX_TURN_RATE * deltaSecs;
        if (std::abs(diff) <= maxDelta)
            heading_ = tgtHeading_;
        else
            heading_ += (diff > 0.0 ? 1.0 : -1.0) * maxDelta;
        while (heading_ >= 360.0) heading_ -= 360.0;
        while (heading_ <    0.0) heading_ += 360.0;
    }

    // Speed
    {
        double diff = tgtVelocity_ - velocity_;
        double maxDelta = MAX_ACCEL * deltaSecs;
        if (std::abs(diff) <= maxDelta)
            velocity_ = tgtVelocity_;
        else
            velocity_ += (diff > 0.0 ? 1.0 : -1.0) * maxDelta;
    }

    // Vertical rate
    {
        double diff = tgtVertRate_ - vertRate_;
        double maxDelta = MAX_VR_CHANGE * deltaSecs;
        if (std::abs(diff) <= maxDelta)
            vertRate_ = tgtVertRate_;
        else
            vertRate_ += (diff > 0.0 ? 1.0 : -1.0) * maxDelta;
    }

    // --- DR position update (spherical great-circle) ---
    double dist    = velocity_ * deltaSecs;
    double hdgRad  = heading_ * M_PI / 180.0;
    double angDist = dist / EARTH_A;

    double latRad = lat_ * M_PI / 180.0;
    double lonRad = lon_ * M_PI / 180.0;

    double newLat = asin(sin(latRad) * cos(angDist)
                        + cos(latRad) * sin(angDist) * cos(hdgRad));
    double newLon = lonRad + atan2(sin(hdgRad) * sin(angDist) * cos(latRad),
                                   cos(angDist) - sin(latRad) * sin(newLat));

    lat_ = newLat * 180.0 / M_PI;
    lon_ = newLon * 180.0 / M_PI;

    baroAlt_ += vertRate_ * deltaSecs;
    geoAlt_  += vertRate_ * deltaSecs;
}

// ---------------------------------------------------------------------------
// Called on each API refresh — snap DR to API and record corrective offset
// so the displayed position transitions smoothly rather than snapping.
// ---------------------------------------------------------------------------
void AircraftObj::setData(double lat, double lon, double baroAlt, double geoAlt,
                           double velocity, double heading, double vertRate, bool onGround)
{
    if (firstUpdate_) {
        lat_         = lat;
        lon_         = lon;
        baroAlt_     = baroAlt;
        geoAlt_      = geoAlt;
        heading_     = heading;
        velocity_    = velocity;
        vertRate_    = vertRate;
        tgtHeading_  = heading;
        tgtVelocity_ = velocity;
        tgtVertRate_ = vertRate;
        onGround_    = onGround;
        corrT_       = 1.0; // no correction needed
        firstUpdate_ = false;
        return;
    }

    // Capture current displayed position before snapping
    double dispLat  = getLat();
    double dispLon  = getLon();
    double dispBaro = getBaroAlt();
    double dispGeo  = getGeoAlt();

    // Snap DR to new API position
    lat_     = lat;
    lon_     = lon;
    baroAlt_ = baroAlt;
    geoAlt_  = geoAlt;

    // Corrective offset fades displayed position back smoothly
    corrLat_  = dispLat  - lat;
    corrLon_  = dispLon  - lon;
    corrBaro_ = dispBaro - baroAlt;
    corrGeo_  = dispGeo  - geoAlt;
    corrT_    = 0.0;

    // Update kinematic targets for Layer 2 smoothing
    tgtHeading_  = heading;
    tgtVelocity_ = velocity;
    tgtVertRate_ = vertRate;
    onGround_    = onGround;
}

void AircraftObj::setFlightInfo(const QString& originCountry, const QString& squawk,
                                 int positionSource, qint64 lastPositionTime)
{
    originCountry_    = originCountry;
    squawk_           = squawk;
    positionSource_   = positionSource;
    lastPositionTime_ = lastPositionTime;
}

// ---------------------------------------------------------------------------
// WGS84 geo → ECEF
// ---------------------------------------------------------------------------
void AircraftObj::geoToECEF(double latRad, double lonRad, double altM,
                              double& X, double& Y, double& Z)
{
    double sinLat = sin(latRad), cosLat = cos(latRad);
    double N = EARTH_A / sqrt(1.0 - EARTH_E2 * sinLat * sinLat);
    X = (N + altM) * cosLat * cos(lonRad);
    Y = (N + altM) * cosLat * sin(lonRad);
    Z = (N * (1.0 - EARTH_E2) + altM) * sinLat;
}

// ---------------------------------------------------------------------------
// Core geometry: ENU vector from observer to aircraft (uses corrected position)
// ---------------------------------------------------------------------------
bool AircraftObj::computeENU(const StelCore* core,
                              double& outE, double& outN, double& outU) const
{
    const StelLocation& loc = core->getCurrentLocation();
    double obsLat = static_cast<double>(loc.getLatitude())  * M_PI / 180.0;
    double obsLon = static_cast<double>(loc.getLongitude()) * M_PI / 180.0;
    double obsAlt = static_cast<double>(loc.altitude);

    double acAlt = (getGeoAlt() > 0.0 ? getGeoAlt() : getBaroAlt());
    double acLat = getLat() * M_PI / 180.0;
    double acLon = getLon() * M_PI / 180.0;

    double ox, oy, oz, ax, ay, az;
    geoToECEF(obsLat, obsLon, obsAlt, ox, oy, oz);
    geoToECEF(acLat,  acLon,  acAlt, ax, ay, az);

    double dx = ax - ox, dy = ay - oy, dz = az - oz;

    double sinLat = sin(obsLat), cosLat = cos(obsLat);
    double sinLon = sin(obsLon), cosLon = cos(obsLon);

    outE = -sinLon * dx + cosLon * dy;
    outN = -sinLat * cosLon * dx - sinLat * sinLon * dy + cosLat * dz;
    outU =  cosLat * cosLon * dx + cosLat * sinLon * dy + sinLat * dz;

    return true;
}

// ---------------------------------------------------------------------------
// AltAz direction vector (for rendering / selection)
// ---------------------------------------------------------------------------
bool AircraftObj::computeAltAzVec(const StelCore* core, Vec3d& outVec) const
{
    double E, N, U;
    computeENU(core, E, N, U);

    double horiz  = sqrt(E * E + N * N);
    double altRad = atan2(U, horiz);
    double azRad  = atan2(E, N);
    if (azRad < 0.0) azRad += 2.0 * M_PI;

    outVec = Vec3d(-cos(altRad) * cos(azRad),
                    cos(altRad) * sin(azRad),
                    sin(altRad));
    outVec.normalize();
    return true;
}

Vec3d AircraftObj::getJ2000EquatorialPos(const StelCore* core) const
{
    Vec3d altaz;
    computeAltAzVec(core, altaz);
    return core->altAzToJ2000(altaz, StelCore::RefractionOff);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static QString compassDir(double azDeg)
{
    static const char* dirs[] = {
        "N","NNE","NE","ENE","E","ESE","SE","SSE",
        "S","SSW","SW","WSW","W","WNW","NW","NNW"
    };
    int idx = static_cast<int>((azDeg + 11.25) / 22.5) % 16;
    return QString::fromLatin1(dirs[idx]);
}

static QString posSourceLabel(int src)
{
    switch (src) {
        case 0: return QStringLiteral("ADS-B");
        case 1: return QStringLiteral("ASTERIX");
        case 2: return QStringLiteral("MLAT");
        case 3: return QStringLiteral("FLARM");
        default: return QStringLiteral("unknown");
    }
}

static QString fmtAlt(double meters)
{
    int ft = static_cast<int>(meters * 3.28084);
    int m  = static_cast<int>(meters);
    return QString("%1 ft  (%2 m)")
        .arg(QLocale().toString(ft))
        .arg(QLocale().toString(m));
}

// ---------------------------------------------------------------------------
// Info string — shown in Stellarium's info panel on selection
// ---------------------------------------------------------------------------
QString AircraftObj::getInfoString(const StelCore* core, const InfoStringGroup& flags) const
{
    QString str;
    QTextStream ts(&str);

    if (flags & Name)
    {
        QString cs = callsign_.trimmed();
        QString hdr = cs.isEmpty() ? icao24_.toUpper() : cs;
        ts << "<h2>" << hdr << "</h2>";

        if (!model_.isEmpty())
            ts << "<p><b>" << model_ << "</b></p>";
        if (!operator_.isEmpty())
            ts << "<p>" << operator_ << "</p>";
    }

    if (flags & Extra)
    {
        // Identity
        ts << "<table width=\"100%\">";

        if (!callsign_.trimmed().isEmpty())
            ts << "<tr><td><b>" << q_("Callsign") << "</b></td><td>"
               << callsign_.trimmed() << "</td></tr>";

        ts << "<tr><td><b>" << q_("ICAO24") << "</b></td><td>"
           << icao24_.toUpper() << "</td></tr>";

        if (!registration_.isEmpty())
            ts << "<tr><td><b>" << q_("Registration") << "</b></td><td>"
               << registration_ << "</td></tr>";

        if (!originCountry_.isEmpty())
            ts << "<tr><td><b>" << q_("Country") << "</b></td><td>"
               << originCountry_ << "</td></tr>";

        if (!squawk_.isEmpty())
            ts << "<tr><td><b>" << q_("Squawk") << "</b></td><td>"
               << squawk_ << "  <i>(" << posSourceLabel(positionSource_) << ")</i></td></tr>";

        ts << "</table><br>";

        // Flight data
        double altM = (getGeoAlt() > 0.0 ? getGeoAlt() : getBaroAlt());

        ts << "<table width=\"100%\">";

        if (onGround_)
        {
            ts << "<tr><td><b>" << q_("Status") << "</b></td><td>"
               << q_("On ground") << "</td></tr>";
        }
        else
        {
            ts << "<tr><td><b>" << q_("Altitude") << "</b></td><td>"
               << fmtAlt(altM) << "</td></tr>";
        }

        if (velocity_ > 0.5)
        {
            int kts = static_cast<int>(velocity_ * 1.94384);
            int kmh = static_cast<int>(velocity_ * 3.6);
            ts << "<tr><td><b>" << q_("Speed") << "</b></td><td>"
               << kts << " kts  (" << kmh << " km/h)</td></tr>";
        }

        if (!onGround_ && velocity_ > 0.5)
        {
            int hdg = static_cast<int>(heading_) % 360;
            ts << "<tr><td><b>" << q_("Heading") << "</b></td><td>"
               << hdg << "\xc2\xb0  " << compassDir(heading_) << "</td></tr>";
        }

        if (!onGround_ && qAbs(vertRate_) > 0.5)
        {
            int fpm = static_cast<int>(vertRate_ * 196.85);
            QString arrow = (vertRate_ > 0) ? QStringLiteral("\xe2\x86\x91") : QStringLiteral("\xe2\x86\x93");
            ts << "<tr><td><b>" << q_("Vertical rate") << "</b></td><td>"
               << arrow << " " << qAbs(fpm) << " ft/min</td></tr>";
        }

        double dispLat = getLat();
        double dispLon = getLon();
        ts << "<tr><td><b>" << q_("Position") << "</b></td><td>"
           << QString::number(qAbs(dispLat), 'f', 4) << "\xc2\xb0 "
           << (dispLat >= 0 ? "N" : "S") << " / "
           << QString::number(qAbs(dispLon), 'f', 4) << "\xc2\xb0 "
           << (dispLon >= 0 ? "E" : "W")
           << "</td></tr>";

        ts << "</table><br>";

        // Observer-relative
        double E, N, U;
        if (computeENU(core, E, N, U))
        {
            double distKm  = sqrt(E*E + N*N + U*U) / 1000.0;
            double horiz   = sqrt(E*E + N*N);
            double elevDeg = atan2(U, horiz) * 180.0 / M_PI;
            double azDeg   = atan2(E, N)     * 180.0 / M_PI;
            if (azDeg < 0.0) azDeg += 360.0;

            ts << "<table width=\"100%\">";
            ts << "<tr><td><b>" << q_("Distance") << "</b></td><td>"
               << QString::number(distKm, 'f', 1) << " km</td></tr>";
            ts << "<tr><td><b>" << q_("Elevation") << "</b></td><td>"
               << QString::number(elevDeg, 'f', 1) << "\xc2\xb0</td></tr>";
            ts << "<tr><td><b>" << q_("Azimuth") << "</b></td><td>"
               << QString::number(azDeg, 'f', 1) << "\xc2\xb0  "
               << compassDir(azDeg) << "</td></tr>";
            ts << "</table><br>";
        }

        if (lastPositionTime_ > 0)
        {
            qint64 ageS = QDateTime::currentSecsSinceEpoch() - lastPositionTime_;
            ts << "<p><i>" << q_("Position last seen: ")
               << ageS << " s ago</i></p>";
        }
    }

    postProcessInfoString(str, flags);
    return str;
}

// ---------------------------------------------------------------------------
QVariantMap AircraftObj::getInfoMap(const StelCore* core) const
{
    QVariantMap m = StelObject::getInfoMap(core);
    m["icao24"]          = icao24_;
    m["callsign"]        = callsign_.trimmed();
    m["registration"]    = registration_;
    m["model"]           = model_;
    m["operator"]        = operator_;
    m["origin_country"]  = originCountry_;
    m["latitude"]        = getLat();
    m["longitude"]       = getLon();
    m["altitude_m"]      = (getGeoAlt() > 0.0 ? getGeoAlt() : getBaroAlt());
    m["velocity_ms"]     = velocity_;
    m["heading"]         = heading_;
    m["vert_rate"]       = vertRate_;
    m["on_ground"]       = onGround_;
    m["squawk"]          = squawk_;
    m["position_source"] = posSourceLabel(positionSource_);
    m["last_pos_time"]   = lastPositionTime_;
    return m;
}
