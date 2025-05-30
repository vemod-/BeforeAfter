#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QSettings>
#include <QMessageBox>
#include <QInputDialog>
#include <QCloseEvent>
#include <qmath.h>
#include "cprojectdialog.h"

HighQualityImageItem::HighQualityImageItem(const QImage& image, QGraphicsItem* parent)
    : QGraphicsItem(parent), m_Image(image), m_transform()
{
    setFlag(QGraphicsItem::ItemIgnoresTransformations, false);
}

HighQualityImageItem::HighQualityImageItem(const QString &path, QGraphicsItem *parent) : QGraphicsItem(parent) {
    m_Image.load(path);
}

void HighQualityImageItem::setImage(const QImage& i)
{
    prepareGeometryChange();
    m_Image = i;
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
    return m_transform.mapRect(QRectF(QPointF(0,0), m_Image.size()));
}

void HighQualityImageItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    painter->save();
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setTransform(m_transform, true);

    QRectF imageRect(0, 0, m_Image.width(), m_Image.height());

    if (m_viewMode == ViewMode::EditView) {
        painter->setOpacity(m_splitFactor);
        painter->drawImage(QPointF(0,0), m_Image);
    } else if (m_viewMode == ViewMode::SplitView) {
        QRectF splitRect = imageRect;
        splitRect.setRight(splitRect.left() + imageRect.width() * m_splitFactor);
        painter->setClipRect(splitRect);
        painter->drawImage(QPointF(0,0), m_Image);
    } else if (m_viewMode == ViewMode::HSplitView) {
        QRectF splitRect = imageRect;
        splitRect.setBottom(splitRect.top() + imageRect.height() * m_splitFactor);
        painter->setClipRect(splitRect);
        painter->drawImage(QPointF(0,0), m_Image);
    } else {
        painter->drawImage(QPointF(0,0), m_Image);
    }

    painter->setPen(m_OverlayPen);
    painter->setBrush(m_OverlayBrush);
    painter->drawPath(m_OverlayPath);
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

QPointF HighQualityImageItem::mapToOriginal(const QPointF &pt) const
{
    bool invertible;
    QTransform inverse = m_transform.inverted(&invertible);
    if (invertible) {
        return inverse.map(pt);
    } else {
        return QPointF(-1, -1); // eller std::optional/QVariant/etc
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->MainView->setScene(&Scene);
    connect(ui->LoadAfterButton,&QToolButton::clicked,this,&MainWindow::loadAfter);
    connect(ui->SaveAfterButton,&QToolButton::clicked,this,&MainWindow::saveAfterDialog);
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
    for (int i = 0; i < maxAnchors; ++i) {
        QPushButton* b = findChild<QPushButton*>(QString("AnchorBefore%1Button").arg(i + 1));
        QPushButton* a = findChild<QPushButton*>(QString("AnchorAfter%1Button").arg(i + 1));
        anchors.before(i).button = b;
        anchors.after(i).button = a;
        connect(b, &QPushButton::clicked, this, [this, i]() { setAnchorBefore(i); });
        connect(a, &QPushButton::clicked, this, [this, i]() { setAnchorAfter(i); });
        if (i >= anchorCount) {
            b->setVisible(false);
            a->setVisible(false);
        }
    }
    for (int i = 0; i < maxAnchors; i++) {
        QPushButton* b = findChild<QPushButton*>(QString("Compute%1AnchorsButton").arg(i + 1));
        anchors.computeButtons[i] = b;
        connect(b, &QPushButton::clicked, this, [this, i]() { computeAnchors(i); });
        if (i >= anchorCount) b->setVisible(false);
    }
    connect(ui->ClearButton,&QPushButton::clicked,this,&MainWindow::clearAnchors);
    connect(ui->CreateWebSiteButton,&QPushButton::clicked,this,&MainWindow::createWebGallery);
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
            setValue(QString("AnchorBefore%1").arg(i + 1), anchors.before(i));
            setValue(QString("AnchorAfter%1").arg(i + 1), anchors.after(i));
        }
    }
}

void MainWindow::updateFrame()
{
    updateValues();
    for (QGraphicsItem* i : (const QList<QGraphicsItem*>)Scene.items()) Scene.removeItem(i);
    ui->MainView->origSize = beforeImage.originalSize();
    QGraphicsScene* s = &Scene;
    afterImage.setOverlay(anchors.after().path(),anchors.after().pen());
    drawAfter(s,afterImage);
    beforeImage.setOverlay(anchors.before().path(),anchors.before().pen());
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
    i.setTransformMatrix(t);
    s->setSceneRect(beforeImage.originalRect().united(i.mapRectToScene(i.boundingRect()).toRect()));
}

