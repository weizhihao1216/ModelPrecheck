#include "PropertyEditorWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

PropertyEditorWidget::PropertyEditorWidget(QWidget* parent)
    : QWidget(parent) {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    // 1. Initial Parameters GroupBox
    QGroupBox* grpParams = new QGroupBox("模型初始运动参数配置 (Model Initial Parameters)", this);
    QFormLayout* formParams = new QFormLayout(grpParams);

    m_spnLat = new QDoubleSpinBox(this);
    m_spnLat->setRange(-90.0, 90.0);
    m_spnLat->setValue(39.9042);
    m_spnLat->setDecimals(6);
    m_spnLat->setSuffix(" °");

    m_spnLon = new QDoubleSpinBox(this);
    m_spnLon->setRange(-180.0, 180.0);
    m_spnLon->setValue(116.4074);
    m_spnLon->setDecimals(6);
    m_spnLon->setSuffix(" °");

    m_spnAlt = new QDoubleSpinBox(this);
    m_spnAlt->setRange(0.0, 500000.0);
    m_spnAlt->setValue(5000.0);
    m_spnAlt->setSuffix(" m");

    m_spnSpeed = new QDoubleSpinBox(this);
    m_spnSpeed->setRange(0.0, 10000.0);
    m_spnSpeed->setValue(600.0);
    m_spnSpeed->setSuffix(" m/s");

    m_spnHeading = new QDoubleSpinBox(this);
    m_spnHeading->setRange(-360.0, 360.0);
    m_spnHeading->setValue(45.0);
    m_spnHeading->setSuffix(" °");

    m_spnPitch = new QDoubleSpinBox(this);
    m_spnPitch->setRange(-90.0, 90.0);
    m_spnPitch->setValue(15.0);
    m_spnPitch->setSuffix(" °");

    m_spnRoll = new QDoubleSpinBox(this);
    m_spnRoll->setRange(-180.0, 180.0);
    m_spnRoll->setValue(0.0);
    m_spnRoll->setSuffix(" °");

    m_spnDt = new QDoubleSpinBox(this);
    m_spnDt->setRange(0.001, 1.0);
    m_spnDt->setValue(0.02);
    m_spnDt->setDecimals(3);
    m_spnDt->setSuffix(" s (50Hz)");

    formParams->addRow("初始纬度 (Lat):", m_spnLat);
    formParams->addRow("初始经度 (Lon):", m_spnLon);
    formParams->addRow("初始高度 (Alt):", m_spnAlt);
    formParams->addRow("初始速度 (Speed):", m_spnSpeed);
    formParams->addRow("初始航向角 (Heading):", m_spnHeading);
    formParams->addRow("初始俯仰角 (Pitch):", m_spnPitch);
    formParams->addRow("初始滚转角 (Roll):", m_spnRoll);
    formParams->addRow("仿真步长 (dt):", m_spnDt);

    // 2. Symbol Mapping GroupBox
    QGroupBox* grpSymbols = new QGroupBox("DLL 接口导出符号映射 (Interface Mapping)", this);
    QFormLayout* formSymbols = new QFormLayout(grpSymbols);

    m_editInitSymbol = new QLineEdit("Model_Init", this);
    m_editStepSymbol = new QLineEdit("Model_Step", this);
    m_editDestroySymbol = new QLineEdit("Model_Destroy", this);
    m_editInfoSymbol = new QLineEdit("Model_GetInfo", this);

    formSymbols->addRow("初始化接口 (Init):", m_editInitSymbol);
    formSymbols->addRow("步进计算接口 (Step):", m_editStepSymbol);
    formSymbols->addRow("销毁资源接口 (Destroy):", m_editDestroySymbol);
    formSymbols->addRow("模型信息接口 (GetInfo):", m_editInfoSymbol);

    mainLayout->addWidget(grpParams);
    mainLayout->addStretch(1);
}

PropertyEditorWidget::~PropertyEditorWidget() {
}

WeaponModelParams PropertyEditorWidget::GetModelParams() const {
    WeaponModelParams p;
    p.init_lat = m_spnLat->value();
    p.init_lon = m_spnLon->value();
    p.init_alt = m_spnAlt->value();
    p.init_speed = m_spnSpeed->value();
    p.init_heading = m_spnHeading->value();
    p.init_pitch = m_spnPitch->value();
    p.init_roll = m_spnRoll->value();
    p.step_dt = m_spnDt->value();
    return p;
}

InterfaceMapping PropertyEditorWidget::GetInterfaceMapping() const {
    InterfaceMapping m;
    m.initFuncName = "Model_Init";
    m.stepFuncName = "Model_Step";
    m.destroyFuncName = "Model_Destroy";
    m.getInfoFuncName = "Model_GetInfo";
    return m;
}
