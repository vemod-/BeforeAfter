#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QSettings>
#include <QMessageBox>
#include <QInputDialog>
#include <QCloseEvent>
#include <qmath.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->MainView->setScene(&Scene);
    connect(ui->LoadAfterButton,&QToolButton::clicked,this,&MainWindow::loadAfter);
    connect(ui->SaveAfterButton,&QToolButton::clicked,this,&MainWindow::saveAfter);
    connect(ui->AddProjectToolButton,&QToolButton::clicked,this,&MainWindow::addProject);
    connect(ui->RemoveProjectToolButton,&QToolButton::clicked,this,&MainWindow::removeCurrentProject);
    connect(ui->ProjectCombo,&QComboBox::currentTextChanged,this,&MainWindow::loadProject);
    connect(ui->ToggleViewButton,&QPushButton::clicked,this,&MainWindow::toggleView);
    connect(ui->HTranslateSpinBox,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,&MainWindow::updateFrame);
    connect(ui->VTranslateSpinBox,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,&MainWindow::updateFrame);
    connect(ui->HShearSpinBox,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,&MainWindow::updateFrame);
    connect(ui->VShearSpinBox,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,&MainWindow::updateFrame);
    connect(ui->RotateSpinBox,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,&MainWindow::updateFrame);
    connect(ui->XRotateSpinBox,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,&MainWindow::updateFrame);
    connect(ui->YRotateSpinBox,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,&MainWindow::updateFrame);
    connect(ui->HScaleSpinBox,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,&MainWindow::updateFrame);
    connect(ui->VScaleSpinBox,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,&MainWindow::updateFrame);
    connect(ui->TransparancySpinBox,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,&MainWindow::updateFrame);
    connect(ui->MainView,&QGraphicsViewX::fingerMoved,this,&MainWindow::finger);
    connect(ui->AnchorBefore1Button,&QPushButton::clicked,this,&MainWindow::setAnchorBefore1);
    connect(ui->AnchorBefore2Button,&QPushButton::clicked,this,&MainWindow::setAnchorBefore2);
    connect(ui->AnchorBefore3Button,&QPushButton::clicked,this,&MainWindow::setAnchorBefore3);
    connect(ui->AnchorAfter1Button,&QPushButton::clicked,this,&MainWindow::setAnchorAfter1);
    connect(ui->AnchorAfter2Button,&QPushButton::clicked,this,&MainWindow::setAnchorAfter2);
    connect(ui->AnchorAfter3Button,&QPushButton::clicked,this,&MainWindow::setAnchorAfter3);
    connect(ui->Compute2AnchorsButton,&QPushButton::clicked,this,&MainWindow::computeAnchors);
    connect(ui->Compute3AnchorsButton,&QPushButton::clicked,this,&MainWindow::computeAnchorsFromThree);
    connect(ui->ClearButton,&QPushButton::clicked,this,&MainWindow::clearValues);
    ui->Compute3AnchorsButton->setEnabled(false);
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    QSettings s("Veinge Musik och Data","BeforeAfter");
    this->setGeometry(s.value("Rect").toRect());
    m_CurrentIndex = s.value("CurrentIndex",-1).toInt();
    int size = s.beginReadArray("Projects");
    for (int i = 0; i < size; i++)
    {
        s.setArrayIndex(i);
        QMap p = (s.value("Project").toMap());
        m_ProjectList.append(p);
    }
    s.endArray();
    if (m_ProjectList.isEmpty())
    {
        addProject();
    }
    else
    {
        if (m_CurrentIndex == -1) m_CurrentIndex = 0;
        loadProject();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings s("Veinge Musik och Data","BeforeAfter");
    s.setValue("Rect",this->geometry());
    s.setValue("CurrentIndex",m_CurrentIndex);
    s.beginWriteArray("Projects");
    for (int i = 0; i < m_ProjectList.size(); i++)
    {
        s.setArrayIndex(i);
        s.setValue("Project",m_ProjectList[i]);
    }
    s.endArray();
    QMainWindow::closeEvent(event);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    if (m_CurrentIndex > -1) updateFrame();
}

