#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsPixmapItem>
#include <QDoubleSpinBox>
#include <QGraphicsView>
#include <QPinchGesture>
#include <QGestureEvent>
#include <QMouseEvent>
#include <QDebug>
#include "unistd.h"
#include <QApplication>
#include <QPushButton>
#include <QScrollBar>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

#define maxAnchors 4
#define anchorCount 3

enum ViewMode {
    EditView,
    SplitView,
    HSplitView
};

class HighQualityImageItem : public QGraphicsItem
{
public:
    HighQualityImageItem(const QImage& image = QImage(), QGraphicsItem* parent = nullptr);
    HighQualityImageItem(const QString& path, QGraphicsItem* parent = nullptr);

    void setImage(const QImage& image);
    void load(const QString& path) { m_Image.load(path); }
    void save(const QString& path) { m_Image.save(path); }
    void setTransformMatrix(const QTransform& transform);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

    void setViewMode(ViewMode mode);
    void setSplitFactor(qreal factor);
    QSize originalSize() { return m_Image.size(); }
    QRect originalRect() { return m_Image.rect(); }
    QPointF mapToOriginal(const QPointF& pt) const;
    void setOverlay(const QPainterPath& path, const QPen& pen = QPen(), const QBrush& brush = QBrush()) {
        m_OverlayPath = path;
        m_OverlayPen = pen;
        m_OverlayBrush = brush;
    }
private:
    QPainterPath m_OverlayPath;
    QPen m_OverlayPen;
    QBrush m_OverlayBrush;
    QImage m_Image;
    QTransform m_transform;
    ViewMode m_viewMode = ViewMode::SplitView;
    qreal m_splitFactor = 1.0;
};

class QDoubleSpinBoxX : public QDoubleSpinBox
{
    Q_OBJECT
public:
    QDoubleSpinBoxX(QWidget* parent = nullptr) : QDoubleSpinBox(parent) {};
    void setValueSilent(double val) {
        QDoubleSpinBox::blockSignals(true);
        QDoubleSpinBox::setValue(val);
        QDoubleSpinBox::blockSignals(false);
    }
};

class Anchor : public QPointF {
public:
    Anchor(QColor c = Qt::transparent) {
        defaultColor = c;
    }
    void setPoint(const QPointF& p) {
        setX(p.x());
        setY(p.y());
    }
    void clear() {
        setX(0);
        setY(0);
        setButtonColor(Qt::transparent);
    }
    void setButtonColor() {
        if (!isSet()) {
            setButtonColor(Qt::transparent);
        }
        else {
            setButtonColor(defaultColor);
        }
    }
    void setLoopColor() {
        setButtonColor(Qt::red);
    }
    QPainterPath pointPath(int i) {
        QPainterPath g;
        g.addEllipse(x(),y(),1,1);
        g.addEllipse(x()-2,y()-2,5,5);
        g.addText(x() + 8, y() - 8,QFont("Helvetica",8),QString::number(i));
        return g;
    }
    bool isSet() {
        return !(x() == 0 && y() == 0);
    }
    QPushButton* button = nullptr;
private:
    void setButtonColor(QColor c) {
        button->setAutoFillBackground(true);
        QPalette palette = button->palette();
        palette.setColor(QPalette::Button, c);
        button->setPalette(palette);
    }
    QColor defaultColor = Qt::transparent;
};

class AnchorGroup : public std::array<Anchor,maxAnchors> {
public:
    AnchorGroup(QColor c = Qt::transparent) {
        defaultColor = c;
        for (int i = 0; i < maxAnchors; i++) at(i) = Anchor(c);
    }
    QPainterPath path() {
        QPainterPath g;
        for (int i = 0; i < maxAnchors; i++) g.addPath(at(i).pointPath(i + 1));
        return g;
    }
    QPen pen() {
        return QPen(defaultColor);
    }
    void clear() {
        for (int i = 0; i < maxAnchors; i++) at(i).clear();
    }
    void setButtonColor() {
        for (int i = 0; i < maxAnchors; i++) at(i).setButtonColor();
    }
private:
    QColor defaultColor;
};

