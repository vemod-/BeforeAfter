#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QSettings>
#include <QMessageBox>
#include <QInputDialog>
#include <QCloseEvent>
#include <qmath.h>

HighQualityImageItem::HighQualityImageItem(const QImage& image, QGraphicsItem* parent)
    : QGraphicsItem(parent), image(image), m_transform()
{
    setFlag(QGraphicsItem::ItemIgnoresTransformations, false);
}

void HighQualityImageItem::setImage(const QImage& i)
{
    prepareGeometryChange();
    image = i;
    update();
}

void HighQualityImageItem::setTransformMatrix(const QTransform& transform)
{
    prepareGeometryChange();
    m_transform = transform;
    update();
}

QRectF HighQualityImageItem::boundingRect() const
{
    return m_transform.mapRect(QRectF(QPointF(0, 0), image.size()));
}

void HighQualityImageItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    painter->save();
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setTransform(m_transform, true);

    QRectF imageRect(0, 0, image.width(), image.height());

    if (m_viewMode == ViewMode::EditView) {
        painter->setOpacity(m_splitFactor);
        painter->drawImage(QPointF(0, 0), image);
    } else if (m_viewMode == ViewMode::SplitView) {
        QRectF splitRect = imageRect;
        splitRect.setRight(splitRect.left() + imageRect.width() * m_splitFactor);
        painter->setClipRect(splitRect);
        painter->drawImage(QPointF(0, 0), image);
    } else if (m_viewMode == ViewMode::HSplitView) {
        QRectF splitRect = imageRect;
        splitRect.setBottom(splitRect.top() + imageRect.height() * m_splitFactor);
        painter->setClipRect(splitRect);
        painter->drawImage(QPointF(0, 0), image);
    } else {
        painter->drawImage(QPointF(0, 0), image);
    }

    painter->setPen(extraPen);
    painter->drawPath(extra);
    painter->restore();
}

void HighQualityImageItem::setViewMode(ViewMode mode)
{
    m_viewMode = mode;
    update();
}

void HighQualityImageItem::setSplitFactor(qreal factor)
{
    m_splitFactor = std::clamp(factor, 0.0, 1.0);
    update();
}

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
    for (int i = 0; i < 4; ++i) {
        beforeButtons[i] = findChild<QPushButton*>(QString("AnchorBefore%1Button").arg(i + 1));
        afterButtons[i]  = findChild<QPushButton*>(QString("AnchorAfter%1Button").arg(i + 1));

        if (i < anchorCount) {
            connect(beforeButtons[i], &QPushButton::clicked, this, [this, i]() {
                setAnchorBefore(i);
            });
            connect(afterButtons[i], &QPushButton::clicked, this, [this, i]() {
                setAnchorAfter(i);
            });
        }
        else {
            beforeButtons[i]->setVisible(false);
            afterButtons[i]->setVisible(false);
        }
    }
    connect(ui->Compute2AnchorsButton,&QPushButton::clicked,this,&MainWindow::computeAnchorsFromTwo);
    connect(ui->Compute3AnchorsButton,&QPushButton::clicked,this,&MainWindow::computeAnchorsFromThree);
    connect(ui->Compute4AnchorsButton,&QPushButton::clicked,this,&MainWindow::computeAnchorsFromFour);
    ui->Compute4AnchorsButton->setVisible(false);
    connect(ui->ClearButton,&QPushButton::clicked,this,&MainWindow::clearAnchors);
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

        for (int i = 0; i < anchorCount; ++i) {
            setValue(QString("AnchorBefore%1").arg(i + 1), anchorBefore[i]);
            setValue(QString("AnchorAfter%1").arg(i + 1), anchorAfter[i]);
        }
    }
}

void MainWindow::updateFrame()
{
    updateValues();
    for (QGraphicsItem* i : (const QList<QGraphicsItem*>)Scene.items()) Scene.removeItem(i);
    ui->MainView->origSize = beforeImage.image.size();
    QGraphicsScene* s = &Scene;
    afterImage.extra = afterAnchors();
    afterImage.extraPen = Qt::green;
    drawAfter(s,afterImage);
    beforeImage.extra = beforeAnchors();
    beforeImage.extraPen = Qt::yellow;
    drawBefore(s,beforeImage);
    beforeImage.setViewMode((ViewMode)valueInt("ViewMode"));
    beforeImage.setSplitFactor(valueDouble("Transparancy"));
}

void MainWindow::drawBefore(QGraphicsScene* s, HighQualityImageItem& i) {
    s->addItem(&i);
}

