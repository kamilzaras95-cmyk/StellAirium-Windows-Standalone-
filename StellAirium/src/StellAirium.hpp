#pragma once

#include "StelObjectModule.hpp"
#include "StelTextureTypes.hpp"
#include "Aircraft.hpp"

#include <QMap>
#include <QSet>
#include <QTimer>
#include <QDateTime>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QVector>
#include <QPair>

class StelButton;
class StellAiriumDialog;

class StellAirium : public StelObjectModule
{
    Q_OBJECT
    Q_PROPERTY(bool     enabled         READ isEnabled         WRITE setEnabled         NOTIFY enabledChanged)
    Q_PROPERTY(double   radiusKm        READ getRadiusKm       WRITE setRadiusKm        NOTIFY radiusKmChanged)
    Q_PROPERTY(int      refreshInterval READ getRefreshInterval WRITE setRefreshInterval NOTIFY refreshIntervalChanged)
    Q_PROPERTY(bool     showOnGround    READ getShowOnGround    WRITE setShowOnGround    NOTIFY showOnGroundChanged)

public:
    enum class AcCategory {
        Unknown,
        SmallProp,
        Turboprop,
        BusinessJet,
        Narrowbody,
        Widebody,
        Helicopter
    };

    enum class DataSource {
        OpenSky = 0,
        AdsbFi  = 1,
        AirplanesLive = 2
    };
    static constexpr int SOURCE_COUNT = 3;

    StellAirium();
    ~StellAirium() override;

    // --- StelModule ---
    void init() override;
    void deinit() override;
    void draw(StelCore* core) override;
    void update(double deltaTime) override;
    double getCallOrder(StelModuleActionName) const override;
    bool configureGui(bool show=true) override;

    // --- StelObjectModule ---
    QList<StelObjectP> searchAround(const Vec3d& v, double limitFov, const StelCore* core) const override;
    StelObjectP searchByName(const QString& name) const override;
    StelObjectP searchByNameI18n(const QString& name) const override;
    StelObjectP searchByID(const QString& id) const override;
    QVector<QPair<QString,StelObjectP>> listMatchingObjects(const QString& objPrefix, int maxNbItem=5, bool useStartOfWords=false) const override;
    QVector<QPair<QString,StelObjectP>> listAllObjects(bool inEnglish) const override;
    QString getName() const override { return QStringLiteral("StellAirium"); }
    QString getStelObjectType() const override { return QStringLiteral("Aircraft"); }

    // --- Properties ---
    bool       isEnabled()          const { return enabled_; }
    double     getRadiusKm()        const { return radiusKm_; }
    int        getRefreshInterval() const { return refreshInterval_; }
    bool       getShowOnGround()    const { return showOnGround_; }
    DataSource getPreferredSource() const { return preferredSource_; }
    DataSource getActiveSource()    const { return activeSource_; }

    int     getAircraftCount() const { return aircrafts_.size(); }
    QString getStatusText()    const;
    static QString sourceName(DataSource s);

public slots:
    void setEnabled(bool v);
    void setRadiusKm(double km);
    void setRefreshInterval(int secs);
    void setShowOnGround(bool v);
    void setPreferredSource(DataSource s);
    void fetchNow();

signals:
    void enabledChanged(bool);
    void radiusKmChanged(double);
    void refreshIntervalChanged(int);
    void showOnGroundChanged(bool);
    void aircraftUpdated();
    void activeSourceChanged(DataSource);

private slots:
    void onTimer();
    void onNetworkReply(QNetworkReply* reply);
    void onMetaReply(QNetworkReply* reply);
    void onMetaTimer();
    void onToggleDialog();

private:
    void loadSettings();
    void saveSettings();
    void startFetch();
    void drawAircraft(StelCore* core);
    void initIcons();
    void queueMetaRequest(const QString& icao24);
    StelTextureSP getIcon(const AircraftObjP& ac) const;

    // --- Data source helpers ---
    QString buildUrl(DataSource s) const;
    void parseOpenSky(const QJsonDocument& doc);
    void parseAdsbFi(const QJsonDocument& doc);
    void parseAirplanesLive(const QJsonDocument& doc);
    void parseAdsbExchange(const QJsonDocument& doc, DataSource src);
    DataSource nextSource(DataSource from) const;
    void markRateLimited(DataSource s);
    bool isRateLimited(DataSource s) const;

    // --- Icon generation ---
    static QImage makeIconImage(AcCategory cat, int sz = 64);
    static AcCategory categoryFromTypecode(const QString& tc);
    static AcCategory categoryFromIcaoType(const QString& icaoType);

    // --- Geo helpers ---
    static double greatCircleDistM(double lat1, double lon1,
                                   double lat2, double lon2);

    // --- State ---
    bool    enabled_         {true};
    double  radiusKm_        {50.0};
    int     refreshInterval_ {15};
    bool    showOnGround_    {false};

    DataSource preferredSource_ {DataSource::OpenSky};
    DataSource activeSource_    {DataSource::OpenSky};

    // Per-source rate-limit expiry (invalid = not rate limited)
    QDateTime rateLimitedUntil_[SOURCE_COUNT];

    QMap<QString, AircraftObjP> aircrafts_;

    QNetworkAccessManager* netManager_   {nullptr};
    QTimer*                timer_        {nullptr};
    bool                   fetching_     {false};
    QDateTime              lastFetch_;
    QString                lastError_;
    int                    totalReceived_{0};

    // --- Aircraft type metadata ---
    QNetworkAccessManager* metaManager_  {nullptr};
    QTimer*                metaTimer_    {nullptr};
    bool                   metaFetching_ {false};
    QMap<QString, AcCategory> typeCache_;
    QSet<QString>          metaQueued_;
    QStringList            metaQueue_;

    // --- Icon textures ---
    QMap<AcCategory, StelTextureSP> icons_;

    StelButton*            toolbarBtn_   {nullptr};
    StellAiriumDialog*     dialog_       {nullptr};
};
