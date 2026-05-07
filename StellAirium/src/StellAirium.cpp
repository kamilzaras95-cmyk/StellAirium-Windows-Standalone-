#include "StellAirium.hpp"
#include "StellAiriumDialog.hpp"

#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelModuleMgr.hpp"
#include "StelObjectMgr.hpp"
#include "StelGui.hpp"
#include "StelGuiItems.hpp"
#include "StelPainter.hpp"
#include "StelProjector.hpp"
#include "StelLocation.hpp"
#include "StelLocaleMgr.hpp"
#include "StelUtils.hpp"
#include "StelTextureMgr.hpp"

#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QDebug>
#include <cmath>

// ============================================================================
// Plugin entry point
// ============================================================================
#include "StelPluginInterface.hpp"

class StellAiriumInterface : public QObject, public StelPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID StelPluginInterface_iid FILE "StellAirium.json")
    Q_INTERFACES(StelPluginInterface)
public:
    StelModule* getStelModule() const override { return new StellAirium(); }
    StelPluginInfo getPluginInfo() const override
    {
        StelPluginInfo info;
        info.id              = QStringLiteral("StellAirium");
        info.displayedName   = N_("StellAirium - Live Aircraft Tracker");
        info.authors         = QStringLiteral("Kamil Zaras");
        info.contact         = QString();
        info.description     = N_("Displays real-time aircraft positions on the sky map "
                                  "using ADS-B data from the OpenSky Network. "
                                  "Positions are interpolated between API refreshes "
                                  "using speed, heading and vertical rate.");
        info.version         = QStringLiteral("1.0.0");
        info.license         = QStringLiteral("GPLv2+");
        info.startByDefault  = false;
        return info;
    }
    QObjectList getExtensionList() const override { return {}; }
};

#include "StellAirium.moc"

// ============================================================================
// Icon generation
// ============================================================================

// Draw a filled airplane silhouette into a sz×sz QImage.
// All icons are oriented nose-UP (toward y=0 in image space).
// The icon is white on transparent; painter.setColor() tints it at draw time.
QImage StellAirium::makeIconImage(AcCategory cat, int sz)
{
    QImage img(sz, sz, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 230));

    float cx = sz * 0.5f;
    float cy = sz * 0.5f;
    float s  = sz / 64.0f;

    // Helper: pixel coords relative to image center.
    // x>0 = right, y>0 = toward tail (down in image), y<0 = toward nose (up in image)
    auto px = [&](float x, float y) { return QPointF(cx + x*s, cy + y*s); };

    switch (cat)
    {
    // ---------------------------------------------------------------- Helicopter
    case AcCategory::Helicopter:
    {
        // Main rotor disc — two crossing blades
        p.setBrush(QColor(255, 255, 255, 160));
        p.drawEllipse(QRectF(cx - 28*s, cy - 2*s, 56*s, 4*s));   // horizontal blade
        p.drawEllipse(QRectF(cx - 2*s,  cy - 28*s, 4*s, 56*s));  // fore-aft blade
        p.setBrush(QColor(255, 255, 255, 230));
        // Hub
        p.drawEllipse(QPointF(cx, cy), 3*s, 3*s);
        // Cabin (below center)
        QPainterPath cabin;
        cabin.addEllipse(QPointF(cx, cy + 8*s), 7*s, 9*s);
        p.drawPath(cabin);
        // Tail boom (thin, angled right/down)
        QPolygonF boom;
        boom << px(4, 6) << px(4, 16) << px(26, 26) << px(26, 22);
        p.drawPolygon(boom);
        // Tail rotor
        p.drawEllipse(QRectF(cx + 23*s, cy + 18*s, 6*s, 3.5f*s));
        break;
    }

    // ---------------------------------------------------------------- Small prop
    case AcCategory::SmallProp:
    {
        // Fuselage — shorter, wider
        QPainterPath fus;
        fus.addEllipse(QPointF(cx, cy - 2*s), 4.5f*s, 20*s);
        p.drawPath(fus);
        // High straight wings (wider than fuselage)
        QPolygonF lw, rw;
        lw << px(-3.5f, -5) << px(-3.5f, 3) << px(-28, 5) << px(-27, -3);
        rw << px( 3.5f, -5) << px( 3.5f, 3) << px( 28, 5) << px( 27, -3);
        p.drawPolygon(lw);
        p.drawPolygon(rw);
        // Propeller disc (semi-transparent ellipse at nose)
        p.setBrush(QColor(255, 255, 255, 110));
        p.drawEllipse(QRectF(cx - 8*s, cy - 23*s, 16*s, 5*s));
        p.setBrush(QColor(255, 255, 255, 230));
        // Small tail fins
        QPolygonF lt, rt;
        lt << px(-3, 14) << px(-3, 18) << px(-11, 20) << px(-11, 16);
        rt << px( 3, 14) << px( 3, 18) << px( 11, 20) << px( 11, 16);
        p.drawPolygon(lt);
        p.drawPolygon(rt);
        break;
    }

    // ---------------------------------------------------------------- Turboprop
    case AcCategory::Turboprop:
    {
        // Fuselage
        p.drawEllipse(QRectF(cx - 5*s, cy - 26*s, 10*s, 52*s));
        // Slightly swept wings
        QPolygonF lw, rw;
        lw << px(-3.5f, -4) << px(-3.5f, 4) << px(-28, 8) << px(-26, -2);
        rw << px( 3.5f, -4) << px( 3.5f, 4) << px( 28, 8) << px( 26, -2);
        p.drawPolygon(lw);
        p.drawPolygon(rw);
        // Propeller discs on engines (underwing)
        p.setBrush(QColor(255, 255, 255, 110));
        p.drawEllipse(QRectF(cx - 22*s, cy - 1*s, 14*s, 4.5f*s));
        p.drawEllipse(QRectF(cx +  8*s, cy - 1*s, 14*s, 4.5f*s));
        p.setBrush(QColor(255, 255, 255, 230));
        // Horizontal stabilizer
        QPolygonF lt, rt;
        lt << px(-3, 18) << px(-3, 22) << px(-13, 24) << px(-12, 20);
        rt << px( 3, 18) << px( 3, 22) << px( 13, 24) << px( 12, 20);
        p.drawPolygon(lt);
        p.drawPolygon(rt);
        break;
    }

    // ---------------------------------------------------------------- Business jet
    case AcCategory::BusinessJet:
    {
        // Sleek narrow fuselage
        p.drawEllipse(QRectF(cx - 3.5f*s, cy - 27*s, 7*s, 54*s));
        // More aggressively swept wings
        QPolygonF lw, rw;
        lw << px(-3, -5) << px(-3, 2) << px(-24, 18) << px(-20, 8);
        rw << px( 3, -5) << px( 3, 2) << px( 24, 18) << px( 20, 8);
        p.drawPolygon(lw);
        p.drawPolygon(rw);
        // Rear-mounted engines (on fuselage, not under wing)
        p.drawEllipse(QPointF(cx - 8*s, cy + 14*s), 3*s, 5.5f*s);
        p.drawEllipse(QPointF(cx + 8*s, cy + 14*s), 3*s, 5.5f*s);
        // T-tail horizontal stabilizer (higher up)
        QPolygonF lt, rt;
        lt << px(-3, 19) << px(-3, 23) << px(-10, 25) << px(-10, 21);
        rt << px( 3, 19) << px( 3, 23) << px( 10, 25) << px( 10, 21);
        p.drawPolygon(lt);
        p.drawPolygon(rt);
        break;
    }

    // ---------------------------------------------------------------- Wide body
    case AcCategory::Widebody:
    {
        // Wide fuselage
        p.drawEllipse(QRectF(cx - 7*s, cy - 27*s, 14*s, 54*s));
        // Broad swept wings
        QPolygonF lw, rw;
        lw << px(-5, -6) << px(-5, 4) << px(-28, 16) << px(-22, 4);
        rw << px( 5, -6) << px( 5, 4) << px( 28, 16) << px( 22, 4);
        p.drawPolygon(lw);
        p.drawPolygon(rw);
        // 4 engine nacelles
        p.drawEllipse(QPointF(cx - 14*s, cy + 4*s), 3.5f*s, 5.5f*s);
        p.drawEllipse(QPointF(cx - 22*s, cy + 9*s), 3.5f*s, 5.5f*s);
        p.drawEllipse(QPointF(cx + 14*s, cy + 4*s), 3.5f*s, 5.5f*s);
        p.drawEllipse(QPointF(cx + 22*s, cy + 9*s), 3.5f*s, 5.5f*s);
        // Large horizontal stabilizer
        QPolygonF lt, rt;
        lt << px(-5, 20) << px(-5, 24) << px(-17, 26) << px(-16, 22);
        rt << px( 5, 20) << px( 5, 24) << px( 17, 26) << px( 16, 22);
        p.drawPolygon(lt);
        p.drawPolygon(rt);
        break;
    }

    // ---------------------------------------------------------------- Narrowbody (default)
    default:
    {
        // Standard fuselage
        p.drawEllipse(QRectF(cx - 5*s, cy - 27*s, 10*s, 54*s));
        // Swept wings
        QPolygonF lw, rw;
        lw << px(-3.5f, -8) << px(-3.5f, 1) << px(-26, 14) << px(-22, 4);
        rw << px( 3.5f, -8) << px( 3.5f, 1) << px( 26, 14) << px( 22, 4);
        p.drawPolygon(lw);
        p.drawPolygon(rw);
        // 2 engine nacelles under wings
        p.drawEllipse(QPointF(cx - 14*s, cy + 4*s), 3*s, 5*s);
        p.drawEllipse(QPointF(cx + 14*s, cy + 4*s), 3*s, 5*s);
        // Horizontal stabilizer
        QPolygonF lt, rt;
        lt << px(-3, 20) << px(-3, 24) << px(-13, 26) << px(-12, 22);
        rt << px( 3, 20) << px( 3, 24) << px( 13, 26) << px( 12, 22);
        p.drawPolygon(lt);
        p.drawPolygon(rt);
        break;
    }
    }

    p.end();
    return img;
}