void MainWindow::drawAfter(QGraphicsScene* s, HighQualityImageItem& i) {
    s->addItem(&i);
    QTransform t;
    t.translate(valueDouble("HTranslate"), valueDouble("VTranslate"));
    t.shear(valueDouble("HShear"), valueDouble("VShear"));
    t.scale(valueDouble("HScale"), valueDouble("VScale"));
    t.rotate(valueDouble("XRotate"), Qt::XAxis);
    t.rotate(valueDouble("YRotate"), Qt::YAxis);
    t.rotate(valueDouble("Rotate"), Qt::ZAxis);
    i.setTransform(t);
    s->setSceneRect(beforeImage.image.rect().united(i.mapRectToScene(i.boundingRect()).toRect()));
}

void MainWindow::loadBefore()
{
    QString p = QFileDialog::getOpenFileName(this, tr("Open Image"), "", tr("Image Files (*.jpg *.jpeg)"));
    if (!p.isEmpty())
    {
        beforeImage.image.load(p);
        setValue("BeforePix",p);
    }
}

void MainWindow::loadAfter()
{
    QString p = QFileDialog::getOpenFileName(this, tr("Open Image"), "", tr("Image Files (*.jpg *.jpeg)"));
    if (!p.isEmpty())
    {
        afterImage.image.load(p);
        setValue("AfterPix",p);
    }
    updateFrame();
}

void MainWindow::saveAfter()
{
    QImage outImage( beforeImage.image.size(), QImage::Format_RGB32);
    outImage.fill(Qt::white);
    QPainter painter(&outImage);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    QGraphicsScene s(beforeImage.image.rect());
    afterImage.extra.clear();
    drawAfter(&s, afterImage);
    s.render(&painter, outImage.rect(), outImage.rect());
    QString path = QFileDialog::getSaveFileName(this, tr("Save Image"), "", tr("Image Files (*.jpg *.jpeg *.png)"));
    if (!path.isEmpty()) outImage.save(path);
    for (QGraphicsItem* i : (const QList<QGraphicsItem*>)s.items()) s.removeItem(i);
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
    QString s = "Transparancy";
    if (valueInt("ViewMode") == ViewMode::SplitView) s = "Vertical Split";
    if (valueInt("ViewMode") == ViewMode::HSplitView) s = "Horizontal Split";
    ui->TransparancyLabel->setText(s);
}

void MainWindow::loadProject(QString name)
{
    if (!name.isEmpty()) m_CurrentIndex = indexFromName(name);
    beforeImage.image.load(valueString("BeforePix"));
    afterImage.image.load(valueString("AfterPix"));

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

    for (int i = 0; i < anchorCount; ++i) {
        anchorBefore[i] = valuePointF(QString("AnchorBefore%1").arg(i + 1));
        anchorAfter[i] = valuePointF(QString("AnchorAfter%1").arg(i + 1));

        if (!anchorBefore[i].isNull())
            ui->MainView->setButtonColor(beforeButtons[i], Qt::yellow);
        if (!anchorAfter[i].isNull())
            ui->MainView->setButtonColor(afterButtons[i], Qt::green);
    }
    enableComputeButtons();

    updateLabel();
    updateFrame();
    updateProjects();
    ui->ProjectCombo->blockSignals(true);
    ui->ProjectCombo->setCurrentText(valueString("ProjectName"));
    ui->ProjectCombo->blockSignals(false);
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
        //beforePix.load(p);
        beforeImage.image.load(p);
        setValue("BeforePix",p);
        loadProject();
        loadAfter();
    }
}

void MainWindow::removeCurrentProject()
{
    removeProject(ui->ProjectCombo->currentText());
}

bool MainWindow::computeEnabled(int count) {
    for (int i = 0; i < count; i++) {
        if (anchorBefore[i] == QPointF(0,0)) return false;
        if (anchorAfter[i] == QPointF(0,0)) return false;
    }
    return true;
}

void MainWindow::enableComputeButtons() {
    ui->Compute2AnchorsButton->setEnabled(computeEnabled(2));
    ui->Compute3AnchorsButton->setEnabled(computeEnabled(3));
}

void MainWindow::computeMax() {
    //if (computeEnabled(4)) {
    //    computeAnchorsFromFour();
    //}
    if (computeEnabled(3)) {
        computeAnchorsFromThree();
    }
    else if (computeEnabled(2)) {
        computeAnchorsFromTwo();
    }
}

