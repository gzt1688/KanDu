#pragma once
// 图片显示控件：按比例显示、滚轮缩放、拖拽平移、双击动作、裁剪。
#include <QWidget>
#include <QImage>
#include <QPixmap>
#include <QPointF>
#include <QRectF>
#include <QColor>
#include <QString>

#include "settings.h"

namespace kandu {

class ImageView : public QWidget
{
    Q_OBJECT
public:
    explicit ImageView(const Settings &settings, QWidget *parent = nullptr);

    void setImage(const QImage &img, bool preserveView = false);
    void setBackgroundColor(const QColor &color);
    bool hasImage() const { return !mImage.isNull(); }
    QColor background() const { return mBg; }

    // 缩放
    void fitToWindow();
    void setActualSize();
    void zoomIn();
    void zoomOut();
    void zoomAt(const QPointF &pos, double factor);
    double fitScale() const;
    double zoom() const { return mZoom; }
    bool isFit() const { return mFit; }

    // 裁剪
    bool startCrop();
    void exitCrop();
    bool cropMode() const { return mCropMode; }
    // 应用裁剪，返回裁剪后的图片；取消/无效时返回空
    QImage applyCrop();

    // 两点摆正
    bool startStraighten(bool horiz);
    void exitStraighten();
    bool straightenMode() const { return mStraightenMode; }
    bool straightenHoriz() const { return mStrHoriz; }
    void setStraightenHoriz(bool h) { mStrHoriz = h; update(); }
    void toggleStraightenAxis();
    // 应用摆正，返回需要旋转的角度（度）；无效返回空
    bool straightenAngle(double &degOut) const;

    // 透视校准
    bool startPerspective();
    void exitPerspective();
    bool perspectiveMode() const { return mPerspMode; }
    // 应用透视变形，返回新图片；无效/取消返回空
    QImage applyPerspective();

    const QImage &image() const { return mImage; }

signals:
    void zoomChanged(double zoom);
    void fitChanged(bool fit);
    void doubleClicked();
    void contextMenuRequested(const QPoint &gpos);
    void fileDropped(const QString &path);
    void cropApplyRequested();
    void cropCancelRequested();
    void straightenApplyRequested(double deg);
    void straightenCancelRequested();
    void perspectiveCancelRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    QRectF imageRect() const;                 // 图片在屏幕上的绘制区域
    QPointF toImage(const QPointF &pos) const;
    QPointF toScreen(const QPointF &pt) const;
    void clampOffset();
    void updateCropCursor(const QPointF &pos);

    // 裁剪内部
    struct Handle { QString name; QPointF screenPos; };
    QList<Handle> cropHandles() const;
    QString hitHandle(const QPointF &pos) const;
    void cropPress(const QPointF &pos);
    void cropMove(const QPointF &pos);
    void cropRelease();
    void paintCrop(QPainter &p, const QRectF &imgRect);

    // 两点摆正内部
    void straightenPress(const QPointF &pos);
    void straightenMove(const QPointF &pos);
    void straightenRelease(const QPointF &pos);
    void paintStraighten(QPainter &p, const QRectF &imgRect, double deg);
    QPointF straightenCenter() const;
    static QPointF rotateScreenPt(const QPointF &sp, const QPointF &center, double deg);
    double currentPreviewDeg() const;
    QPointF straightenScreenToImage(const QPointF &sp) const;

    // 透视校准内部
    int perspHandleAt(const QPointF &pos) const;
    void perspPress(const QPointF &pos);
    void perspMove(const QPointF &pos);
    void perspRelease();
    void paintPerspective(QPainter &p);

    void refreshPixmap();

    const Settings &mSettings;
    QImage mImage;
    QPixmap mPixmap;
    bool mFit = true;
    double mZoom = 1.0;
    QPointF mOffset{0, 0};
    QColor mBg;

    bool mDragging = false;
    QPointF mLastPos;

    // 裁剪状态（矩形使用图像像素坐标）
    bool mCropMode = false;
    QRectF mCropRect;
    QRectF mCropPrev;
    QString mCropGrab;        // 空 | move | new | 手柄名
    QPointF mCropGrabOffset;
    QPointF mCropAnchor;

    // 两点摆正状态（点使用图像像素坐标）
    bool mStraightenMode = false;
    QList<QPointF> mStrPts;       // 0/1/2 个参考点
    QPointF mStrCur;              // 鼠标当前位置（橡皮筋）
    bool mStrHoriz = true;        // true=摆水平，false=摆垂直
    bool mStrSuppressMenu = false; // 右键刚确认，吞掉随后的 contextMenu 事件
    bool mStrHasPreview = false;   // 已有有效预览角度（重画期间保持不回弹）
    double mStrPreviewDeg = 0.0;  // 上次预览角度

    // 透视校准状态（角点使用图像像素坐标，顺序 tl,tr,br,bl）
    bool mPerspMode = false;
    QList<QPointF> mPerspPts;     // 4 个角点
    int mPerspGrab = -1;          // 正在拖动的角点索引，-1=无
    QImage mPerspPreview;         // 实时结果预览（右下角小窗，拖动时即时刷新）
    bool mPerspPreviewDirty = false; // 角点变化后需重算预览

    static constexpr double kMinZoom = 0.02;
    static constexpr double kMaxZoom = 64.0;
    static constexpr double kHandleRadius = 10.0;
};

} // namespace kandu