// Map ICAO typecode (e.g. "B738", "A320", "C172") to AcCategory.
// Falls back to Unknown for anything not in the list.
StellAirium::AcCategory StellAirium::categoryFromTypecode(const QString& tc)
{
    static const QMap<QString, AcCategory> m = {
        // Widebody jets
        {"A332", AcCategory::Widebody}, {"A333", AcCategory::Widebody},
        {"A342", AcCategory::Widebody}, {"A343", AcCategory::Widebody},
        {"A345", AcCategory::Widebody}, {"A346", AcCategory::Widebody},
        {"A350", AcCategory::Widebody}, {"A359", AcCategory::Widebody},
        {"A35K", AcCategory::Widebody}, {"A380", AcCategory::Widebody},
        {"A388", AcCategory::Widebody},
        {"B742", AcCategory::Widebody}, {"B744", AcCategory::Widebody},
        {"B748", AcCategory::Widebody}, {"B752", AcCategory::Widebody},
        {"B753", AcCategory::Widebody}, {"B762", AcCategory::Widebody},
        {"B763", AcCategory::Widebody}, {"B764", AcCategory::Widebody},
        {"B772", AcCategory::Widebody}, {"B773", AcCategory::Widebody},
        {"B77L", AcCategory::Widebody}, {"B77W", AcCategory::Widebody},
        {"B788", AcCategory::Widebody}, {"B789", AcCategory::Widebody},
        {"B78X", AcCategory::Widebody},
        {"IL96", AcCategory::Widebody}, {"A124", AcCategory::Widebody},
        // Narrowbody jets
        {"A318", AcCategory::Narrowbody}, {"A319", AcCategory::Narrowbody},
        {"A320", AcCategory::Narrowbody}, {"A321", AcCategory::Narrowbody},
        {"A20N", AcCategory::Narrowbody}, {"A21N", AcCategory::Narrowbody},
        {"B731", AcCategory::Narrowbody}, {"B732", AcCategory::Narrowbody},
        {"B733", AcCategory::Narrowbody}, {"B734", AcCategory::Narrowbody},
        {"B735", AcCategory::Narrowbody}, {"B736", AcCategory::Narrowbody},
        {"B737", AcCategory::Narrowbody}, {"B738", AcCategory::Narrowbody},
        {"B739", AcCategory::Narrowbody}, {"B38M", AcCategory::Narrowbody},
        {"B39M", AcCategory::Narrowbody},
        {"E170", AcCategory::Narrowbody}, {"E175", AcCategory::Narrowbody},
        {"E190", AcCategory::Narrowbody}, {"E195", AcCategory::Narrowbody},
        {"E75L", AcCategory::Narrowbody}, {"E75S", AcCategory::Narrowbody},
        {"CRJ2", AcCategory::Narrowbody}, {"CRJ7", AcCategory::Narrowbody},
        {"CRJ9", AcCategory::Narrowbody}, {"CRJX", AcCategory::Narrowbody},
        {"MD80", AcCategory::Narrowbody}, {"MD82", AcCategory::Narrowbody},
        {"MD83", AcCategory::Narrowbody}, {"MD88", AcCategory::Narrowbody},
        {"MD90", AcCategory::Narrowbody}, {"B717", AcCategory::Narrowbody},
        {"F100", AcCategory::Narrowbody}, {"F70",  AcCategory::Narrowbody},
        // Business jets
        {"C510", AcCategory::BusinessJet}, {"C525", AcCategory::BusinessJet},
        {"C55B", AcCategory::BusinessJet}, {"C56X", AcCategory::BusinessJet},
        {"C680", AcCategory::BusinessJet}, {"C68A", AcCategory::BusinessJet},
        {"C750", AcCategory::BusinessJet},
        {"E50P", AcCategory::BusinessJet}, {"E55P", AcCategory::BusinessJet},
        {"LJ35", AcCategory::BusinessJet}, {"LJ45", AcCategory::BusinessJet},
        {"LJ55", AcCategory::BusinessJet}, {"LJ60", AcCategory::BusinessJet},
        {"LJ75", AcCategory::BusinessJet},
        {"GL5T", AcCategory::BusinessJet}, {"GLEX", AcCategory::BusinessJet},
        {"G150", AcCategory::BusinessJet}, {"G280", AcCategory::BusinessJet},
        {"G450", AcCategory::BusinessJet}, {"G550", AcCategory::BusinessJet},
        {"G650", AcCategory::BusinessJet},
        {"F900", AcCategory::BusinessJet}, {"F2TH", AcCategory::BusinessJet},
        {"PC24", AcCategory::BusinessJet},
        {"CL60", AcCategory::BusinessJet}, {"CL30", AcCategory::BusinessJet},
        {"CL35", AcCategory::BusinessJet},
        // Turboprops
        {"AT43", AcCategory::Turboprop}, {"AT45", AcCategory::Turboprop},
        {"AT72", AcCategory::Turboprop}, {"AT76", AcCategory::Turboprop},
        {"DH8A", AcCategory::Turboprop}, {"DH8B", AcCategory::Turboprop},
        {"DH8C", AcCategory::Turboprop}, {"DH8D", AcCategory::Turboprop},
        {"SF34", AcCategory::Turboprop}, {"JS41", AcCategory::Turboprop},
        {"E120", AcCategory::Turboprop}, {"D228", AcCategory::Turboprop},
        {"PC12", AcCategory::Turboprop}, {"P180", AcCategory::Turboprop},
        {"TBM7", AcCategory::Turboprop}, {"TBM8", AcCategory::Turboprop},
        {"TBM9", AcCategory::Turboprop}, {"PILB", AcCategory::Turboprop},
        {"C208", AcCategory::Turboprop}, {"C212", AcCategory::Turboprop},
        // Small piston
        {"C172", AcCategory::SmallProp}, {"C152", AcCategory::SmallProp},
        {"C182", AcCategory::SmallProp}, {"C210", AcCategory::SmallProp},
        {"C150", AcCategory::SmallProp},
        {"PA28", AcCategory::SmallProp}, {"PA18", AcCategory::SmallProp},
        {"PA32", AcCategory::SmallProp}, {"PA44", AcCategory::SmallProp},
        {"DA40", AcCategory::SmallProp}, {"DA42", AcCategory::SmallProp},
        {"SR20", AcCategory::SmallProp}, {"SR22", AcCategory::SmallProp},
        {"P28A", AcCategory::SmallProp}, {"P28B", AcCategory::SmallProp},
        {"BE33", AcCategory::SmallProp}, {"BE35", AcCategory::SmallProp},
        {"BE36", AcCategory::SmallProp},
        // Helicopters
        {"R22",  AcCategory::Helicopter}, {"R44",  AcCategory::Helicopter},
        {"R66",  AcCategory::Helicopter},
        {"B06",  AcCategory::Helicopter}, {"B06T", AcCategory::Helicopter},
        {"AS32", AcCategory::Helicopter}, {"AS50", AcCategory::Helicopter},
        {"AS55", AcCategory::Helicopter}, {"AS65", AcCategory::Helicopter},
        {"EC30", AcCategory::Helicopter}, {"EC35", AcCategory::Helicopter},
        {"EC45", AcCategory::Helicopter}, {"EC55", AcCategory::Helicopter},
        {"H145", AcCategory::Helicopter}, {"H135", AcCategory::Helicopter},
        {"H120", AcCategory::Helicopter},
        {"S76",  AcCategory::Helicopter}, {"S92",  AcCategory::Helicopter},
        {"S61",  AcCategory::Helicopter},
        {"AW13", AcCategory::Helicopter}, {"AW16", AcCategory::Helicopter},
        {"AW19", AcCategory::Helicopter},
        {"MD52", AcCategory::Helicopter}, {"MD60", AcCategory::Helicopter},
        {"MI8",  AcCategory::Helicopter}, {"MI17", AcCategory::Helicopter},
        {"AS32", AcCategory::Helicopter},
    };

    auto it = m.find(tc.toUpper());
    return it != m.end() ? it.value() : AcCategory::Unknown;
}

