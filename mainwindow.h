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

    void setImage(const QImage& image);
    void setTransformMatrix(const QTransform& transform);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

    void setViewMode(ViewMode mode);
    void setSplitFactor(qreal factor);
    QImage image;
    QPainterPath extra;
    QPen extraPen;
private:
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
    void loopForPoint(QPushButton* b, QPointF& p, QColor c) {
        loopPoint = p;
        looping = true;
        setButtonColor(b, Qt::red);
        QApplication::setOverrideCursor(Qt::PointingHandCursor);
        while (looping) {
            usleep(100);
            QApplication::processEvents();
        }
        QApplication::restoreOverrideCursor();
        if (loopPoint != QPointF(0,0)) {
            setButtonColor(b, c);
            p = loopPoint;
        }
        else {
            setButtonColor(b,Qt::transparent);
        }
    }
    void setButtonColor(QPushButton* b, QColor c) {
        b->setAutoFillBackground(true);
        QPalette palette = b->palette();
        palette.setColor(QPalette::Button, c);
        b->setPalette(palette);
    }
    QSizeF origSize;
signals:
    void fingerMoved(QPointF);
protected:
    virtual bool event(QEvent *event)
    {
        if (event->type() == QEvent::Gesture)
            return gestureEvent(static_cast<QGestureEvent*>(event));
        return QGraphicsView::event(event);
    }
    void keyPressEvent(QKeyEvent* /*event*/) {
        looping = false;
    }
    void closeEvent(QCloseEvent* /*event*/) {
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
            QPointF newGestureCenterInView = mapFromScene(gestureCenterInScene);
            QPointF delta = gestureCenterInView - newGestureCenterInView;
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
            verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());

            return; // Vi är klara här
        }

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
        QPointF newGestureCenterInView = mapFromScene(gestureCenterInScene);
        QPointF delta = gestureCenterInView - newGestureCenterInView;
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
    std::array<QPointF,4> anchorBefore;
    std::array<QPointF,4> anchorAfter;
    std::array<QPushButton*,4> beforeButtons;
    std::array<QPushButton*,4> afterButtons;
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
    bool computeEnabled(int count);
    void enableComputeButtons();
    void computeMax();
    QPainterPath anchorPoint(QPointF p, int i) {
        QPainterPath g;
        g.addEllipse(p.x(),p.y(),1,1);
        g.addEllipse(p.x()-2,p.y()-2,5,5);
        g.addText(p + QPointF(8,-8),QFont("Helvetica",8),QString::number(i));
        return g;
    }
    QPainterPath beforeAnchors() {
        QPainterPath g;
        for (int i = 0; i < 4; i++) g.addPath(anchorPoint(anchorBefore[i],i + 1));
        return g;
    }
    QPainterPath afterAnchors() {
        QPainterPath g;
        for (int i = 0; i < 4; i++) g.addPath(anchorPoint(anchorAfter[i],i + 1));
        return g;
    }
    QTransform computeAlignTransform(std::array<QPointF,4> after,std::array<QPointF,4> before)
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
    QTransform computeAffineFromThreePoints(std::array<QPointF,4> after,std::array<QPointF,4> before)
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
    QTransform computeAffineFromFourPoints(std::array<QPointF,4> after,std::array<QPointF,4> before)
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
private slots:
    void loadBefore();
    void loadAfter();
    void saveAfter();
    void toggleView();
    void updateLabel();
    void finger(QPointF);
    void setAnchorBefore(int index);
    void setAnchorAfter(int index);
    //void clearValues();
    void clearAnchors();
    void computeAnchorsFromTwo();
    void computeAnchorsFromThree();
    void computeAnchorsFromFour();
public slots:
    void updateFrame();
    void showEvent(QShowEvent*);
    void closeEvent(QCloseEvent*);
    void resizeEvent(QResizeEvent*);
};


#endif // MAINWINDOW_H
