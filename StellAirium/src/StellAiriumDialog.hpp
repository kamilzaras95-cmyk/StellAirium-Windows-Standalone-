#pragma once

#include "StelDialog.hpp"
#include "StellAirium.hpp"

class QDoubleSpinBox;
class QSpinBox;
class QLabel;
class QCheckBox;
class QComboBox;

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
    void onSourceChanged(int index);
    void onActiveSourceChanged(StellAirium::DataSource s);
    void updateStatus();

private:
    QDoubleSpinBox* radiusSpin_  {nullptr};
    QSpinBox*       refreshSpin_ {nullptr};
    QCheckBox*      groundCheck_ {nullptr};
    QComboBox*      sourceCombo_ {nullptr};
    QLabel*         sourceStatus_{nullptr};
    QLabel*         statusLabel_ {nullptr};
    StellAirium*    plugin_      {nullptr};
};