void MainWindow::loadBefore()
{
    QString p = QFileDialog::getOpenFileName(this, tr("Open Image"), "", tr("Image Files (*.jpg *.jpeg)"));
    if (!p.isEmpty())
    {
        beforeImage.load(p);
        setValue("BeforePix",p);
    }
}

void MainWindow::loadAfter()
{
    QString p = QFileDialog::getOpenFileName(this, tr("Open Image"), "", tr("Image Files (*.jpg *.jpeg)"));
    if (!p.isEmpty())
    {
        afterImage.load(p);
        setValue("AfterPix",p);
    }
    updateFrame();
}

void MainWindow::saveAfterDialog() {
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Image"), "", tr("Image Files (*.jpg *.jpeg *.png)"));
    if (!path.isEmpty()) saveAfter(path);
}

void MainWindow::saveAfter(QString path)
{
    QImage outImage(beforeImage.originalSize(), QImage::Format_RGB32);
    outImage.fill(Qt::white);
    QPainter painter(&outImage);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    QGraphicsScene s(beforeImage.originalRect());
    afterImage.setOverlay(QPainterPath());
    drawAfter(&s, afterImage);
    s.render(&painter, outImage.rect(), outImage.rect());
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
    beforeImage.load(valueString("BeforePix"));
    afterImage.load(valueString("AfterPix"));

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
        anchors.before(i).setPoint(valuePointF(QString("AnchorBefore%1").arg(i + 1)));
        anchors.after(i).setPoint(valuePointF(QString("AnchorAfter%1").arg(i + 1)));
    }
    anchors.setButtonColor();
    anchors.enableComputeButtons();

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
        beforeImage.load(p);
        setValue("BeforePix",p);
        loadProject();
        loadAfter();
    }
}

void MainWindow::removeCurrentProject()
{
    removeProject(ui->ProjectCombo->currentText());
}

