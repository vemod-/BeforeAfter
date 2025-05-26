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
    QPointF loopForPoint(QPushButton* b, QColor c) {
        looping = true;
        setButtonColor(b, Qt::red);
        QApplication::setOverrideCursor(Qt::PointingHandCursor);
        while (looping) {
            usleep(100);
            QApplication::processEvents();
        }
        QApplication::restoreOverrideCursor();
        setButtonColor(b, Qt::transparent);
        if (loopPoint != QPointF(0,0)) setButtonColor(b, c);
        return loopPoint;
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
            //qDebug() << p;
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
        // 1. Hämta gesture center point i view-koordinater
        QPointF gestureCenterInView = event->centerPoint();
        QPointF gestureCenterInScene = mapToScene(gestureCenterInView.toPoint());

        // 2. Uppdatera senaste skalningsfaktor alltid
        currentStepScaleFactor = event->totalScaleFactor();

        if (event->state() == Qt::GestureFinished) {
            scaleFactor *= currentStepScaleFactor;
            currentStepScaleFactor = 1.0;
        }

        // 3. Beräkna total skala
        qreal sxy = scaleFactor * currentStepScaleFactor;
        sxy = std::clamp(sxy, 0.2, 5.0);

        // Om vi nått gräns – lås skalan och återställ step
        if (sxy == 0.2 || sxy == 5.0) {
            scaleFactor = sxy;
            currentStepScaleFactor = 1.0;
        }

        // 4. Zooma med transform
        QTransform t;
        t.scale(sxy, sxy);
        setTransform(t);

        // 5. Justera position så att gesture center hålls kvar
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
    enum ViewMode {
        EditView,
        SplitView,
        HSplitView
    };

private:
    Ui::MainWindow *ui;
    QGraphicsScene Scene;
    int m_CurrentIndex = -1;
    QList<QMap<QString,QVariant>> m_ProjectList;
    QGraphicsPixmapItem* drawBefore(QGraphicsScene*,QPixmap);
    QGraphicsPixmapItem* drawAfter(QGraphicsScene*,QPixmap);
    QPixmap beforePix;
    QPixmap afterPix;
    QPointF anchorBefore1;
    QPointF anchorBefore2;
    QPointF anchorBefore3;
    QPointF anchorAfter1;
    QPointF anchorAfter2;
    QPointF anchorAfter3;
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
    void setValue(QString key, QVariant value) {
        m_ProjectList[m_CurrentIndex].insert(key, value);
    }
    bool valueExist(QString key, QVariant value){
        for (int i = 0; i < m_ProjectList.size(); i++) if (m_ProjectList[i].value(key)==value) return true;
        return false;
    }
    bool compute2Enabled();
    bool compute3Enabled();
    void enableComputeButtons();
    void drawPoint(QPointF p, QPainter& pt) {
        pt.drawPoint(p);
        pt.drawEllipse(p,5,5);
    }
    const QPixmap pixmapWithBeforeAnchors(QPixmap& pix) {
        QPixmap p(pix);
        QPainter pt(&p);
        pt.setPen(Qt::yellow);
        drawPoint(anchorBefore1,pt);
        drawPoint(anchorBefore2,pt);
        drawPoint(anchorBefore3,pt);
        return p;
    }
    const QPixmap pixmapWithAfterAnchors(QPixmap& pix) {
        QPixmap p(pix);
        QPainter pt(&p);
        pt.setPen(Qt::green);
        drawPoint(anchorAfter1,pt);
        drawPoint(anchorAfter2,pt);
        drawPoint(anchorAfter3,pt);
        return p;
    }
    QTransform computeAlignTransform(QPointF after1, QPointF after2, QPointF before1, QPointF before2)
    {
        // Steg 1: Vektorer
        QPointF vAfter = after2 - after1;
        QPointF vBefore = before2 - before1;

        // Steg 2: Längder och skala
        double lenAfter = std::hypot(vAfter.x(), vAfter.y());
        double lenBefore = std::hypot(vBefore.x(), vBefore.y());
        double scale = lenBefore / lenAfter;

        // Steg 3: Rotation i grader
        double angleAfter = std::atan2(vAfter.y(), vAfter.x());
        double angleBefore = std::atan2(vBefore.y(), vBefore.x());
        double rotation = (angleBefore - angleAfter) * 180.0 / M_PI;

        QTransform t;
        t.translate(before1.x(), before1.y());        // 3. flytta till slutposition
        t.rotate(rotation);                           // 2. rotera
        t.scale(scale, scale);                        // 2. skala
        t.translate(-after1.x(), -after1.y());        // 1. flytta från startpunkt

        qDebug() << "Mapped after1:" << t.map(after1) << "Expected:" << before1;
        qDebug() << "Mapped after2:" << t.map(after2) << "Expected:" << before2;
        return t;
    }
    QTransform computeAffineFromThreePoints(QPointF a1, QPointF a2, QPointF a3,
                                            QPointF b1, QPointF b2, QPointF b3)
    {
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
        double x1 = a1.x(), y1 = a1.y();
        double x2 = a2.x(), y2 = a2.y();
        double x3 = a3.x(), y3 = a3.y();

        double u1 = b1.x(), v1 = b1.y();
        double u2 = b2.x(), v2 = b2.y();
        double u3 = b3.x(), v3 = b3.y();

        // Lös med Cramers regel eller Gauss-elimination – här direkt formelbaserat
        double denom = x1*(y2 - y3) - y1*(x2 - x3) + (x2*y3 - x3*y2);

        if (std::abs(denom) < 1e-8) {
            qWarning("Points are colinear; cannot compute affine transform.");
            return QTransform(); // Identitet
        }

        // Lös koefficienterna
        double a = ((u1*(y2 - y3) - u2*(y1 - y3) + u3*(y1 - y2)) / denom);
        double b = ((u1*(x3 - x2) + u2*(x1 - x3) + u3*(x2 - x1)) / denom);
        double c = ((u1*(x2*y3 - x3*y2) - u2*(x1*y3 - x3*y1) + u3*(x1*y2 - x2*y1)) / denom);

        double d = ((v1*(y2 - y3) - v2*(y1 - y3) + v3*(y1 - y2)) / denom);
        double e = ((v1*(x3 - x2) + v2*(x1 - x3) + v3*(x2 - x1)) / denom);
        double f = ((v1*(x2*y3 - x3*y2) - v2*(x1*y3 - x3*y1) + v3*(x1*y2 - x2*y1)) / denom);

        QTransform t(a, d, b, e, c, f); // m11 m12 m21 m22 dx dy

        qDebug() << "Mapped after1:" << t.map(a1) << "Expected:" << b1;
        qDebug() << "Mapped after2:" << t.map(a2) << "Expected:" << b2;
        qDebug() << "Mapped after3:" << t.map(a3) << "Expected:" << b3;

        return t;
    }
private slots:
    void loadBefore();
    void loadAfter();
    void saveAfter();
    void toggleView();
    void updateLabel();
    void finger(QPointF);
    void setAnchorBefore1();
    void setAnchorBefore2();
    void setAnchorBefore3();
    void setAnchorAfter1();
    void setAnchorAfter2();
    void setAnchorAfter3();
    void clearValues();
    void clearAnchors();
    void computeAnchors();
    void computeAnchorsFromThree();
public slots:
    void updateFrame();
    void showEvent(QShowEvent*);
    void closeEvent(QCloseEvent*);
    void resizeEvent(QResizeEvent*);
};


#endif // MAINWINDOW_H
