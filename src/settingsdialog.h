#pragma once
// 设置对话框：鼠标 / 显示 / 关于 三页。
#include <QDialog>
#include <QFormLayout>

#include "settings.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;

namespace kandu {

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(const Settings &settings, QWidget *parent = nullptr);

    const Settings &settings() const { return mResult; }

private:
    void buildMousePage();
    QFormLayout *buildDisplayPage();
    QWidget *buildBgColorRow();
    void buildAboutPage(QWidget *page);
    void applyToSettings();
    void restoreDefaults();

    Settings mResult;
    bool mDefaultColor = false;
    QString mCustomBg;

    // 鼠标页
    QCheckBox *mWheelInvert = nullptr;
    QDoubleSpinBox *mWheelStep = nullptr;
    QComboBox *mDoubleClick = nullptr;
    QCheckBox *mDragPan = nullptr;
    // 显示页
    QComboBox *mTheme = nullptr;
    QCheckBox *mFitOnOpen = nullptr;
    QCheckBox *mShowStatusBar = nullptr;
    QCheckBox *mConfirmOverwrite = nullptr;
    QLineEdit *mBgColor = nullptr;
};

} // namespace kandu