void MainWindow::computeMax() {
    for (int i = maxAnchors; i > 0; --i) {
        if (anchors.computeEnabled(i)) {
            computeAnchors(i - 1);
            break;
        }
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

void MainWindow::generateFolders(const QString& baseDirPath, const QStringList& projectNames){
    const QString currentProject = ui->ProjectCombo->currentText();
    QDir baseDir(baseDirPath);
    QStringList caseDirs;
    if (baseDir.exists()) {
        QMessageBox msgBox;
        msgBox.setText("Directory Exists!");
        msgBox.setInformativeText("Do you want to use this Directory?");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Yes);
        int ret = msgBox.exec();
        if (ret == QMessageBox::Cancel) return;
    }
    else {
        baseDir.mkpath(baseDirPath);
        baseDir.setPath(baseDirPath);
    }
    for (const QString& pName : projectNames) {
        loadProject(pName);
        baseDir.mkdir(pName);
        beforeImage.save(baseDirPath + "/" + pName + "/before.jpg");
        saveAfter(baseDirPath + "/" + pName + "/after.jpg");
    }
    if (!currentProject.isEmpty()) loadProject(currentProject);
}

void MainWindow::removeProject(QString name) {
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
    Anchor& a = anchors.before(index);
    ui->MainView->loopForPoint(a);
    anchors.enableComputeButtons();
    computeMax();
    updateFrame();
}

void MainWindow::setAnchorAfter(int index) {
    Anchor& a = anchors.after(index);
    ui->MainView->loopForPoint(a);
    if (a.isSet()) a.setPoint(afterImage.mapToOriginal(a));
    anchors.enableComputeButtons();
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
    anchors.clear();
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

void MainWindow::computeAnchors(int index) {
    QTransform t;
    switch (index) {
    case 0:
        t = computePointTransform(anchors.after(), anchors.before());
        break;
    case 1:
        t = computeAlignTransform(anchors.after(), anchors.before());
        break;
    case 2:
        t = computeAffineFromThreePoints(anchors.after(), anchors.before());
        break;
    case 3:
        t = computeAffineFromFourPoints(anchors.after(), anchors.before());
        break;
    }
    saveTransform(t);
    updateFrame();
}

void MainWindow::createWebGallery() {
    QStringList projectNames;
    QStringList allProjects;
    QString title = "Before/After Gallery";
    foreach(QMap p, m_ProjectList) {
        allProjects.append(p.value("ProjectName").toString());
    }
    CProjectDialog p(this);
    p.exec(allProjects,projectNames,title);
    if (projectNames.isEmpty()) return;
    qDebug() << projectNames;
    const QString path = QFileDialog::getExistingDirectory(this, tr("Base Path"), "/home", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (path.isEmpty()) return;
    qDebug() << path;
    generateFolders(path,projectNames);
    generateHtmlGallery(path,title);
}

void MainWindow::generateHtmlGallery(const QString& baseDirPath, const QString& title)
{
    QDir baseDir(baseDirPath);
    QStringList caseDirs;

    // Hitta undermappar med before.jpg + after.jpg
    for (const QString& entry : (const QStringList)baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QDir subdir(baseDir.filePath(entry));
        if (subdir.exists("before.jpg") && subdir.exists("after.jpg")) {
            caseDirs << entry;
        }
    }

    // Skriv HTML-filen
    QFile htmlFile(baseDir.filePath("index.html"));
    if (!htmlFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning("Kunde inte skapa index.html");
        return;
    }

    QTextStream out(&htmlFile);
    out << R"(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <title>)";
    out << title;
out << R"(</title>
  <style>
    body { margin: 0; font-family: sans-serif; background: #111; color: white; }
    .pair-container { margin: 2em auto; width: 90vw; max-width: 1200px; }
    .label { margin-bottom: 0.5em; font-size: 1.2em; }

    .img-container {
      position: relative;
      width: 100%;
      overflow: hidden;
      background: #222;
    }
    .img-container::before {
      content: "";
      display: block;
      padding-top: 56.25%; /* 16:9 ratio fallback */
    }
    .img-before, .img-after {
      position: absolute; top: 0; left: 0;
      width: 100%; height: 100%; object-fit: contain;
    }
    .img-before {
      position: absolute;
      top: 0; left: 0;
      width: 100%; height: 100%;
      object-fit: contain;
      pointer-events: none;
      z-index: 2;
      transition: clip-path 0.2s, opacity 0.2s;
    }
    .img-after {
      z-index: 1;
    }
    select {
      /* ... */
      background-color: #000;
      color: #fff;
    }

    select::before {
      /* ... */
      border-bottom: var(--size) solid #fff;
    }

    select::after {
      /* ... */
      border-top: var(--size) solid #fff;
    }

    input[type=range] {
      width: 100%;
      margin-top: 0.5em;
    }
  </style>
</head>
<body>
<h1 style="text-align: center;">)";
    out << title;
    out << R"(</h1>
<div id="gallery">
)";

    for (int i = 0; i < caseDirs.size(); ++i) {
        const QString& folder = caseDirs[i];
        out << QString(R"(
  <div class="pair-container">
    <div class="label">%1</div>
    <div class="img-container">
      <img src="%1/before.jpg" class="img-before" id="before-%2">
      <img src="%1/after.jpg" class="img-after">
    </div>
    <input type="range" min="0" max="100" value="50" id="slider-%2">
    <select id="mode-%2">
      <option value="vertical">Vertical Split</option>
      <option value="horizontal">Horizontal Split</option>
      <option value="diagonal">Diagonal Split</option>
      <option value="transparent">Transparency</option>
    </select>
  </div>
)").arg(folder).arg(i);
    }

    out << R"(</div>
<script>
)";

    // JS för sliders
    out << "const count = " << caseDirs.size() << ";\n";
    out << R"(
for (let i = 0; i < count; i++) {
  const slider = document.getElementById(`slider-${i}`);
  const before = document.getElementById(`before-${i}`);
  const after = document.getElementById(`after-${i}`);
  const mode = document.getElementById(`mode-${i}`);

  function updateView() {
    const val = slider.value;
    const m = mode.value;

    // Reset style
    before.style.mixBlendMode = '';
    before.style.opacity = '';
    before.style.clipPath = '';
    before.style.transform = '';

    if (m === 'vertical') {
      before.style.clipPath = `inset(0 ${100 - val}% 0 0)`;
    } else if (m === 'horizontal') {
      before.style.clipPath = `inset(0 0 ${100 - val}% 0)`;
    } else if (m === 'diagonal') {
      const pct = val / 50;
      const x = 100 - pct * 100;
      const y = pct * 100;
      before.style.clipPath = `polygon(0 0, ${x}% 0, 100% ${y}%, 100% 100%, 0 100%)`;
    } else if (m === 'transparent') {
      before.style.mixBlendMode = 'normal'; // or 'multiply', 'overlay', etc.
      before.style.opacity = (val / 100).toString();
    }
  }

  slider.addEventListener('input', updateView);
  mode.addEventListener('change', updateView);

  updateView(); // Init
}
</script>
</body>
</html>
)";
    htmlFile.close();
    qDebug() << "HTML-sida genererad till" << htmlFile.fileName();
}


