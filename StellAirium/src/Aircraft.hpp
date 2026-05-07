#pragma once

#include "StelObject.hpp"
#include <QString>
#include <QSharedPointer>

class StelCore;

class AircraftObj : public StelObject
{
public:
    AircraftObj(const QString& icao24, const QString& callsign,
                double lat, double lon, double baroAlt, double geoAlt,
                double velocity, double heading, double vertRate, bool onGround);
    ~AircraftObj() override = default;

    // --- StelObject interface ---
    QString getType() const override { return QStringLiteral("Aircraft"); }
    QString getObjectType() const override { return QStringLiteral("Aircraft"); }
    QString getObjectTypeI18n() const override { return QStringLiteral("Aircraft"); }
    QString getID() const override { return icao24_; }
    QString getEnglishName() const override;
    QString getNameI18n() const override { return getEnglishName(); }
    Vec3d getJ2000EquatorialPos(const StelCore* core) const override;
    float getSelectPriority(const StelCore*) const override { return -5.f; }
    QString getInfoString(const StelCore* core, const InfoStringGroup& flags) const override;
    QVariantMap getInfoMap(const StelCore* core) const override;

    // --- Aircraft specific ---
    void update(double deltaSecs);

    // Called on each API refresh. Snaps the DR state to the new API position
    // and starts a smooth positional correction that fades over corrDuration_.
    void setData(double lat, double lon, double baroAlt, double geoAlt,
                 double velocity, double heading, double vertRate, bool onGround);

    void setFlightInfo(const QString& originCountry, const QString& squawk,
                       int positionSource, qint64 lastPositionTime);

    bool computeAltAzVec(const StelCore* core, Vec3d& outVec) const;

    // --- Getters (return display values with correction applied) ---
    // Position with fading correction baked in:
    double getLat()     const { return lat_     + corrLat_  * corrWeight(); }
    double getLon()     const { return lon_     + corrLon_  * corrWeight(); }
    double getBaroAlt() const { return baroAlt_ + corrBaro_ * corrWeight(); }
    double getGeoAlt()  const { return geoAlt_  + corrGeo_  * corrWeight(); }

    // Smoothed kinematics (physically constrained, close to API values):
    double  getHeading()    const { return heading_;  }
    double  getVelocity()   const { return velocity_; }
    double  getVertRate()   const { return vertRate_; }
    bool    isOnGround()    const { return onGround_; }

    QString getCallsign()       const;
    QString getIcao24()         const { return icao24_; }
    QString getModel()          const { return model_; }
    QString getRegistration()   const { return registration_; }
    QString getOperator()       const { return operator_; }
    QString getOriginCountry()  const { return originCountry_; }
    QString getSquawk()         const { return squawk_; }
    int     getPositionSource() const { return positionSource_; }
    qint64  getLastPosTime()    const { return lastPositionTime_; }

    void setModel(const QString& m)        { model_ = m; }
    void setRegistration(const QString& r) { registration_ = r; }
    void setOperator(const QString& op)    { operator_ = op; }

private:
    // --- Identity ---
    QString icao24_;
    QString callsign_;
    QString model_;
    QString registration_;
    QString operator_;
    QString originCountry_;

    // --- DR position (true dead-reckoned, snapped to API on each refresh) ---
    double lat_, lon_;
    double baroAlt_, geoAlt_;

    // --- Smoothed kinematics (physically rate-limited toward API targets) ---
    double heading_;   // degrees, max change 3°/s  (standard rate turn)
    double velocity_;  // m/s,     max change 1 m/s²
    double vertRate_;  // m/s,     max change 2 m/s²

    // --- API targets (latest values received from OpenSky) ---
    double tgtHeading_;
    double tgtVelocity_;
    double tgtVertRate_;
    bool   onGround_;

    // --- Position correction: fades from (old_display - api_pos) → 0
    //     This is what eliminates the snap-back effect.
    double corrLat_{0.0};
    double corrLon_{0.0};
    double corrBaro_{0.0};
    double corrGeo_{0.0};
    double corrT_{1.0};                // 0 = just applied, 1 = fully faded
    static constexpr double CORR_DUR = 6.0; // seconds to fade correction

    // Returns the blend weight for the correction: 1→0 as corrT_ goes 0→1
    double corrWeight() const;

    bool firstUpdate_{true};

    // --- Transponder / metadata ---
    QString squawk_;
    int     positionSource_{0};
    qint64  lastPositionTime_{0};

    // --- Constants ---
    static constexpr double EARTH_A  = 6378137.0;
    static constexpr double EARTH_E2 = 0.00669437999014;

    static constexpr double MAX_TURN_RATE  = 3.0;  // °/s  (standard rate turn)
    static constexpr double MAX_ACCEL      = 1.0;  // m/s² (gentle throttle change)
    static constexpr double MAX_VR_CHANGE  = 2.0;  // m/s² (vertical rate change)

    static void   geoToECEF(double latRad, double lonRad, double altM,
                             double& X, double& Y, double& Z);
    bool computeENU(const StelCore* core,
                    double& outE, double& outN, double& outU) const;
    static double smoothStep(double t);
};

using AircraftObjP = QSharedPointer<AircraftObj>;
