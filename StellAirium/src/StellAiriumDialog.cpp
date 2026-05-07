#include "StellAiriumDialog.hpp"
#include "StellAirium.hpp"
#include "StelApp.hpp"
#include "StelModuleMgr.hpp"
#include "Dialog.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QTimer>
#include <QFrame>

StellAiriumDialog::StellAiriumDialog(QObject* parent)
    : StelDialog(QStringLiteral("StellAiriumDialog"), parent)
{
    plugin_ = GETSTELMODULE(StellAirium);
}

void StellAiriumDialog::createDialogContent()
{
    auto* root = new QVBoxLayout(dialog);
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 0);

    auto* titleBar = new TitleBar(dialog);
    titleBar->setTitle(q_("StellAirium — Live Aircraft Tracker"));
    root->addWidget(titleBar);
    connect(titleBar, &TitleBar::closeClicked, this, &StelDialog::close);
    connect(titleBar, &TitleBar::movedTo, this, &StelDialog::handleMovedTo);

    auto* body = new QWidget(dialog);
    auto* bl = new QVBoxLayout(body);
    bl->setSpacing(8);
    bl->setContentsMargins(10, 8, 10, 10);
    root->addWidget(body);

    // Settings group
    auto* grp = new QGroupBox(q_("Settings"), body);
    auto* grid = new QGridLayout(grp);
    grid->setSpacing(6);
    grid->setContentsMargins(8, 8, 8, 8);

    // Data source
    grid->addWidget(new QLabel(q_("Data source:"), grp), 0, 0);
    sourceCombo_ = new QComboBox(grp);
    sourceCombo_->addItem(StellAirium::sourceName(StellAirium::DataSource::OpenSky));
    sourceCombo_->addItem(StellAirium::sourceName(StellAirium::DataSource::AdsbFi));
    sourceCombo_->addItem(StellAirium::sourceName(StellAirium::DataSource::AirplanesLive));
    sourceCombo_->setCurrentIndex(static_cast<int>(plugin_->getPreferredSource()));
    grid->addWidget(sourceCombo_, 0, 1);

    // Active source indicator
    sourceStatus_ = new QLabel(grp);
    sourceStatus_->setWordWrap(true);
    grid->addWidget(sourceStatus_, 1, 0, 1, 2);

    // Radius
    grid->addWidget(new QLabel(q_("Radius (km):"), grp), 2, 0);
    radiusSpin_ = new QDoubleSpinBox(grp);
    radiusSpin_->setRange(5.0, 500.0);
    radiusSpin_->setSingleStep(5.0);
    radiusSpin_->setDecimals(0);
    radiusSpin_->setValue(plugin_->getRadiusKm());
    grid->addWidget(radiusSpin_, 2, 1);

    // Refresh interval (10-60s)
    grid->addWidget(new QLabel(q_("Refresh interval (s):"), grp), 3, 0);
    refreshSpin_ = new QSpinBox(grp);
    refreshSpin_->setRange(10, 60);
    refreshSpin_->setSingleStep(5);
    refreshSpin_->setValue(plugin_->getRefreshInterval());
    grid->addWidget(refreshSpin_, 3, 1);

    // Show on ground
    groundCheck_ = new QCheckBox(q_("Show aircraft on ground"), grp);
    groundCheck_->setChecked(plugin_->getShowOnGround());
    grid->addWidget(groundCheck_, 4, 0, 1, 2);

    bl->addWidget(grp);

    // Status label
    statusLabel_ = new QLabel(body);
    statusLabel_->setAlignment(Qt::AlignCenter);
    statusLabel_->setWordWrap(true);
    bl->addWidget(statusLabel_);

    // Refresh Now button
    auto* btnLayout = new QHBoxLayout();
    auto* refreshBtn = new QPushButton(q_("Refresh Now"), body);
    btnLayout->addStretch();
    btnLayout->addWidget(refreshBtn);
    btnLayout->addStretch();
    bl->addLayout(btnLayout);

    bl->addStretch();

    // Connections
    connect(sourceCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) { onSourceChanged(idx); });
    connect(radiusSpin_,  QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double km) { onRadiusChanged(km); });
    connect(refreshSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int s) { onRefreshChanged(s); });
    connect(groundCheck_, &QCheckBox::stateChanged,
            this, [this](int state) { onShowGroundChanged(state); });
    connect(refreshBtn, &QPushButton::clicked,
            this, [this]() { plugin_->fetchNow(); });
    connect(plugin_, &StellAirium::aircraftUpdated,
            this, [this]() { updateStatus(); });
    connect(plugin_, &StellAirium::activeSourceChanged,
            this, [this](StellAirium::DataSource s) { onActiveSourceChanged(s); });

    auto* pollTimer = new QTimer(this);
    connect(pollTimer, &QTimer::timeout, this, [this]() { updateStatus(); });
    pollTimer->start(1000);

    onActiveSourceChanged(plugin_->getActiveSource());
    updateStatus();
}

void StellAiriumDialog::onSourceChanged(int index)
{
    plugin_->setPreferredSource(static_cast<StellAirium::DataSource>(index));
}

void StellAiriumDialog::onActiveSourceChanged(StellAirium::DataSource s)
{
    StellAirium::DataSource preferred = plugin_->getPreferredSource();
    if (s != preferred)
        sourceStatus_->setText(QString(q_("⚠ Active: %1 (fallback)")).arg(StellAirium::sourceName(s)));
    else
        sourceStatus_->setText(QString(q_("Active: %1")).arg(StellAirium::sourceName(s)));
}

void StellAiriumDialog::onRadiusChanged(double km)    { plugin_->setRadiusKm(km); }
void StellAiriumDialog::onRefreshChanged(int secs)    { plugin_->setRefreshInterval(secs); }
void StellAiriumDialog::onShowGroundChanged(int state){ plugin_->setShowOnGround(state == Qt::Checked); }

void StellAiriumDialog::updateStatus()
{
    statusLabel_->setText(plugin_->getStatusText());
}