class Anchors : public std::array<AnchorGroup,2> {
public:
    Anchors() {
        at(0) = AnchorGroup(Qt::yellow);
        at(1) = AnchorGroup(Qt::green);
    }
    void clear() {
        for (int i = 0; i < 2; i++) at(i).clear();
    }
    void setButtonColor() {
        for (int i = 0; i < 2; i++) at(i).setButtonColor();
    }
    AnchorGroup& before() { return at(0); }
    AnchorGroup& after() { return at(1); }
    Anchor& before(int index) { return at(0)[index]; }
    Anchor& after(int index) { return at(1)[index]; }
    bool computeEnabled(int count) {
        for (int i = 0; i < count; i++) {
            if (!before(i).isSet()) return false;
            if (!after(i).isSet()) return false;
        }
        return true;
    }
    void enableComputeButtons() {
        for (int i = 0; i < maxAnchors; i++) computeButtons[i]->setEnabled(computeEnabled(i + 1));
    }
    std::array<QPushButton*,maxAnchors> computeButtons;
};

class QGraphicsViewX: public QGraphicsView
{
    Q_OBJECT
public:
    QGraphicsViewX(QWidget *parent = 0)
    {
        Q_UNUSED(parent);
        setTransformationAnchor(AnchorUnderMouse);
        resetTransform();
        setDragMode(ScrollHandDrag);
        grabGesture(Qt::PinchGesture);
    }
    void loopForPoint(Anchor& a) {
        loopPoint = a;
        if (looping) {
            breakLoop(a);
            return;
        }
        looping = true;
        a.setLoopColor();
        QApplication::setOverrideCursor(Qt::PointingHandCursor);
        while (looping) {
            usleep(100);
            QApplication::processEvents();
        }
        breakLoop(a);
    }
    QSizeF origSize;
signals:
    void fingerMoved(QPointF);
protected:
    virtual bool event(QEvent *event)
    {
        if (event->type() == QEvent::Gesture) return gestureEvent(static_cast<QGestureEvent*>(event));
        return QGraphicsView::event(event);
    }
    void keyPressEvent(QKeyEvent* /*event*/) {
        looping = false;
    }
    void hideEvent(QHideEvent* /*event*/) {
        looping = false;
    }
    void mousePressEvent(QMouseEvent* event) {
        m_MouseDown = true;
        loopPoint = mapToScene(event->position().toPoint());
        looping = false;
    }
    void mouseMoveEvent(QMouseEvent* event) {
        if (m_MouseDown) {
            QPointF p = mapToScene(event->position().toPoint());
            p.setX(p.x() / origSize.width());
            p.setY(p.y() / origSize.height());
            if (p.x() < 0) p.setX(0);
            if (p.y() < 0) p.setY(0);
            if (p.x() > 1) p.setX(1);
            if (p.y() > 1) p.setY(0);
            emit fingerMoved(p);
        }
    }
    void mouseReleaseEvent(QMouseEvent* /*event*/) {
        m_MouseDown = false;
    }
private:
    bool m_MouseDown = false;
    bool looping = false;
    QPointF loopPoint;
    void breakLoop(Anchor& a) {
        looping = false;
        QApplication::restoreOverrideCursor();
        if (loopPoint != QPointF(0,0)) a.setPoint(loopPoint);
        a.setButtonColor();
    }
    bool gestureEvent(QGestureEvent *event)
    {
        if (QGesture *pinch = event->gesture(Qt::PinchGesture))
            pinchTriggered(static_cast<QPinchGesture *>(pinch));
        return true;
    }
    void pinchTriggered(QPinchGesture* event)
    {
        // 1. Hämta gesture center i view- och scenkoordinater
        QPointF gestureCenterInView = event->centerPoint();
        QPointF gestureCenterInScene = mapToScene(gestureCenterInView.toPoint());

        // 2. Hämta aktuell skalningsfaktor från gesten
        currentStepScaleFactor = event->totalScaleFactor();

        // 3. Om gesten är klar – applicera sista skalningen innan vi nollställer
        if (event->state() == Qt::GestureFinished) {
            scaleFactor *= currentStepScaleFactor;
            scaleFactor = std::clamp(scaleFactor, 0.2, 5.0);
            currentStepScaleFactor = 1.0;

            // Sätt ny transform
            QTransform t;
            t.scale(scaleFactor, scaleFactor);
            setTransform(t);

            // Justera scroll så gesture center stannar visuellt kvar
            setScrollBars(gestureCenterInScene,gestureCenterInView);
            return; // Vi är klara här
        }
        else if (event->state() == Qt::GestureUpdated) {
            // 4. Pågående gest: räkna ut temporär skala
            qreal sxy = scaleFactor * currentStepScaleFactor;
            sxy = std::clamp(sxy, 0.2, 5.0);

            // Om vi nått gräns – lås skalan och återställ steg
            if (sxy == 0.2 || sxy == 5.0) {
                scaleFactor = sxy;
                currentStepScaleFactor = 1.0;
            }

            // 5. Sätt transform för pågående skalning
            QTransform t;
            t.scale(sxy, sxy);
            setTransform(t);

            // 6. Justera scroll så pinch center stannar kvar
            setScrollBars(gestureCenterInScene,gestureCenterInView);
        }
    }
    void setScrollBars(QPointF gestureCenterInScene, QPointF gestureCenterInView) {
        const QPointF newGestureCenterInView = mapFromScene(gestureCenterInScene);
        const QPointF delta = gestureCenterInView - newGestureCenterInView;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
    }
    qreal currentStepScaleFactor = 1;
    qreal scaleFactor = 1;

};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
private:
    Ui::MainWindow *ui;
    QGraphicsScene Scene;
    int m_CurrentIndex = -1;
    QList<QMap<QString,QVariant>> m_ProjectList;
    void drawBefore(QGraphicsScene*, HighQualityImageItem&);
    void drawAfter(QGraphicsScene*, HighQualityImageItem&);
    HighQualityImageItem beforeImage;
    HighQualityImageItem afterImage;
    Anchors anchors;
    void updateValues();
    void updateProjects();
    void addProject();
    void loadProject(QString name = QString());
    void removeProject(QString);
    void removeCurrentProject();
    int indexFromName(QString name) {
        for (int i = 0; i < m_ProjectList.size(); i++) if (m_ProjectList[i].value("ProjectName").toString()==name) return i;
        return -1;
    }
    QVariant value(QString key) {
        return m_ProjectList[m_CurrentIndex].value(key);
    }
    int valueInt(QString key) {
        return value(key).toInt();
    }
    double valueDouble(QString key) {
        return value(key).toDouble();
    }
    QString valueString(QString key) {
        return value(key).toString();
    }
    QPointF valuePointF(QString key) {
        return value(key).toPointF();
    }
    void setValue(QString key, QVariant value) {
        m_ProjectList[m_CurrentIndex].insert(key, value);
    }
    bool valueExist(QString key, QVariant value){
        for (int i = 0; i < m_ProjectList.size(); i++) if (m_ProjectList[i].value(key)==value) return true;
        return false;
    }
    void computeMax();
    QTransform computePointTransform(std::array<Anchor,maxAnchors> after,std::array<Anchor,maxAnchors> before) {
        QTransform t;
        t.translate(before[0].x(), before[0].y());
        t.translate(-after[0].x(), -after[0].y());
        return t;
    }
    QTransform computeAlignTransform(std::array<Anchor,maxAnchors> after,std::array<Anchor,maxAnchors> before)
    {
        // Steg 1: Vektorer
        QPointF vAfter = after[1] - after[0];
        QPointF vBefore = before[1] - before[0];

        // Steg 2: Längder och skala
        const double lenAfter = qHypot(vAfter.x(), vAfter.y());
        const double lenBefore = qHypot(vBefore.x(), vBefore.y());
        const double scale = lenBefore / lenAfter;

        // Steg 3: Rotation i grader
        const double angleAfter = qAtan2(vAfter.y(), vAfter.x());
        const double angleBefore = qAtan2(vBefore.y(), vBefore.x());
        const double rotationRad = angleBefore - angleAfter;

        QTransform t;
        t.translate(before[0].x(), before[0].y());        // 3. flytta till slutposition
        t.rotateRadians(rotationRad);                           // 2. rotera
        t.scale(scale, scale);                        // 2. skala
        t.translate(-after[0].x(), -after[0].y());        // 1. flytta från startpunkt

        for (int i = 0; i < 2; i++) qDebug() << "Mapped after " << i + 1 << t.map(after[i]) << "Expected:" << before[i];
        return t;
    }
    QTransform computeAffineFromThreePoints(std::array<Anchor,maxAnchors> after,std::array<Anchor,maxAnchors> before)
    {
        /*
        QPolygonF srcPoly = {a1, a2, a3};
        QPolygonF dstPoly = {b1, b2, b3};
        QTransform q;
        bool ok = QTransform::quadToQuad(srcPoly, dstPoly, q);
        return q;
*/
        // Vi vill hitta en affinn transform T så att:
        //   T * a1 = b1
        //   T * a2 = b2
        //   T * a3 = b3
        //
        // Alltså vill vi lösa:
        //   [ x1 y1 1 0  0 0 ]   [m11]   = [x1']
        //   [ 0  0 0 x1 y1 1 ]   [m12]     [y1']
        //   [ x2 y2 1 0  0 0 ]   [m21]     ...
        //   [ 0  0 0 x2 y2 1 ]   [m22]
        //   [ x3 y3 1 0  0 0 ]   [dx ]
        //   [ 0  0 0 x3 y3 1 ]   [dy ]

        // Extrahera koordinater
        const double x1 = after[0].x(), y1 = after[0].y();
        const double x2 = after[1].x(), y2 = after[1].y();
        const double x3 = after[2].x(), y3 = after[2].y();

        const double u1 = before[0].x(), v1 = before[0].y();
        const double u2 = before[1].x(), v2 = before[1].y();
        const double u3 = before[2].x(), v3 = before[2].y();

        // Lös med Cramers regel eller Gauss-elimination – här direkt formelbaserat
        const double denom = x1*(y2 - y3) - y1*(x2 - x3) + (x2*y3 - x3*y2);

        if (std::abs(denom) < 1e-8) {
            qWarning("Points are colinear; cannot compute affine transform.");
            return QTransform(); // Identitet
        }

        // Lös koefficienterna
        const double a = ((u1*(y2 - y3) - u2*(y1 - y3) + u3*(y1 - y2)) / denom);
        const double b = ((u1*(x3 - x2) + u2*(x1 - x3) + u3*(x2 - x1)) / denom);
        const double c = ((u1*(x2*y3 - x3*y2) - u2*(x1*y3 - x3*y1) + u3*(x1*y2 - x2*y1)) / denom);

        const double d = ((v1*(y2 - y3) - v2*(y1 - y3) + v3*(y1 - y2)) / denom);
        const double e = ((v1*(x3 - x2) + v2*(x1 - x3) + v3*(x2 - x1)) / denom);
        const double f = ((v1*(x2*y3 - x3*y2) - v2*(x1*y3 - x3*y1) + v3*(x1*y2 - x2*y1)) / denom);

        QTransform t(a, d, b, e, c, f); // m11 m12 m21 m22 dx dy

        for (int i = 0; i < 3; i++) qDebug() << "Mapped after " << i + 1 << t.map(after[i]) << "Expected:" << before[i];
        return t;
    }
    QTransform computeAffineFromFourPoints(std::array<Anchor,maxAnchors> after,std::array<Anchor,maxAnchors> before)
    {
        QList<QPointF> a;
        QList<QPointF> b;
        for (int i = 0; i < 4; i++) {
            a.append(after[i]);
            b.append(before[i]);
        }
        QPolygonF srcPoly(a);
        QPolygonF dstPoly(b);
        QTransform q;
        QTransform::quadToQuad(srcPoly, dstPoly, q);
        return q;
    }
    void saveTransform(QTransform& t);
    void generateFolders(const QString& baseDirPath, const QStringList& projectNames);
    void generateHtmlGallery(const QString& baseDirPath, const QString& title);
private slots:
    void loadBefore();
    void loadAfter();
    void saveAfterDialog();
    void saveAfter(QString path);
    void toggleView();
    void updateLabel();
    void finger(QPointF);
    void setAnchorBefore(int index);
    void setAnchorAfter(int index);
    void clearAnchors();
    void computeAnchors(int index);
    void createWebGallery();
public slots:
    void updateFrame();
    void showEvent(QShowEvent*);
    void closeEvent(QCloseEvent*);
    void resizeEvent(QResizeEvent*);
};


#endif // MAINWINDOW_H
