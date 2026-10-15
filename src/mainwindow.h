#pragma once
// KanDu 主窗口：图标标题栏、图片视图、状态栏、快捷键、文件/编辑/导航。
#include <QMainWindow>
#include <QImage>
#include <QString>
#include <QStringList>
#include <QLabel>

#include "settings.h"

class QAction;
class QKeyEvent;
class QCloseEvent;
class QMenu;
class QLabel;
class QStatusBar;

namespace kandu {

class ImageView;
class TitleBar;
class FlatButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    void openPath(const QString &path, bool promptIfDirty = true,
                  bool preserveView = false);
    bool saveCurrent(bool askOverwrite = true);   // 覆盖保存
    bool saveAsCurrent();
    void refreshActions();

#ifdef KANDU_SELFTEST
    // 自动化自测专用钩子（正式版不编译）
    void debugToggleCrop() { toggleCrop(); }
    TitleBar *debugTitleBar() { return mTitleBar; }
    void debugSetTheme(const QString &theme)
    {
        mSettings.theme = theme;
        applySettings();
    }
#endif

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void buildActions();
    void buildStatusBar();
    bool handleKey(QKeyEvent *e);

    // 无边框边缘缩放 / 标题栏与状态栏拖动
    QString edgeFromWindowPos(const QPoint &winPos) const; // 以窗口左上角为原点的边缘判定
    static Qt::CursorShape cursorForEdge(const QString &edge);
    bool handleWindowDrag(QObject *obj, QEvent *event);
    void applyResize(const QPoint &gpos);

    // 文件
    void openDialog();
    void scanFolder();
    void go(int delta);
    void resizeToImage(bool fitView = true);
    bool confirmReplace();
    bool ask(const QString &text, const QString &okText = QStringLiteral("确定"),
             const QString &cancelText = QStringLiteral("取消"));

    // 编辑
    void applyEdit(const QImage &newImg);
    void rotate(int angle);
    void flipH();
    void flipV();
    void toggleCrop();
    void applyCrop();
    void cancelCrop();
    void toggleStraighten(bool horiz = true);
    void applyStraighten(double deg);
    void cancelStraighten();
    void togglePerspective();
    void applyPerspective();
    void cancelPerspective();

    // 窗口
    void toggleTop(bool checked = false);
    void toggleFullscreen(bool checked);
    void toggleStatusBar(bool checked);
    void applySettings();
    void applyTheme();

    // 其他
    void showContextMenu(const QPoint &gpos);
    void showHelp();
    void showAbout();
    void openSettingsDialog();
    void updateTitle();
    void updateStatus();
    void onZoomChanged(double zoom);
    void onDoubleClicked();
    void restoreGeometryFromSettings();
    void saveGeometryToSettings();
    static bool isAnimated(const QString &path);

    Settings mSettings;
    QImage mImage;
    QString mPath;
    bool mDirty = false;
    bool mAlwaysOnTop = false;
    QStringList mFolderFiles;
    int mFolderIndex = -1;
    QString mResizeEdge;
    bool mDragWindow = false;
    QPoint mDragOffset;
    QWidget *mEdgeCursorWidget = nullptr; // 当前显示边缘缩放光标的控件

    ImageView *mView = nullptr;
    TitleBar *mTitleBar = nullptr;
    QLabel *mLblFile = nullptr;
    QLabel *mLblInfo = nullptr;
    QLabel *mLblIdx = nullptr;
    FlatButton *mBtnFit = nullptr;
    FlatButton *mNavPrev = nullptr; // 状态栏张数导航 ‹ ›
    FlatButton *mNavNext = nullptr;

    // Actions
    QAction *actOpen = nullptr;
    QAction *actSave = nullptr;
    QAction *actSaveAs = nullptr;
    QAction *actCopy = nullptr;
    QAction *actCrop = nullptr;
    QAction *actStraighten = nullptr;
    QAction *actPerspective = nullptr;
    QAction *actRotR = nullptr;
    QAction *actRotL = nullptr;
    QAction *actFlipH = nullptr;
    QAction *actFlipV = nullptr;
    QAction *actFit = nullptr;
    QAction *actActual = nullptr;
    QAction *actZoomIn = nullptr;
    QAction *actZoomOut = nullptr;
    QAction *actPrev = nullptr;
    QAction *actNext = nullptr;
    QAction *actFullscreen = nullptr;
    QAction *actTop = nullptr;
    QAction *actStatusBar = nullptr;
    QAction *actSettings = nullptr;
    QAction *actHelp = nullptr;
    QAction *actAbout = nullptr;
};

} // namespace kandu