void MainWindow::updateValues()
{
    if (m_CurrentIndex > -1) {
        setValue("HTranslate", ui->HTranslateSpinBox->value());
        setValue("VTranslate", ui->VTranslateSpinBox->value());
        setValue("HShear", ui->HShearSpinBox->value());
        setValue("VShear", ui->VShearSpinBox->value());
        setValue("Transparancy", ui->TransparancySpinBox->value());
        setValue("HScale", ui->HScaleSpinBox->value());
        setValue("VScale", ui->VScaleSpinBox->value());
        setValue("Rotate", ui->RotateSpinBox->value());
        setValue("XRotate", ui->XRotateSpinBox->value());
        setValue("YRotate", ui->YRotateSpinBox->value());

        setValue("AnchorBefore1",anchorBefore1);
        setValue("AnchorBefore2",anchorBefore2);
        setValue("AnchorBefore3",anchorBefore3);
        setValue("AnchorAfter1",anchorAfter1);
        setValue("AnchorAfter2",anchorAfter2);
        setValue("AnchorAfter3",anchorAfter3);
    }
}

void MainWindow::updateFrame()
{
    updateValues();
    Scene.clear();
    //Scene.setSceneRect(beforePix.rect());
    ui->MainView->origSize = beforePix.size();
    QGraphicsScene* s = &Scene; //new QGraphicsScene(beforePix.rect());
    QGraphicsPixmapItem* i = drawBefore(s,pixmapWithBeforeAnchors(beforePix));
    QGraphicsPixmapItem* i1 = drawAfter(s,pixmapWithAfterAnchors(afterPix));
    if (valueInt("ViewMode") == EditView)
    {
        i1->setOpacity(1.0 - valueDouble("Transparancy"));
    }
    else
    {
        i->setZValue(1);
        QRect r = i->pixmap().rect();
        if (valueInt("ViewMode") == SplitView)
            r.setRight(r.right() * valueDouble("Transparancy"));
        else if (valueInt("ViewMode") == HSplitView)
            r.setBottom(r.bottom() * valueDouble("Transparancy"));
        i->setPixmap(i->pixmap().copy(r));
    }
}

QGraphicsPixmapItem* MainWindow::drawBefore(QGraphicsScene* s, QPixmap p)
{
    return s->addPixmap(p);
}

QGraphicsPixmapItem* MainWindow::drawAfter(QGraphicsScene* s, QPixmap p)
{
    QGraphicsPixmapItem* i = s->addPixmap(p);
    QTransform t;
    t.translate(valueDouble("HTranslate"), valueDouble("VTranslate"));
    t.rotate(valueDouble("XRotate"), Qt::XAxis);
    t.rotate(valueDouble("YRotate"), Qt::YAxis);
    t.rotate(valueDouble("Rotate"), Qt::ZAxis);
    t.shear(valueDouble("HShear"), valueDouble("VShear"));
    t.scale(valueDouble("HScale"), valueDouble("VScale"));
    i->setTransform(t);
    s->setSceneRect(beforePix.rect().united(i->sceneBoundingRect().toRect()));
    return i;
}

void MainWindow::loadBefore()
{
    QString p = QFileDialog::getOpenFileName(this, tr("Open Image"), "", tr("Image Files (*.jpg *.jpeg)"));
    if (!p.isEmpty())
    {
        beforePix.load(p);
        setValue("BeforePix",p);
    }
}

void MainWindow::loadAfter()
{
    QString p = QFileDialog::getOpenFileName(this, tr("Open Image"), "", tr("Image Files (*.jpg *.jpeg)"));
    if (!p.isEmpty())
    {
        afterPix.load(p);
        setValue("AfterPix",p);
    }
    updateFrame();
}

void MainWindow::saveAfter()
{
    QPixmap pix(beforePix.size());
    QPainter painter(&pix);
    //painter.setRenderHints(QPainter::Antialiasing | QPainter::LosslessImageRendering | QPainter::SmoothPixmapTransform);
    QGraphicsScene s = QGraphicsScene(beforePix.rect());
    drawBefore(&s,beforePix);
    drawAfter(&s,afterPix);
    s.render(&painter,pix.rect(),pix.rect());
    QString path = QFileDialog::getSaveFileName(this, tr("Save Image"), "", tr("Image Files (*.jpg *.jpeg)"));
    if (!path.isEmpty())
    {
        pix.copy(beforePix.rect()).save(path);
    }
}

void MainWindow::toggleView()
{
    int i = valueInt("ViewMode");
    i++;
    if (i > HSplitView) i = 0;
    setValue("ViewMode", static_cast<ViewMode>(i));
    updateLabel();
    updateFrame();
}

void MainWindow::updateLabel()
{
    if (valueInt("ViewMode") == EditView)
        ui->TransparancyLabel->setText("Transparancy");
    else
        ui->TransparancyLabel->setText("Split");

}

