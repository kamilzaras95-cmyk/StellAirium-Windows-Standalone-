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
#include <QTimer>
#include <QFrame>

StellAiriumDialog::StellAiriumDialog(QObject* parent)
    : StelDialog(QStringLiteral("StellAiriumDialog"), parent)
{
    plugin_ = GETSTELMODULE(StellAirium);
}

void StellAiriumDialog::createDialogContent()
{
    // Outer layout: title bar + body (no margins so title bar touches edges)
    auto* root = new QVBoxLayout(dialog);
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 0);

    // TitleBar — mandatory for StelDialog; provides dragging and close button
    auto* titleBar = new TitleBar(dialog);
    titleBar->setTitle(q_("StellAirium — Live Aircraft Tracker"));
    root->addWidget(titleBar);
    connect(titleBar, &TitleBar::closeClicked, this, &StelDialog::close);
    connect(titleBar, &TitleBar::movedTo, this, &StelDialog::handleMovedTo);

    // Body widget with actual content
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

    grid->addWidget(new QLabel(q_("Radius (km):"), grp), 0, 0);
    radiusSpin_ = new QDoubleSpinBox(grp);
    radiusSpin_->setRange(5.0, 500.0);
    radiusSpin_->setSingleStep(5.0);
    radiusSpin_->setDecimals(0);
    radiusSpin_->setValue(plugin_->getRadiusKm());
    grid->addWidget(radiusSpin_, 0, 1);

    grid->addWidget(new QLabel(q_("Refresh interval (s):"), grp), 1, 0);
    refreshSpin_ = new QSpinBox(grp);
    refreshSpin_->setRange(5, 120);
    refreshSpin_->setSingleStep(5);
    refreshSpin_->setValue(plugin_->getRefreshInterval());
    grid->addWidget(refreshSpin_, 1, 1);

    groundCheck_ = new QCheckBox(q_("Show aircraft on ground"), grp);
    groundCheck_->setChecked(plugin_->getShowOnGround());
    grid->addWidget(groundCheck_, 2, 0, 1, 2);

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

    // Signal connections
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

    auto* pollTimer = new QTimer(this);
    connect(pollTimer, &QTimer::timeout, this, [this]() { updateStatus(); });
    pollTimer->start(1000);

    updateStatus();
}

void StellAiriumDialog::onRadiusChanged(double km)
{
    plugin_->setRadiusKm(km);
}

void StellAiriumDialog::onRefreshChanged(int secs)
{
    plugin_->setRefreshInterval(secs);
}

void StellAiriumDialog::onShowGroundChanged(int state)
{
    plugin_->setShowOnGround(state == Qt::Checked);
}

void StellAiriumDialog::updateStatus()
{
    statusLabel_->setText(plugin_->getStatusText());
}
