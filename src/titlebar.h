#pragma once
// KanDu 自定义标题栏：图标快捷按钮 + 置顶图钉 + 窗口控制。
// 无 QSS：背景/文字用 QPalette，按钮 hover/checked 用 FlatButton 自绘。
#include <QWidget>
#include <QIcon>
#include <QPoint>
#include <QMap>
#include <QPushButton>
#include <QLabel>

#include "flatbutton.h"

namespace kandu {

class TitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit TitleBar(QWidget *parent = nullptr);

    void setTitle(const QString &text) { mLblTitle->setText(text); }
    void setMaximized(bool maximized);
    void applyTheme(const QColor &bg, const QColor &fg, const QColor &hover,
                    const QColor &sep, const QColor &pinNormal, const QColor &pinActive,
                    const QColor &pinCheckedBg);

    QPushButton *button(const QString &kind) const { return mIconButtons.value(kind); }
    QPushButton *pinButton() const { return mBtnPin; }

signals:
    void minimizeRequested();
    void maximizeRequested();
    void closeRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    FlatButton *makeActionButton(const QString &kind, const QString &tip);
    QFrame *vline();
    FlatButton *makeWinButton(const QString &glyph, const QString &objName);

    QLabel *mLblTitle = nullptr;
    FlatButton *mBtnPin = nullptr;
    FlatButton *mBtnMin = nullptr;
    FlatButton *mBtnMax = nullptr;
    FlatButton *mBtnClose = nullptr;
    QMap<QString, FlatButton *> mIconButtons;
    QColor mPinNormal{QColor("#555558")};
    QColor mPinActive{QColor("#1e6fd9")};
    QColor mPinCheckedBg{QColor("#cfe3fb")};
    QColor mHover{QColor("#e2e2e6")};
    QColor mFg{QColor("#3a3a3c")};

    bool mDragging = false;
    QPoint mDragOffset;
};

} // namespace kandu