void MainWindow::loadProject(QString name)
{
    if (!name.isEmpty()) m_CurrentIndex = indexFromName(name);
    beforePix.load(valueString("BeforePix"));
    afterPix.load(valueString("AfterPix"));

    ui->HTranslateSpinBox->setValueSilent(valueDouble("HTranslate"));
    ui->VTranslateSpinBox->setValueSilent(valueDouble("VTranslate"));
    ui->HShearSpinBox->setValueSilent(valueDouble("HShear"));
    ui->VShearSpinBox->setValueSilent(valueDouble("VShear"));
    ui->TransparancySpinBox->setValueSilent(valueDouble("Transparancy"));
    ui->HScaleSpinBox->setValueSilent(valueDouble("HScale"));
    ui->VScaleSpinBox->setValueSilent(valueDouble("VScale"));
    ui->RotateSpinBox->setValueSilent(valueDouble("Rotate"));
    ui->XRotateSpinBox->setValueSilent(valueDouble("XRotate"));
    ui->YRotateSpinBox->setValueSilent(valueDouble("YRotate"));

    clearAnchors();

    anchorBefore1 = valuePointF("AnchorBefore1");
    anchorBefore2 = valuePointF("AnchorBefore2");
    anchorBefore3 = valuePointF("AnchorBefore3");
    anchorAfter1 = valuePointF("AnchorAfter1");
    anchorAfter2 = valuePointF("AnchorAfter2");
    anchorAfter3 = valuePointF("AnchorAfter3");
    if (anchorBefore1 != QPointF(0,0)) ui->MainView->setButtonColor(ui->AnchorBefore1Button,Qt::yellow);
    if (anchorBefore2 != QPointF(0,0)) ui->MainView->setButtonColor(ui->AnchorBefore2Button,Qt::yellow);
    if (anchorBefore3 != QPointF(0,0)) ui->MainView->setButtonColor(ui->AnchorBefore3Button,Qt::yellow);
    if (anchorAfter1 != QPointF(0,0)) ui->MainView->setButtonColor(ui->AnchorAfter1Button,Qt::green);
    if (anchorAfter2 != QPointF(0,0)) ui->MainView->setButtonColor(ui->AnchorAfter2Button,Qt::green);
    if (anchorAfter3 != QPointF(0,0)) ui->MainView->setButtonColor(ui->AnchorAfter3Button,Qt::green);

    ui->Compute2AnchorsButton->setEnabled(compute2Enabled());
    ui->Compute3AnchorsButton->setEnabled(compute3Enabled());

    updateLabel();
    updateFrame();
    updateProjects();
    ui->ProjectCombo->blockSignals(true);
    ui->ProjectCombo->setCurrentText(valueString("ProjectName"));
    ui->ProjectCombo->blockSignals(false);
    //clearAnchors();
}

void MainWindow::updateProjects()
{
    ui->ProjectCombo->blockSignals(true);
    ui->ProjectCombo->clear();
    for (int i = 0; i < m_ProjectList.size(); i++) ui->ProjectCombo->addItem(m_ProjectList[i].value("ProjectName").toString());
    ui->ProjectCombo->blockSignals(false);
}

void MainWindow::addProject()
{
    bool ok;
    QString text;
    do {
        text = QInputDialog::getText(this, "Project name",
                                             "Project name:", QLineEdit::Normal,
                                             QDir::home().dirName(), &ok);
        if (!ok) return;
    }
    while (valueExist("ProjectName",text) || text.isEmpty());

    QString p = QFileDialog::getOpenFileName(this, tr("Open Image"), "", tr("Image Files (*.jpg *.jpeg)"));
    if (!p.isEmpty())
    {
        QMap<QString,QVariant> proj;
        proj.insert("ProjectName",text);
        m_ProjectList.append(proj);
        m_CurrentIndex = indexFromName(text);
        setValue("HTranslate",0);
        setValue("VTranslate",0);
        setValue("HShear",0);
        setValue("VShear",0);
        setValue("Rotate",0);
        setValue("XRotate",0);
        setValue("YRotate",0);
        setValue("ViewMode",0);
        setValue("Transparancy",0.5);
        setValue("HScale",1);
        setValue("VScale",1);
        beforePix.load(p);
        setValue("BeforePix",p);
        loadProject();
        loadAfter();
    }
}

void MainWindow::removeCurrentProject()
{
    removeProject(ui->ProjectCombo->currentText());
}