void MainWindow::saveTransform(QTransform &t) {
    ui->HTranslateSpinBox->setValueSilent(t.dx());
    ui->VTranslateSpinBox->setValueSilent(t.dy());

    const double rotationRad = qAtan2(t.m12(), t.m11());
    const double rotationDeg = qRadiansToDegrees(rotationRad);
    ui->RotateSpinBox->setValueSilent(rotationDeg);
    QTransform pure = QTransform(t).rotateRadians(-rotationRad);

    const double scaleX = pure.m11();
    const double scaleY = pure.m22();
    ui->HScaleSpinBox->setValueSilent(scaleX);
    ui->VScaleSpinBox->setValueSilent(scaleY);
    pure = QTransform(pure).scale(1.0/scaleX,1.0/scaleY);

    const double shearX = pure.m21();
    const double shearY = pure.m12();
    ui->HShearSpinBox->setValueSilent(shearX);
    ui->VShearSpinBox->setValueSilent(shearY);
    pure = QTransform(pure).shear(-shearX,-shearY);

    QTransform test;
    test.translate(t.dx(),t.dy());
    test.shear(shearX,shearY);
    test.scale(scaleX, scaleY);
    test.rotate(rotationDeg, Qt::ZAxis);
    QTransform v;
    v.translate(ui->HTranslateSpinBox->value(), ui->VTranslateSpinBox->value());
    v.shear(ui->HShearSpinBox->value(), ui->VShearSpinBox->value());
    v.scale(ui->HScaleSpinBox->value(), ui->VScaleSpinBox->value());
    v.rotate(ui->RotateSpinBox->value(), Qt::ZAxis);
    qDebug() << t;
    qDebug() << test;
    qDebug() << v;
    qDebug() << pure;
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

void MainWindow::setAnchorBefore(int index) {
    for (QGraphicsItem* i : (const QList<QGraphicsItem*>)Scene.items()) Scene.removeItem(i);
    ui->MainView->origSize = beforeImage.image.size();
    QGraphicsScene* s = &Scene;
    beforeImage.extra = beforeAnchors();
    beforeImage.extraPen = Qt::yellow;
    beforeImage.setSplitFactor(1);
    s->addItem(&beforeImage);
    ui->MainView->loopForPoint(beforeButtons[index], anchorBefore[index], Qt::yellow);
    enableComputeButtons();
    computeMax();
    updateFrame();
}

void MainWindow::setAnchorAfter(int index) {
    for (QGraphicsItem* i : (const QList<QGraphicsItem*>)Scene.items()) Scene.removeItem(i);
    ui->MainView->origSize = beforeImage.image.size();
    QGraphicsScene* s = &Scene;
    afterImage.extra = afterAnchors();
    afterImage.extraPen = Qt::green;
    afterImage.setTransform(QTransform());
    s->addItem(&afterImage);
    ui->MainView->loopForPoint(afterButtons[index], anchorAfter[index], Qt::green);
    enableComputeButtons();
    computeMax();
    updateFrame();
}
/*
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
    updateFrame();
}
*/
void MainWindow::clearAnchors() {
    for (int i = 0; i < anchorCount; i++) {
        anchorBefore[i] = QPointF(0,0);
        anchorAfter[i] = QPointF(0,0);
        ui->MainView->setButtonColor(beforeButtons[i],Qt::transparent);
        ui->MainView->setButtonColor(afterButtons[i],Qt::transparent);
    }
    ui->HTranslateSpinBox->setValueSilent(0);
    ui->VTranslateSpinBox->setValueSilent(0);
    ui->HShearSpinBox->setValueSilent(0);
    ui->VShearSpinBox->setValueSilent(0);
    ui->HScaleSpinBox->setValueSilent(1);
    ui->VScaleSpinBox->setValueSilent(1);
    ui->RotateSpinBox->setValueSilent(0);
    ui->XRotateSpinBox->setValueSilent(0);
    ui->YRotateSpinBox->setValueSilent(0);
    updateFrame();
}

void MainWindow::computeAnchorsFromTwo() {
    QTransform t = computeAlignTransform(anchorAfter, anchorBefore);
    saveTransform(t);
    updateFrame();
}

void MainWindow::computeAnchorsFromThree()
{
    QTransform t = computeAffineFromThreePoints(anchorAfter, anchorBefore);
    saveTransform(t);
    updateFrame();
}

void MainWindow::computeAnchorsFromFour() {
    QTransform t = computeAffineFromFourPoints(anchorAfter, anchorBefore);
    saveTransform(t);
    updateFrame();
}

