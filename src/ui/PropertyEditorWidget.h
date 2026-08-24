#ifndef PROPERTY_EDITOR_WIDGET_H
#define PROPERTY_EDITOR_WIDGET_H

#include <QWidget>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QGroupBox>
#include <QFormLayout>
#include "../utils/SehHelper.h"
#include "../core/DllLoader.h"

class PropertyEditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit PropertyEditorWidget(QWidget* parent = nullptr);
    ~PropertyEditorWidget();

    WeaponModelParams GetModelParams() const;
    InterfaceMapping GetInterfaceMapping() const;

private:
    // Param Spinboxes
    QDoubleSpinBox* m_spnLat;
    QDoubleSpinBox* m_spnLon;
    QDoubleSpinBox* m_spnAlt;
    QDoubleSpinBox* m_spnSpeed;
    QDoubleSpinBox* m_spnHeading;
    QDoubleSpinBox* m_spnPitch;
    QDoubleSpinBox* m_spnRoll;
    QDoubleSpinBox* m_spnDt;

    // Interface symbol mappings
    QLineEdit* m_editInitSymbol;
    QLineEdit* m_editStepSymbol;
    QLineEdit* m_editDestroySymbol;
    QLineEdit* m_editInfoSymbol;
};

#endif // PROPERTY_EDITOR_WIDGET_H