bool MainWindow::compute2Enabled() {
    if (anchorBefore1 != QPointF(0,0)) {
        if (anchorBefore2 != QPointF(0,0)) {
            if (anchorAfter1 != QPointF(0,0)) {
                if (anchorAfter2 != QPointF(0,0)) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool MainWindow::compute3Enabled() {
    if (anchorBefore3 != QPointF(0,0)) {
        if (anchorAfter3 != QPointF(0,0)) {
            if (compute2Enabled()) {
                return true;
            }
        }
    }
    return false;
}

void MainWindow::enableComputeButtons() {
    ui->Compute2AnchorsButton->setEnabled(compute2Enabled());
    ui->Compute3AnchorsButton->setEnabled(compute3Enabled());
}

void MainWindow::saveTransform(QTransform &t) {
    // Translation
    ui->HTranslateSpinBox->setValueSilent(t.dx());
    ui->VTranslateSpinBox->setValueSilent(t.dy());
    // 1. Extrahera skala
    const double scaleX = std::hypot(t.m11(), t.m12());
    const double scaleY = std::hypot(t.m21(), t.m22());

    ui->HScaleSpinBox->setValueSilent(scaleX);
    ui->VScaleSpinBox->setValueSilent(scaleY);

    // 2. Extrahera rotation (radians)
    const double rotationRad = std::atan2(t.m12(), t.m11());
    const double rotationDeg = rotationRad * 180.0 / M_PI;
    ui->RotateSpinBox->setValueSilent(rotationDeg);

    // 3. Ta bort rotationen från matrisen
    QTransform rotationOnly;
    rotationOnly.rotate(rotationDeg);
    const QTransform withoutRotation = rotationOnly.inverted() * t;
    // 4. Extrahera shear från den roterade transformen
    // Nu bör m21 (dvs shearX) vara ren
    const double shearX = (withoutRotation.m11() * withoutRotation.m21() + withoutRotation.m12() * withoutRotation.m22()) / (scaleX * scaleX);
    ui->HShearSpinBox->setValueSilent(shearX);

    // 5. VShear används sällan – kan sättas till 0
    ui->VShearSpinBox->setValueSilent(0);

    QTransform test;
    test.translate(t.dx(),t.dy());
    test.rotate(rotationDeg, Qt::ZAxis);
    test.shear(shearX,0);
    test.scale(scaleX, scaleY);
    QTransform v;
    v.translate(ui->HTranslateSpinBox->value(), ui->VTranslateSpinBox->value());
    v.rotate(ui->RotateSpinBox->value(), Qt::ZAxis);
    v.shear(ui->HShearSpinBox->value(), ui->VShearSpinBox->value());
    v.scale(ui->HScaleSpinBox->value(), ui->VScaleSpinBox->value());
    qDebug() << t;
    qDebug() << test;
    qDebug() << v;

}

void MainWindow::removeProject(QString name)
{
    foreach(QMap p, m_ProjectList) {
        if (p.value("ProjectName") == name)
        {
            QMessageBox msgBox;
            msgBox.setText("Remove Project.");
            msgBox.setInformativeText("Do you want to remove this Project?");
            msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
            msgBox.setDefaultButton(QMessageBox::Yes);
            int ret = msgBox.exec();
            if (ret == QMessageBox::Cancel) return;
            m_ProjectList.removeOne(p);
            if (m_ProjectList.size() == 0) {
                addProject();
                return;
            }
            if (m_CurrentIndex >= m_ProjectList.size()) m_CurrentIndex = m_ProjectList.size() - 1;
            loadProject();
        }
    }
}

void MainWindow::finger(QPointF p) {
    ViewMode v = static_cast<ViewMode>(valueInt("ViewMode"));
    if (v == SplitView) {
        ui->TransparancySpinBox->setValue(p.x());
    }
    if (v == HSplitView) {
        ui->TransparancySpinBox->setValue(p.y());
    }
    if (v == EditView) {
        ui->TransparancySpinBox->setValue((p.x()+p.y())*0.5);
    }
}

void MainWindow::setAnchorBefore1() {
    anchorBefore1 = ui->MainView->loopForPoint(ui->AnchorBefore1Button, Qt::yellow);
    ui->Compute2AnchorsButton->setEnabled(compute2Enabled());
    updateFrame();
}

void MainWindow::setAnchorBefore2() {
    anchorBefore2 = ui->MainView->loopForPoint(ui->AnchorBefore2Button, Qt::yellow);
    ui->Compute2AnchorsButton->setEnabled(compute2Enabled());
    updateFrame();
}

void MainWindow::setAnchorBefore3() {
    anchorBefore3 = ui->MainView->loopForPoint(ui->AnchorBefore3Button, Qt::yellow);
    ui->Compute3AnchorsButton->setEnabled(compute3Enabled());
    updateFrame();
}

void MainWindow::setAnchorAfter1() {
    anchorAfter1 = ui->MainView->loopForPoint(ui->AnchorAfter1Button, Qt::green);
    ui->Compute2AnchorsButton->setEnabled(compute2Enabled());
    updateFrame();
}

void MainWindow::setAnchorAfter2() {
    anchorAfter2 = ui->MainView->loopForPoint(ui->AnchorAfter2Button, Qt::green);
    ui->Compute2AnchorsButton->setEnabled(compute2Enabled());
    updateFrame();
}

void MainWindow::setAnchorAfter3() {
    anchorAfter3 = ui->MainView->loopForPoint(ui->AnchorAfter3Button, Qt::green);
    ui->Compute3AnchorsButton->setEnabled(compute3Enabled());
    updateFrame();
}

void MainWindow::clearValues() {
    ui->HTranslateSpinBox->setValueSilent(0);
    ui->VTranslateSpinBox->setValueSilent(0);
    ui->HShearSpinBox->setValueSilent(0);
    ui->VShearSpinBox->setValueSilent(0);
    ui->HScaleSpinBox->setValueSilent(1);
    ui->VScaleSpinBox->setValueSilent(1);
    ui->RotateSpinBox->setValueSilent(0);
    ui->XRotateSpinBox->setValueSilent(0);
    ui->YRotateSpinBox->setValueSilent(0);
    //clearAnchors();
    updateFrame();
}

void MainWindow::clearAnchors() {
    anchorBefore1 = QPointF(0,0);
    anchorBefore2 = QPointF(0,0);
    anchorBefore3 = QPointF(0,0);
    anchorAfter1 = QPointF(0,0);
    anchorAfter2 = QPointF(0,0);
    anchorAfter3 = QPointF(0,0);
    ui->MainView->setButtonColor(ui->AnchorBefore1Button,Qt::transparent);
    ui->MainView->setButtonColor(ui->AnchorBefore2Button,Qt::transparent);
    ui->MainView->setButtonColor(ui->AnchorBefore3Button,Qt::transparent);
    ui->MainView->setButtonColor(ui->AnchorAfter1Button,Qt::transparent);
    ui->MainView->setButtonColor(ui->AnchorAfter2Button,Qt::transparent);
    ui->MainView->setButtonColor(ui->AnchorAfter3Button,Qt::transparent);
}

void MainWindow::computeAnchors() {
    QTransform t = computeAlignTransform(anchorAfter1,anchorAfter2,anchorBefore1,anchorBefore2);
/*
    Scene.clear();
    Scene.setSceneRect(beforePix.rect());
    QGraphicsScene* s = &Scene; //new QGraphicsScene(beforePix.rect());
    QGraphicsPixmapItem* i = drawBefore(s);
    QPixmap p = afterPix;
    QGraphicsPixmapItem* i1 = s->addPixmap(p);
    i1->setTransform(t);
    i1->setOpacity(0.5);
    return;

    // Translation (enkelt)
    ui->HTranslateSpinBox->setValueSilent(t.dx());
    ui->VTranslateSpinBox->setValueSilent(t.dy());

    // Skala: längd på kolumnvektorer
    double scaleX = std::hypot(t.m11(), t.m12());
    double scaleY = std::hypot(t.m21(), t.m22());
    ui->HScaleSpinBox->setValueSilent(scaleX);
    ui->VScaleSpinBox->setValueSilent(scaleY);

    // Rotation: vinkel på första kolumn (X-axelns riktning)
    double rotationDeg = std::atan2(t.m12(), t.m11()) * 180.0 / M_PI;
    ui->RotateSpinBox->setValueSilent(rotationDeg);

    // Skjuvning:
    // shearX: hur mycket Y påverkas av X (dvs horisontell skjuvning)
    double shearX = (t.m11() * t.m21() + t.m12() * t.m22()) / (scaleX * scaleX);
    ui->HShearSpinBox->setValueSilent(shearX);

    // Vertikal skjuvning är inte trivial att extrahera om shearX ≠ 0
    // För enkelhet: sätt till 0 (om du inte använder VShear)
    ui->VShearSpinBox->setValueSilent(0);
*/
    saveTransform(t);
    updateFrame();
}

void MainWindow::computeAnchorsFromThree()
{
    QTransform t = computeAffineFromThreePoints(
        anchorAfter1, anchorAfter2, anchorAfter3,
        anchorBefore1, anchorBefore2, anchorBefore3
        );
    saveTransform(t);
    updateFrame();
}