// Fallback mapping from ICAO aircraft type designator (e.g. "L2J", "H1T").
StellAirium::AcCategory StellAirium::categoryFromIcaoType(const QString& icaoType)
{
    if (icaoType.isEmpty()) return AcCategory::Unknown;
    QChar kind = icaoType[0];
    if (kind == 'H' || kind == 'G') return AcCategory::Helicopter; // G = gyroplane
    if (icaoType.size() < 3) return AcCategory::Unknown;
    QString eng = icaoType.mid(2);
    if (eng.startsWith('J'))
    {
        // Get engine count
        int cnt = icaoType.size() > 1 ? QString(icaoType[1]).toInt() : 0;
        return (cnt >= 3) ? AcCategory::Widebody : AcCategory::Narrowbody;
    }
    if (eng.startsWith('T')) return AcCategory::Turboprop;
    if (eng.startsWith('P')) return AcCategory::SmallProp;
    return AcCategory::Unknown;
}

// ============================================================================
// StellAirium implementation
// ============================================================================

StellAirium::StellAirium()
{
    setObjectName(QStringLiteral("StellAirium"));
}

StellAirium::~StellAirium()
{
    delete dialog_;
}

void StellAirium::initIcons()
{
    auto& mgr = StelApp::getInstance().getTextureManager();
    const int SZ = 64;

    for (AcCategory cat : { AcCategory::Unknown,
                             AcCategory::Narrowbody,
                             AcCategory::Widebody,
                             AcCategory::BusinessJet,
                             AcCategory::Turboprop,
                             AcCategory::SmallProp,
                             AcCategory::Helicopter })
    {
        // mirrored(false, true) = vertical flip to compensate for Stellarium's
        // GL upload which reads QImage rows bottom-to-top, otherwise the nose
        // appears at the bottom of the sprite (180° wrong).
        icons_[cat] = mgr.createTexture(makeIconImage(cat, SZ).mirrored(false, true));
    }
}

