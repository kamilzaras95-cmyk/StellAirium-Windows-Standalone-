#pragma once

#include "StelDialog.hpp"

class QDoubleSpinBox;
class QSpinBox;
class QLabel;
class QCheckBox;
class StellAirium;

class StellAiriumDialog : public StelDialog
{
    Q_OBJECT
public:
    explicit StellAiriumDialog(QObject* parent = nullptr);
    ~StellAiriumDialog() override = default;

public slots:
    void retranslate() override {}

protected:
    void createDialogContent() override;

private slots:
    void onRadiusChanged(double km);
    void onRefreshChanged(int secs);
    void onShowGroundChanged(int state);
    void updateStatus();

private:
    QDoubleSpinBox* radiusSpin_  {nullptr};
    QSpinBox*       refreshSpin_ {nullptr};
    QCheckBox*      groundCheck_ {nullptr};
    QLabel*         statusLabel_ {nullptr};
    StellAirium*    plugin_      {nullptr};
};