StelTextureSP StellAirium::getIcon(const AircraftObjP& ac) const
{
    AcCategory cat = typeCache_.value(ac->getIcao24(), AcCategory::Unknown);
    auto it = icons_.find(cat);
    if (it == icons_.end()) it = icons_.find(AcCategory::Unknown);
    return (it != icons_.end()) ? it.value() : StelTextureSP{};
}

void StellAirium::init()
{
    loadSettings();

    GETSTELMODULE(StelObjectMgr)->registerStelObjectMgr(this);

    initIcons();

    netManager_ = new QNetworkAccessManager(this);
    connect(netManager_, &QNetworkAccessManager::finished,
            this, [this](QNetworkReply* r) { onNetworkReply(r); });

    metaManager_ = new QNetworkAccessManager(this);
    connect(metaManager_, &QNetworkAccessManager::finished,
            this, [this](QNetworkReply* r) { onMetaReply(r); });

    metaTimer_ = new QTimer(this);
    connect(metaTimer_, &QTimer::timeout, this, [this]() { onMetaTimer(); });
    metaTimer_->start(1500); // process one metadata request every 1.5 s

    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, [this]() { onTimer(); });

    if (enabled_)
    {
        timer_->start(refreshInterval_ * 1000);
        fetchNow();
    }

    // Toolbar button
    auto* gui = dynamic_cast<StelGui*>(StelApp::getInstance().getGui());
    if (gui)
    {
        QPixmap pm(32, 32);
        pm.fill(Qt::transparent);
        {
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(QPen(Qt::white, 2, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(16, 6, 16, 26);
            p.drawLine(4, 16, 28, 16);
            p.drawLine(10, 24, 16, 20);
            p.drawLine(22, 24, 16, 20);
        }

        toolbarBtn_ = new StelButton(nullptr,
            QPixmap(), pm, QPixmap(),
            QStringLiteral("actionShow_StellAirium_dialog"),
            false, QStringLiteral("actionShow_StellAirium"));

        toolbarBtn_->setToolTip(q_("StellAirium: Live Aircraft Tracker"));
        gui->getButtonBar()->addButton(toolbarBtn_, QStringLiteral("065-pluginsGroup"));

        addAction(QStringLiteral("actionShow_StellAirium_dialog"),
                  N_("StellAirium"),
                  N_("Show StellAirium settings dialog"),
                  this, "onToggleDialog()");

        addAction(QStringLiteral("actionShow_StellAirium"),
                  N_("StellAirium"),
                  N_("Toggle StellAirium aircraft display"),
                  this, "enabled", "Ctrl+Alt+A");
    }
}

void StellAirium::deinit()
{
    saveSettings();
    if (timer_)
        timer_->stop();
    if (metaTimer_)
        metaTimer_->stop();
    icons_.clear();
}

// ---------------------------------------------------------------------------
// Per-frame update: interpolate all aircraft positions
// ---------------------------------------------------------------------------
void StellAirium::update(double deltaTime)
{
    if (!enabled_) return;
    for (auto& ac : aircrafts_)
        ac->update(deltaTime);
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------
void StellAirium::draw(StelCore* core)
{
    if (!enabled_ || aircrafts_.isEmpty()) return;
    drawAircraft(core);
}

double StellAirium::getCallOrder(StelModuleActionName action) const
{
    if (action == ActionDraw)   return 10.0;
    if (action == ActionUpdate) return 10.0;
    return 0.0;
}

// ---------------------------------------------------------------------------
// Drawing implementation
// ---------------------------------------------------------------------------
void StellAirium::drawAircraft(StelCore* core)
{
    StelProjectorP prj = core->getProjection(StelCore::FrameAltAz);
    StelPainter    painter(prj);

    painter.setFont(QFont(QStringLiteral("sans-serif"), 7));

    // Determine selected aircraft (if any)
    const StelObject* selectedRaw = nullptr;
    {
        const QList<StelObjectP>& sel = StelApp::getInstance().getStelObjectMgr().getSelectedObject();
        if (!sel.isEmpty())
            selectedRaw = sel.first().get();
    }

    for (const auto& ac : aircrafts_)
    {
        if (!showOnGround_ && ac->isOnGround()) continue;

        Vec3d altaz;
        if (!ac->computeAltAzVec(core, altaz)) continue;
        if (altaz[2] < 0.0) continue;

        Vec3d screenPos;
        if (!prj->project(altaz, screenPos)) continue;

        float sx = static_cast<float>(screenPos[0]);
        float sy = static_cast<float>(screenPos[1]);

        // Colour by altitude
        double alt = ac->getGeoAlt() > 0.0 ? ac->getGeoAlt() : ac->getBaroAlt();
        Vec3f colour;
        if (ac->isOnGround())
            colour = Vec3f(0.9f, 0.9f, 0.0f);
        else if (alt < 3000.0)
            colour = Vec3f(0.0f, 1.0f, 0.4f);
        else if (alt < 9000.0)
            colour = Vec3f(0.2f, 0.8f, 1.0f);
        else
            colour = Vec3f(0.8f, 0.4f, 1.0f);

        // Screen-space heading direction: project a point 500 m ahead
        float screenDx = 0.f, screenDy = -1.f; // fallback: pointing up
        {
            double hdgRad  = ac->getHeading() * M_PI / 180.0;
            double angDist = 500.0 / 6371000.0;
            double latR = ac->getLat() * M_PI / 180.0;
            double lonR = ac->getLon() * M_PI / 180.0;
            double latA = asin(sin(latR)*cos(angDist) + cos(latR)*sin(angDist)*cos(hdgRad));
            double lonA = lonR + atan2(sin(hdgRad)*sin(angDist)*cos(latR),
                                       cos(angDist) - sin(latR)*sin(latA));

            const StelLocation& loc = core->getCurrentLocation();
            double obsLat = static_cast<double>(loc.getLatitude())  * M_PI / 180.0;
            double obsLon = static_cast<double>(loc.getLongitude()) * M_PI / 180.0;
            double obsAlt = static_cast<double>(loc.altitude);

            auto geoECEF = [](double la, double lo, double h,
                               double& X, double& Y, double& Z) {
                constexpr double A  = 6378137.0;
                constexpr double E2 = 0.00669437999014;
                double sla = sin(la);
                double N = A / sqrt(1.0 - E2*sla*sla);
                X = (N+h)*cos(la)*cos(lo);
                Y = (N+h)*cos(la)*sin(lo);
                Z = (N*(1.0-E2)+h)*sla;
            };

            double ox,oy,oz,ax,ay,az2;
            double acAlt = ac->getGeoAlt()>0.0 ? ac->getGeoAlt() : ac->getBaroAlt();
            geoECEF(obsLat,obsLon,obsAlt, ox,oy,oz);
            geoECEF(latA,lonA,acAlt, ax,ay,az2);

            double ddx=ax-ox, ddy=ay-oy, ddz=az2-oz;
            double sla2=sin(obsLat), cla=cos(obsLat);
            double slo=sin(obsLon),  clo=cos(obsLon);
            double E = -slo*ddx + clo*ddy;
            double N = -sla2*clo*ddx - sla2*slo*ddy + cla*ddz;
            double U = cla*clo*ddx + cla*slo*ddy + sla2*ddz;
            double horiz = sqrt(E*E+N*N);
            double altA  = atan2(U,horiz);
            double azA   = atan2(E,N);

            Vec3d aheadVec(-cos(altA)*cos(azA), cos(altA)*sin(azA), sin(altA));
            aheadVec.normalize();
            Vec3d aheadScr;
            if (prj->project(aheadVec, aheadScr))
            {
                float ddsx = static_cast<float>(aheadScr[0]) - sx;
                float ddsy = static_cast<float>(aheadScr[1]) - sy;
                float len  = sqrtf(ddsx*ddsx + ddsy*ddsy);
                if (len > 0.5f) { screenDx = ddsx/len; screenDy = ddsy/len; }
            }
        }

        // Rotation angle: CW from "up-on-screen"
        float rotDeg = atan2f(screenDx, -screenDy) * 180.f / static_cast<float>(M_PI);

        bool isSelected = (selectedRaw == ac.get());

        // Selection ring — drawn before sprite so sprite renders on top
        if (isSelected)
        {
            painter.setBlending(true);
            painter.setColor(1.f, 0.85f, 0.1f, 0.9f);
            painter.drawCircle(sx, sy, 20.f);
            painter.drawCircle(sx, sy, 22.f);
        }

        // Draw textured sprite
        StelTextureSP tex = getIcon(ac);
        if (tex && tex->bind())
        {
            painter.setColor(colour, isSelected ? 1.0f : 0.92f);
            painter.setBlending(true);
            painter.drawSprite2dMode(sx, sy, 13.f, rotDeg);
        }

        // Queue metadata fetch if not yet requested
        queueMetaRequest(ac->getIcao24());

        // Callsign / ICAO label — brighter when selected
        painter.setColor(1.f, 1.f, isSelected ? 0.1f : 1.f, isSelected ? 1.f : 0.85f);
        QString label = ac->getCallsign().isEmpty() ? ac->getIcao24() : ac->getCallsign();
        if (!ac->getRegistration().isEmpty() && isSelected)
            label += QStringLiteral("  ") + ac->getRegistration();
        painter.drawText(sx + 16.f, sy + 3.f, label);
    }
}

// ============================================================================
// Metadata fetching (hexdb.io — free, no auth required)
// ============================================================================

void StellAirium::queueMetaRequest(const QString& icao24)
{
    if (metaQueued_.contains(icao24)) return;
    metaQueued_.insert(icao24);
    metaQueue_.append(icao24);
}

void StellAirium::onMetaTimer()
{
    if (metaFetching_ || metaQueue_.isEmpty()) return;

    QString icao24 = metaQueue_.takeFirst();

    QNetworkRequest req(QUrl(QString("https://hexdb.io/api/v1/aircraft/%1")
                                 .arg(icao24.toUpper())));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("StellAirium/1.0 (Stellarium plugin)"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    // Store icao24 so we can update the right aircraft in the reply handler
    req.setAttribute(static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 1),
                     icao24);

    metaManager_->get(req);
    metaFetching_ = true;
}

void StellAirium::onMetaReply(QNetworkReply* reply)
{
    reply->deleteLater();
    metaFetching_ = false;

    QString icao24 = reply->request()
        .attribute(static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 1))
        .toString();

    if (reply->error() != QNetworkReply::NoError || icao24.isEmpty())
        return;

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();

    // hexdb.io response fields
    QString typecode = obj.value(QStringLiteral("ICAOTypeCode")).toString();
    QString model    = obj.value(QStringLiteral("Type")).toString();
    QString reg      = obj.value(QStringLiteral("Registration")).toString();
    QString oper     = obj.value(QStringLiteral("RegisteredOwners")).toString();

    AcCategory cat = categoryFromTypecode(typecode);
    typeCache_[icao24] = cat;

    auto it = aircrafts_.find(icao24);
    if (it != aircrafts_.end())
    {
        if (!model.isEmpty()) it.value()->setModel(model);
        if (!reg.isEmpty())   it.value()->setRegistration(reg);
        if (!oper.isEmpty())  it.value()->setOperator(oper);
    }
}

// ---------------------------------------------------------------------------
// StelObjectModule search methods
// ---------------------------------------------------------------------------
QList<StelObjectP> StellAirium::searchAround(const Vec3d& v, double limitFov,
                                               const StelCore* core) const
{
    QList<StelObjectP> result;
    if (!enabled_) return result;

    Vec3d vn = normalize(v);
    double cosLimitFov = cos(limitFov * M_PI / 180.0);

    for (const auto& ac : aircrafts_)
    {
        Vec3d pos = ac->getJ2000EquatorialPos(core);
        if (pos.dot(vn) >= cosLimitFov)
            result.append(qSharedPointerCast<StelObject>(ac));
    }
    return result;
}

StelObjectP StellAirium::searchByName(const QString& name) const
{
    for (const auto& ac : aircrafts_)
        if (ac->getEnglishName().compare(name, Qt::CaseInsensitive) == 0)
            return qSharedPointerCast<StelObject>(ac);
    return {};
}

StelObjectP StellAirium::searchByNameI18n(const QString& name) const
{
    return searchByName(name);
}

StelObjectP StellAirium::searchByID(const QString& id) const
{
    auto it = aircrafts_.find(id.toLower());
    if (it != aircrafts_.end())
        return qSharedPointerCast<StelObject>(it.value());
    return {};
}

QVector<QPair<QString,StelObjectP>> StellAirium::listMatchingObjects(const QString& prefix, int maxNbItem,
                                                                    bool useStartOfWords) const
{
    QVector<QPair<QString,StelObjectP>> result;
    for (const auto& ac : aircrafts_)
    {
        QString name = ac->getEnglishName();
        QString reg  = ac->getRegistration();
        QString icao = ac->getIcao24();

        auto matches = [&](const QString& field) {
            if (field.isEmpty()) return false;
            return useStartOfWords ? field.startsWith(prefix, Qt::CaseInsensitive)
                                   : field.contains(prefix, Qt::CaseInsensitive);
        };

        if (matches(name) || matches(reg) || matches(icao))
        {
            result << qMakePair(name, qSharedPointerCast<StelObject>(ac));
            if (result.size() >= maxNbItem) break;
        }
    }
    return result;
}

QVector<QPair<QString,StelObjectP>> StellAirium::listAllObjects(bool inEnglish) const
{
    Q_UNUSED(inEnglish)
    QVector<QPair<QString,StelObjectP>> list;
    for (const auto& ac : aircrafts_)
        list << qMakePair(ac->getEnglishName(), qSharedPointerCast<StelObject>(ac));
    return list;
}

// ---------------------------------------------------------------------------
// Network — flight data
// ---------------------------------------------------------------------------
void StellAirium::onTimer()
{
    fetchNow();
}

void StellAirium::fetchNow()
{
    if (fetching_) return;
    startFetch();
}

// ---------------------------------------------------------------------------
// Source helpers
// ---------------------------------------------------------------------------
QString StellAirium::sourceName(DataSource s)
{
    switch (s) {
        case DataSource::OpenSky:       return QStringLiteral("OpenSky Network");
        case DataSource::AdsbFi:        return QStringLiteral("adsb.fi");
        case DataSource::AirplanesLive: return QStringLiteral("airplanes.live");
    }
    return QStringLiteral("Unknown");
}

QString StellAirium::buildUrl(DataSource s) const
{
    const StelLocation& loc = StelApp::getInstance().getCore()->getCurrentLocation();
    double lat = static_cast<double>(loc.getLatitude());
    double lon = static_cast<double>(loc.getLongitude());

    switch (s) {
        case DataSource::OpenSky: {
            double dlat = radiusKm_ / 111.0;
            double dlon = radiusKm_ / (111.0 * std::cos(lat * M_PI / 180.0));
            return QString("https://opensky-network.org/api/states/all"
                           "?lamin=%1&lomin=%2&lamax=%3&lomax=%4")
                       .arg(lat - dlat, 0, 'f', 4)
                       .arg(lon - dlon, 0, 'f', 4)
                       .arg(lat + dlat, 0, 'f', 4)
                       .arg(lon + dlon, 0, 'f', 4);
        }
        case DataSource::AdsbFi: {
            double radiusNm = radiusKm_ * 0.539957;
            return QString("https://api.adsb.fi/v1/aircraft?lat=%1&lon=%2&radius=%3")
                       .arg(lat, 0, 'f', 4)
                       .arg(lon, 0, 'f', 4)
                       .arg(radiusNm, 0, 'f', 1);
        }
        case DataSource::AirplanesLive: {
            double radiusNm = radiusKm_ * 0.539957;
            return QString("https://api.airplanes.live/v2/point/%1/%2/%3")
                       .arg(lat, 0, 'f', 4)
                       .arg(lon, 0, 'f', 4)
                       .arg(static_cast<int>(std::ceil(radiusNm)));
        }
    }
    return {};
}

StellAirium::DataSource StellAirium::nextSource(DataSource from) const
{
    // Cycle through all sources starting after 'from', return first non-rate-limited
    for (int i = 1; i < SOURCE_COUNT; ++i)
    {
        DataSource candidate = static_cast<DataSource>((static_cast<int>(from) + i) % SOURCE_COUNT);
        if (!isRateLimited(candidate))
            return candidate;
    }
    return from; // all rate-limited, stay on current
}

void StellAirium::markRateLimited(DataSource s)
{
    rateLimitedUntil_[static_cast<int>(s)] = QDateTime::currentDateTime().addSecs(15 * 60);
}

bool StellAirium::isRateLimited(DataSource s) const
{
    const QDateTime& until = rateLimitedUntil_[static_cast<int>(s)];
    return until.isValid() && QDateTime::currentDateTime() < until;
}

// ---------------------------------------------------------------------------
void StellAirium::startFetch()
{
    // If active source is rate-limited, find the next one
    if (isRateLimited(activeSource_))
    {
        DataSource next = nextSource(activeSource_);
        if (next != activeSource_)
        {
            activeSource_ = next;
            emit activeSourceChanged(activeSource_);
        }
    }

    QString url = buildUrl(activeSource_);
    if (url.isEmpty()) return;

    QNetworkRequest req((QUrl(url)));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("StellAirium/1.0 (Stellarium plugin)"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    netManager_->get(req);
    fetching_ = true;
}

void StellAirium::onNetworkReply(QNetworkReply* reply)
{
    reply->deleteLater();
    fetching_ = false;

    int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    // Rate limited — mark source, switch to next
    if (httpStatus == 429 || reply->error() == QNetworkReply::ContentAccessDenied)
    {
        qWarning() << "StellAirium: rate limited on" << sourceName(activeSource_);
        markRateLimited(activeSource_);
        DataSource next = nextSource(activeSource_);
        if (next != activeSource_)
        {
            activeSource_ = next;
            emit activeSourceChanged(activeSource_);
        }
        lastError_ = QString("Rate limited — switched to %1").arg(sourceName(activeSource_));
        return;
    }

    if (reply->error() != QNetworkReply::NoError)
    {
        lastError_ = reply->errorString();
        qWarning() << "StellAirium: network error:" << lastError_;
        return;
    }

    lastError_.clear();

    QByteArray data = reply->readAll();
    QJsonParseError pe;
    QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
    if (pe.error != QJsonParseError::NoError)
    {
        lastError_ = pe.errorString();
        return;
    }

    switch (activeSource_)
    {
        case DataSource::OpenSky:       parseOpenSky(doc);       break;
        case DataSource::AdsbFi:        parseAdsbFi(doc);        break;
        case DataSource::AirplanesLive: parseAirplanesLive(doc); break;
    }

    lastFetch_     = QDateTime::currentDateTime();
    totalReceived_ += aircrafts_.size();
    emit aircraftUpdated();
}

// ---------------------------------------------------------------------------
// Per-source parsers — all normalize to SI units (m, m/s)
// ---------------------------------------------------------------------------
void StellAirium::parseOpenSky(const QJsonDocument& doc)
{
    QJsonArray states = doc.object().value(QStringLiteral("states")).toArray();

    const StelLocation& loc = StelApp::getInstance().getCore()->getCurrentLocation();
    double obsLat  = static_cast<double>(loc.getLatitude());
    double obsLon  = static_cast<double>(loc.getLongitude());
    double radiusM = radiusKm_ * 1000.0;

    QMap<QString, AircraftObjP> updated;

    for (const QJsonValue& v : states)
    {
        QJsonArray s = v.toArray();
        if (s.size() < 14) continue;
        if (s[5].isNull() || s[6].isNull()) continue;

        QString icao24   = s[0].toString().toLower().trimmed();
        QString callsign = s[1].toString();
        QString country  = s[2].toString();
        qint64  timePosRaw = s[3].isNull() ? 0 : static_cast<qint64>(s[3].toDouble());
        double  acLon    = s[5].toDouble();
        double  acLat    = s[6].toDouble();
        double  baroAlt  = s[7].isNull()  ? 0.0 : s[7].toDouble();   // metres
        bool    onGround = s[8].toBool();
        double  velocity = s[9].isNull()  ? 0.0 : s[9].toDouble();   // m/s
        double  heading  = s[10].isNull() ? 0.0 : s[10].toDouble();  // degrees
        double  vertRate = s[11].isNull() ? 0.0 : s[11].toDouble();  // m/s
        double  geoAlt   = s[13].isNull() ? baroAlt : s[13].toDouble(); // metres
        QString squawk   = (s.size() > 14 && !s[14].isNull()) ? s[14].toString() : QString();
        int     posSrc   = (s.size() > 16 && !s[16].isNull()) ? s[16].toInt() : 0;

        if (greatCircleDistM(obsLat, obsLon, acLat, acLon) > radiusM) continue;
        if (!showOnGround_ && onGround) continue;

        auto it = aircrafts_.find(icao24);
        if (it != aircrafts_.end())
        {
            it.value()->setData(acLat, acLon, baroAlt, geoAlt, velocity, heading, vertRate, onGround);
            it.value()->setFlightInfo(country, squawk, posSrc, timePosRaw);
            updated[icao24] = it.value();
        }
        else
        {
            auto ac = AircraftObjP::create(icao24, callsign, acLat, acLon,
                                           baroAlt, geoAlt, velocity, heading, vertRate, onGround);
            ac->setFlightInfo(country, squawk, posSrc, timePosRaw);
            updated[icao24] = ac;
        }
    }
    aircrafts_ = updated;
}

// Shared parser for adsb.fi and airplanes.live (identical JSON schema)
void StellAirium::parseAdsbExchange(const QJsonDocument& doc, DataSource /*src*/)
{
    QJsonArray ac = doc.object().value(QStringLiteral("ac")).toArray();

    const StelLocation& loc = StelApp::getInstance().getCore()->getCurrentLocation();
    double obsLat  = static_cast<double>(loc.getLatitude());
    double obsLon  = static_cast<double>(loc.getLongitude());
    double radiusM = radiusKm_ * 1000.0;

    QMap<QString, AircraftObjP> updated;

    for (const QJsonValue& v : ac)
    {
        QJsonObject o = v.toObject();

        if (!o.contains(QStringLiteral("lat")) || !o.contains(QStringLiteral("lon"))) continue;

        QString icao24   = o.value(QStringLiteral("hex")).toString().toLower().trimmed();
        QString callsign = o.value(QStringLiteral("flight")).toString().trimmed();
        double  acLat    = o.value(QStringLiteral("lat")).toDouble();
        double  acLon    = o.value(QStringLiteral("lon")).toDouble();

        // alt_baro can be "ground" string or a number (feet)
        QJsonValue altBaroVal = o.value(QStringLiteral("alt_baro"));
        bool    onGround = altBaroVal.isString(); // "ground"
        double  baroAlt  = onGround ? 0.0 : altBaroVal.toDouble() * 0.3048; // ft → m

        double  geoAlt   = o.value(QStringLiteral("alt_geom")).toDouble() * 0.3048; // ft → m
        double  velocity = o.value(QStringLiteral("gs")).toDouble() * 0.514444;     // kts → m/s
        double  heading  = o.value(QStringLiteral("track")).toDouble();             // degrees
        double  vertRate = o.value(QStringLiteral("baro_rate")).toDouble() * 0.00508; // ft/min → m/s
        QString squawk   = o.value(QStringLiteral("squawk")).toString();
        qint64  timePosRaw = 0;
        int     posSrc   = 0;

        if (geoAlt <= 0.0) geoAlt = baroAlt;

        if (greatCircleDistM(obsLat, obsLon, acLat, acLon) > radiusM) continue;
        if (!showOnGround_ && onGround) continue;

        auto it = aircrafts_.find(icao24);
        if (it != aircrafts_.end())
        {
            it.value()->setData(acLat, acLon, baroAlt, geoAlt, velocity, heading, vertRate, onGround);
            it.value()->setFlightInfo(QString(), squawk, posSrc, timePosRaw);
            updated[icao24] = it.value();
        }
        else
        {
            auto aircraft = AircraftObjP::create(icao24, callsign, acLat, acLon,
                                                 baroAlt, geoAlt, velocity, heading, vertRate, onGround);
            aircraft->setFlightInfo(QString(), squawk, posSrc, timePosRaw);
            updated[icao24] = aircraft;
        }
    }
    aircrafts_ = updated;
}

void StellAirium::parseAdsbFi(const QJsonDocument& doc)
{
    parseAdsbExchange(doc, DataSource::AdsbFi);
}

void StellAirium::parseAirplanesLive(const QJsonDocument& doc)
{
    parseAdsbExchange(doc, DataSource::AirplanesLive);
}

// ---------------------------------------------------------------------------
// Status text for dialog
// ---------------------------------------------------------------------------
QString StellAirium::getStatusText() const
{
    if (!enabled_)
        return q_("Plugin disabled.");

    if (fetching_)
        return QString(q_("Fetching from %1…")).arg(sourceName(activeSource_));

    QString status;

    // Fallback warning
    if (activeSource_ != preferredSource_)
    {
        const QDateTime& retryAt = rateLimitedUntil_[static_cast<int>(preferredSource_)];
        int secsLeft = static_cast<int>(QDateTime::currentDateTime().secsTo(retryAt));
        if (secsLeft > 0)
        {
            int m = secsLeft / 60, s = secsLeft % 60;
            status += QString(q_("⚠ %1 rate limited — retry in %2:%3\n"))
                          .arg(sourceName(preferredSource_))
                          .arg(m)
                          .arg(s, 2, 10, QChar('0'));
        }
    }

    if (!lastError_.isEmpty() && lastFetch_.isValid() == false)
        return status + QString(q_("Error: %1")).arg(lastError_);

    if (!lastFetch_.isValid())
        return status + q_("Waiting for first update…");

    int secsAgo = static_cast<int>(lastFetch_.secsTo(QDateTime::currentDateTime()));
    status += QString(q_("%1 aircraft · %2 km · %3 s ago · %4"))
                  .arg(aircrafts_.size())
                  .arg(static_cast<int>(radiusKm_))
                  .arg(secsAgo)
                  .arg(sourceName(activeSource_));
    return status;
}

// ---------------------------------------------------------------------------
// Properties
// ---------------------------------------------------------------------------
void StellAirium::setEnabled(bool v)
{
    if (enabled_ == v) return;
    enabled_ = v;
    if (enabled_)
    {
        timer_->start(refreshInterval_ * 1000);
        fetchNow();
    }
    else
    {
        timer_->stop();
        aircrafts_.clear();
    }
    emit enabledChanged(v);
    saveSettings();
}

void StellAirium::setRadiusKm(double km)
{
    if (qFuzzyCompare(radiusKm_, km)) return;
    radiusKm_ = km;
    emit radiusKmChanged(km);
    saveSettings();
    if (enabled_) fetchNow();
}

void StellAirium::setRefreshInterval(int secs)
{
    if (refreshInterval_ == secs) return;
    refreshInterval_ = secs;
    if (timer_->isActive())
        timer_->setInterval(secs * 1000);
    emit refreshIntervalChanged(secs);
    saveSettings();
}

void StellAirium::setShowOnGround(bool v)
{
    if (showOnGround_ == v) return;
    showOnGround_ = v;
    emit showOnGroundChanged(v);
    saveSettings();
}

void StellAirium::setPreferredSource(DataSource s)
{
    preferredSource_ = s;
    // Reset active to preferred and clear its rate-limit so it gets tried immediately
    rateLimitedUntil_[static_cast<int>(s)] = QDateTime();
    activeSource_ = s;
    emit activeSourceChanged(activeSource_);
    saveSettings();
    if (enabled_) fetchNow();
}

void StellAirium::onToggleDialog()
{
    configureGui(true);
}

bool StellAirium::configureGui(bool show)
{
    if (!dialog_)
        dialog_ = new StellAiriumDialog(nullptr);
    if (show)
        dialog_->setVisible(true);
    return true;
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------
void StellAirium::loadSettings()
{
    QSettings* cfg = StelApp::getInstance().getSettings();
    cfg->beginGroup(QStringLiteral("StellAirium"));
    enabled_         = cfg->value(QStringLiteral("enabled"),         true).toBool();
    radiusKm_        = cfg->value(QStringLiteral("radiusKm"),        50.0).toDouble();
    refreshInterval_ = cfg->value(QStringLiteral("refreshInterval"), 15).toInt();
    showOnGround_    = cfg->value(QStringLiteral("showOnGround"),    false).toBool();
    preferredSource_ = static_cast<DataSource>(
        cfg->value(QStringLiteral("dataSource"), 0).toInt());
    activeSource_    = preferredSource_;
    cfg->endGroup();
}

void StellAirium::saveSettings()
{
    QSettings* cfg = StelApp::getInstance().getSettings();
    cfg->beginGroup(QStringLiteral("StellAirium"));
    cfg->setValue(QStringLiteral("enabled"),         enabled_);
    cfg->setValue(QStringLiteral("radiusKm"),        radiusKm_);
    cfg->setValue(QStringLiteral("refreshInterval"), refreshInterval_);
    cfg->setValue(QStringLiteral("showOnGround"),    showOnGround_);
    cfg->setValue(QStringLiteral("dataSource"),      static_cast<int>(preferredSource_));
    cfg->endGroup();
}

// ---------------------------------------------------------------------------
// Haversine distance (metres)
// ---------------------------------------------------------------------------
double StellAirium::greatCircleDistM(double lat1, double lon1,
                                      double lat2, double lon2)
{
    constexpr double R = 6371000.0;
    double dLat = (lat2 - lat1) * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    double a = sin(dLat/2)*sin(dLat/2)
             + cos(lat1*M_PI/180.0)*cos(lat2*M_PI/180.0)
             * sin(dLon/2)*sin(dLon/2);
    return R * 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
}
