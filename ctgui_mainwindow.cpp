#include <QDebug>
#include <QFileDialog>
#include <QVector>
#include <QProcess>
#include <QtCore>
#include <QSettings>
#include <QFile>
#include <QTime>
#include <unistd.h>

#include "additional_classes.h"
#include "ctgui_mainwindow.h"
#include "ui_ctgui_mainwindow.h"




static const QString warnStyle = "background-color: rgba(255, 0, 0, 128);";
#define innearList dynamic_cast<PositionList*> ( ui->innearListPlace->layout()->itemAt(0)->widget() )
#define outerList dynamic_cast<PositionList*> ( ui->outerListPlace->layout()->itemAt(0)->widget() )
#define currentScan (currentScan1D * totalScans2D + currentScan2D)



const QString MainWindow::storedState = QDir::homePath()+"/.ctgui";

MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  isLoadingState(false),
  ui(new Ui::MainWindow),
  shutter(new Shutter(this)),
  det(new Detector(this)),
  tct(new TriggCT(this)),
  thetaMotor(new QCaMotorGUI),
  bgMotor(new QCaMotorGUI),
  loopMotor(new QCaMotorGUI),
  subLoopMotor(new QCaMotorGUI),
  dynoMotor(new QCaMotorGUI),
  dyno2Motor(new QCaMotorGUI),
  inAcquisitionTest(false),
  inDynoTest(false),
  inMultiTest(false),
  inFFTest(false),
  inCT(false),
  readyToStartCT(false),
  stopMe(true),
  totalScans1D(1),
  totalScans2D(1),
  currentScan1D(-1),
  currentScan2D(-1),
  totalProjections(-1),
  currentProjection(-1),
  totalLoops(-1),
  currentLoop(-1),
  totalSubLoops(-1),
  currentSubLoop(-1),
  totalShots(-1),
  currentShot(-1)
{

  ui->setupUi(this);
  ui->control->finilize();
  ui->control->setCurrentIndex(0);

  prsSelection << ui->aqsSpeed << ui->stepTime << ui->flyRatio;

  ColumnResizer * resizer;
  resizer = new ColumnResizer(this);
  ui->preRunScript->addToColumnResizer(resizer);
  ui->postRunScript->addToColumnResizer(resizer);
  resizer = new ColumnResizer(this);
  ui->preScanScript->addToColumnResizer(resizer);
  ui->postScanScript->addToColumnResizer(resizer);
  resizer = new ColumnResizer(this);
  ui->preLoopScript->addToColumnResizer(resizer);
  ui->postLoopScript->addToColumnResizer(resizer);
  ui->preSubLoopScript->addToColumnResizer(resizer);
  ui->postSubLoopScript->addToColumnResizer(resizer);
  resizer = new ColumnResizer(this);
  ui->preAqScript->addToColumnResizer(resizer);
  ui->postAqScript->addToColumnResizer(resizer);

  QPushButton * save = new QPushButton("Save");
  save->setFlat(true);
  connect(save, SIGNAL(clicked()), SLOT(saveConfiguration()));
  ui->statusBar->addPermanentWidget(save);

  QPushButton * load = new QPushButton("Load");
  load->setFlat(true);
  connect(load, SIGNAL(clicked()), SLOT(loadConfiguration()));
  ui->statusBar->addPermanentWidget(load);

  for (int caqmod=0 ; caqmod < AQMODEEND ; caqmod++)
    ui->aqMode->insertItem(caqmod, AqModeString( AqMode(caqmod) ));

  ui->startStop->setText("Start CT");
  ui->scanProgress->hide();
  ui->dynoProgress->hide();
  ui->serialProgress->hide();
  ui->multiProgress->hide();
  ui->stepTime->setSuffix("s");
  ui->exposureInfo->setSuffix("s");
  ui->detExposure->setSuffix("s");
  ui->detPeriod->setSuffix("s");
  foreach (Detector::Camera cam , Detector::knownCameras)
    ui->detSelection->addItem(Detector::cameraName(cam));

  innearList->putMotor(new QCaMotorGUI);
  outerList->putMotor(new QCaMotorGUI);
  ui->placeThetaMotor->layout()->addWidget(thetaMotor->setupButton());
  ui->placeScanCurrent->layout()->addWidget(thetaMotor->currentPosition(true));
  ui->placeBGmotor->layout()->addWidget(bgMotor->setupButton());
  ui->placeBGcurrent->layout()->addWidget(bgMotor->currentPosition(true));
  ui->placeLoopMotor->layout()->addWidget(loopMotor->setupButton());
  ui->placeLoopCurrent->layout()->addWidget(loopMotor->currentPosition(true));
  ui->placeSubLoopMotor->layout()->addWidget(subLoopMotor->setupButton());
  ui->placeSubLoopCurrent->layout()->addWidget(subLoopMotor->currentPosition(true));
  ui->placeDynoMotor->layout()->addWidget(dynoMotor->setupButton());
  ui->placeDynoCurrent->layout()->addWidget(dynoMotor->currentPosition(true));
  ui->placeDyno2Motor->layout()->addWidget(dyno2Motor->setupButton());
  ui->placeDyno2Current->layout()->addWidget(dyno2Motor->currentPosition(true));

  ui->placeShutterSelection->layout()->addWidget(shutter->ui->selection);
  ui->placeShutterStatus->layout()->addWidget(shutter->ui->status);
  ui->placeShutterToggle->layout()->addWidget(shutter->ui->toggle);

  connect(ui->browseExpPath, SIGNAL(clicked()), SLOT(onWorkingDirBrowse()));
  connect(ui->checkSerial, SIGNAL(toggled(bool)), SLOT(onSerialCheck()));
  connect(ui->checkFF, SIGNAL(toggled(bool)), SLOT(onFFcheck()));
  connect(ui->checkDyno, SIGNAL(toggled(bool)), SLOT(onDynoCheck()));
  connect(ui->checkMulti, SIGNAL(toggled(bool)), SLOT(onMultiCheck()));
  connect(ui->testFF, SIGNAL(clicked()), SLOT(onFFtest()));
  connect(ui->testMulti, SIGNAL(clicked()), SLOT(onLoopTest()));
  connect(ui->swapSerialLists, SIGNAL(clicked(bool)), SLOT(onSwapSerial())); // important to come before updateUi_serials();
  connect(ui->subLoop, SIGNAL(toggled(bool)), SLOT(onSubLoop()));
  connect(ui->testDyno, SIGNAL(clicked()), SLOT(onDynoTest()));
  connect(ui->dynoDirectionLock, SIGNAL(toggled(bool)), SLOT(onDynoDirectionLock()));
  connect(ui->dynoSpeedLock, SIGNAL(toggled(bool)), SLOT(onDynoSpeedLock()));
  connect(ui->dyno2, SIGNAL(toggled(bool)), SLOT(onDyno2()));
  connect(ui->testDetector, SIGNAL(clicked()), SLOT(onDetectorTest()));
  connect(ui->startStop, SIGNAL(clicked()), SLOT(onStartStop()));

  // updateUi's
  {

  updateUi_expPath();
  updateUi_pathSync();
  updateUi_serials();
  updateUi_ffOnEachScan();
  updateUi_scanRange();
  updateUi_aqsPP();
  updateUi_scanStep();
  updateUi_expOverStep();
  updateUi_thetaMotor();
  updateUi_bgTravel();
  updateUi_bgInterval();
  updateUi_dfInterval();
  updateUi_bgMotor();
  updateUi_loopStep();
  updateUi_loopMotor();
  updateUi_subLoopStep();
  updateUi_subLoopMotor();
  updateUi_dynoSpeed();
  updateUi_dynoMotor();
  updateUi_dyno2Speed();
  updateUi_dyno2Motor();
  updateUi_detector();
  updateUi_aqMode();

  }

  onSerialCheck();
  onFFcheck();
  onDynoCheck();
  onMultiCheck();
  onDyno2();
  onSubLoop();
  onDetectorSelection();


  // populate configNames
  {

  configNames[ui->expDesc] = "description";
  configNames[ui->expPath] = "workingdir";
  configNames[ui->detPathSync] = "syncdetectordir";
  configNames[ui->aqMode] = "acquisitionmode";
  configNames[ui->checkSerial] = "doserialscans";
  configNames[ui->checkFF] = "doflatfield";
  configNames[ui->checkDyno] = "dodyno";
  configNames[ui->checkMulti] = "domulti";
  configNames[ui->checkExtTrig] = "useExtTrig";
  configNames[ui->imageFormatGroup] = "imageFormat";
  configNames[ui->preRunScript] = "prerun";
  configNames[ui->postRunScript] = "postrun";

  configNames[ui->endConditionButtonGroup] = "serial/endCondition";
  configNames[ui->acquisitionTime] = "serial/time";
  configNames[ui->conditionScript] = "serial/stopscript";
  configNames[ui->ongoingSeries] = "serial/ongoingseries";
  configNames[ui->ffOnEachScan] = "serial/ffoneachscan";
  configNames[ui->scanDelay] = "serial/scandelay";
  configNames[innearList] = "serial/innearseries"; // redefined in onSwapSerial
  configNames[outerList] = "serial/outerseries"; // redefined in onSwapSerial
  configNames[ui->serial2D] = "serial/2d";
  configNames[ui->pre2DScript] = "serial/prescript";
  configNames[ui->post2DScript] = "serial/postscript";

  configNames[thetaMotor->motor()] = "scan/motor";
  configNames[ui->scanRange] = "scan/range";
  configNames[ui->scanProjections] = "scan/steps";
  configNames[ui->aqsPP] = "scan/aquisitionsperprojection";
  configNames[ui->scanAdd] = "scan/add";
  configNames[ui->preScanScript] = "scan/prescan";
  configNames[ui->postScanScript] = "scan/postscan";
  configNames[ui->flyRatio] = "scan/flyratio";
  configNames[ui->aqsSpeed] = "scan/aqspeed";
  configNames[ui->stepTime] = "scan/steptime";

  configNames[ui->nofBGs] = "flatfield/bgs";
  configNames[ui->bgInterval] = "flatfield/bginterval";
  configNames[ui->bgIntervalBefore] = "flatfield/bgBefore";
  configNames[ui->bgIntervalAfter] = "flatfield/bgAfter";
  configNames[bgMotor->motor()] = "flatfield/motor";
  configNames[ui->bgTravel] = "flatfield/bgtravel";
  configNames[ui->bgExposure] = "flatfield/bgexposure";
  configNames[ui->nofDFs] = "flatfield/dfs";
  configNames[ui->dfInterval] = "flatfield/dfinterval";
  configNames[ui->dfIntervalBefore] = "flatfield/dfBefore";
  configNames[ui->dfIntervalAfter] = "flatfield/dfAfter";

  configNames[ui->singleBg] = "loop/singlebg";
  configNames[loopMotor->motor()] = "loop/motor";
  configNames[ui->loopNumber] = "loop/shots";
  configNames[ui->loopStep] = "loop/step";
  configNames[ui->preLoopScript] = "loop/preloop";
  configNames[ui->postLoopScript] = "loop/postloop";
  configNames[ui->subLoop] = "loop/subloop";

  configNames[subLoopMotor->motor()] = "loop/subloop/motor";
  configNames[ui->subLoopNumber] = "loop/subloop/shots";
  configNames[ui->subLoopStep] = "loop/subloop/step";
  configNames[ui->preSubLoopScript] = "loop/subloop/presubloop";
  configNames[ui->postSubLoopScript] = "loop/subloop/postsubloop";

  configNames[dynoMotor->motor()] = "dyno/motor";
  configNames[ui->dynoSpeed] = "dyno/speed";
  configNames[ui->dynoDirButtonGroup] = "dyno/direction";

  configNames[ui->dyno2] = "dyno/dyno2";
  configNames[dyno2Motor->motor()] = "dyno/dyno2/motor";
  configNames[ui->dynoSpeedLock] = "dyno/dyno2/speedLock";
  configNames[ui->dyno2Speed] = "dyno/dyno2/speed";
  configNames[ui->dyno2DirButtonGroup] = "dyno/dyno2/direction";
  configNames[ui->dynoDirectionLock] = "dyno/dyno2/dirLock";

  configNames[shutter] = "hardware/shutterBox";
  configNames[ui->detSelection] = "hardware/detectorBox";
  configNames[ui->preAqScript] = "hardware/preacquire";
  configNames[ui->postAqScript] = "hardware/postacquire";

  }

  loadConfiguration(storedState);

  foreach (QObject * obj, configNames.keys()) {
    const char * sig = 0;
    if ( dynamic_cast<QLineEdit*>(obj) ||
         dynamic_cast<QPlainTextEdit*>(obj) ||
         dynamic_cast<UScript*>(obj) ||
         dynamic_cast<QSpinBox*>(obj) ||
         dynamic_cast<QAbstractSpinBox*>(obj) )
      sig = SIGNAL(editingFinished());
    else if (dynamic_cast<QComboBox*>(obj))
      sig = SIGNAL(currentIndexChanged(int));
    else if (dynamic_cast<QAbstractButton*>(obj))
      sig = SIGNAL(toggled(bool));
    else if ( dynamic_cast<QButtonGroup*>(obj))
      sig = SIGNAL(buttonClicked(int));
    else if (dynamic_cast<QCaMotor*>(obj))
      sig = SIGNAL(changedPv());
    else if (dynamic_cast<QTableWidget*>(obj))
      sig = SIGNAL(itemChanged(QTableWidgetItem*));
    else if (dynamic_cast<PositionList*>(obj))
      sig = SIGNAL(parameterChanged());
    else if (dynamic_cast<Shutter*>(obj))
      sig = SIGNAL(shutterChanged());
    else
      qDebug() << "Do not know how to connect object " << obj
               << "to the" << SLOT(storeCurrentState());

    if (sig)
      connect (obj, sig, SLOT(storeCurrentState()));

  }

  updateProgress();


}


MainWindow::~MainWindow() {
  delete ui;
}



static void save_cfg(const QObject * obj, const QString & key, QSettings & config ) {

  if ( dynamic_cast<const QLineEdit*>(obj))
    config.setValue(key, dynamic_cast<const QLineEdit*>(obj)->text());
  else if (dynamic_cast<const QPlainTextEdit*>(obj))
    config.setValue(key, dynamic_cast<const QPlainTextEdit*>(obj)->toPlainText());
  else if (dynamic_cast<const QAbstractButton*>(obj))
    config.setValue(key, dynamic_cast<const QAbstractButton*>(obj)->isChecked());
  else if (dynamic_cast<const UScript*>(obj))
    config.setValue(key, dynamic_cast<const UScript*>(obj)->script->path());
  else if (dynamic_cast<const QComboBox*>(obj))
    config.setValue(key, dynamic_cast<const QComboBox*>(obj)->currentText());
  else if (dynamic_cast<const QSpinBox*>(obj))
    config.setValue(key, dynamic_cast<const QSpinBox*>(obj)->value());
  else if (dynamic_cast<const QDoubleSpinBox*>(obj))
    config.setValue(key, dynamic_cast<const QDoubleSpinBox*>(obj)->value());
  else if (dynamic_cast<const QTimeEdit*>(obj))
    config.setValue(key, dynamic_cast<const QTimeEdit*>(obj)->time());
  else if (dynamic_cast<const QCaMotor*>(obj)) {
    config.setValue(key, dynamic_cast<const QCaMotor*>(obj)->getPv());
    config.setValue(key+"Pos", dynamic_cast<const QCaMotor*>(obj)->getUserPosition());
  } else if ( dynamic_cast<const QLabel*>(obj))
    config.setValue(key, dynamic_cast<const QLabel*>(obj)->text());
  else if ( dynamic_cast<const QButtonGroup*>(obj))
    config.setValue(key,
                    dynamic_cast<const QButtonGroup*>(obj)->checkedButton() ?
                      dynamic_cast<const QButtonGroup*>(obj)->checkedButton()->text() : QString() );
  else if (dynamic_cast<const QTableWidget*>(obj)) {
    config.beginWriteArray(key);
    int index = 0;
    foreach (QTableWidgetItem * item,
             dynamic_cast<const QTableWidget*>(obj)->findItems("", Qt::MatchContains) ) {
      config.setArrayIndex(index++);
      config.setValue("position", item ? item->text() : "");
    }
    config.endArray();
  } else if (dynamic_cast<const PositionList*>(obj)) {
    const PositionList *pl = dynamic_cast<const PositionList*>(obj);
    save_cfg(pl->ui->label, key, config);
    save_cfg(pl->motui->motor(), key + "/motor", config);
    save_cfg(pl->ui->nof, key + "/nofsteps", config);
    save_cfg(pl->ui->step, key + "/step", config);
    save_cfg(pl->ui->irregular, key + "/irregular", config);
    save_cfg(pl->ui->list, key + "/positions", config);
  } else if (dynamic_cast<const Shutter*>(obj)) {
    const Shutter * shut = dynamic_cast<const Shutter*>(obj);
    save_cfg(shut->ui->selection, key, config);
    config.setValue(key+"_custom", shut->readCustomDialog());
  } else
    qDebug() << "Cannot save the value of object" << obj << "into config";

}



void MainWindow::saveConfiguration(QString fileName) {

  if ( fileName.isEmpty() )
    fileName = QFileDialog::getSaveFileName(0, "Save configuration", QDir::currentPath());

  QSettings config(fileName, QSettings::IniFormat);

  foreach (QObject * obj, configNames.keys())
    save_cfg(obj, configNames[obj], config);

  config.setValue("scan/fixprs", selectPRS()->objectName());

  config.beginGroup("hardware");
  if (det) {
    config.setValue("detectorpv", det->pv());
    config.setValue("detectorexposure", det->exposure());
    config.setValue("detectorsavepath", det->path(uiImageFormat()));
  }
  config.endGroup();

  setenv("SERIALMOTORIPV", innearList->motui->motor()->getPv().toAscii() , 1);
  setenv("SERIALMOTOROPV", outerList->motui->motor()->getPv().toAscii() , 1);

}

static bool load_cfg(QObject * obj, const QString & key, QSettings & config ) {

  QVariant value;

  if ( config.contains(key) )
    value=config.value(key);
  else if ( config.beginReadArray(key) ) { // an array
    value=0;
    config.endArray();
  } else {
    qDebug() << "Config does not contain key or array" << key;
    return false;
  }

  if ( ! value.isValid() ) {
    qDebug() << "Value read from config key" << key << "is not valid";
    return false;
  } else if ( dynamic_cast<QLineEdit*>(obj) && value.canConvert(QVariant::String) )
    dynamic_cast<QLineEdit*>(obj)->setText(value.toString());
  else if ( dynamic_cast<QPlainTextEdit*>(obj) && value.canConvert(QVariant::String) )
    dynamic_cast<QPlainTextEdit*>(obj)->setPlainText(value.toString());
  else if ( dynamic_cast<QAbstractButton*>(obj) && value.canConvert(QVariant::Bool) )
    dynamic_cast<QAbstractButton*>(obj)->setChecked(value.toBool());
  else if ( dynamic_cast<UScript*>(obj) && value.canConvert(QVariant::String) )
    dynamic_cast<UScript*>(obj)->script->setPath(value.toString());
  else if ( dynamic_cast<QComboBox*>(obj) && value.canConvert(QVariant::String) ) {
    const QString val = value.toString();
    QComboBox * box = dynamic_cast<QComboBox*>(obj);
    const int idx = box->findText(val);
    if ( idx >= 0 )  box->setCurrentIndex(idx);
    else if ( box->isEditable() ) box->setEditText(val);
  }
  else if ( dynamic_cast<QSpinBox*>(obj) && value.canConvert(QVariant::Int) )
    dynamic_cast<QSpinBox*>(obj)->setValue(value.toInt());
  else if ( dynamic_cast<QDoubleSpinBox*>(obj) && value.canConvert(QVariant::Double) )
    dynamic_cast<QDoubleSpinBox*>(obj)->setValue(value.toDouble());
  else if ( dynamic_cast<QTimeEdit*>(obj) && value.canConvert(QVariant::Time) )
    dynamic_cast<QTimeEdit*>(obj)->setTime(value.toTime());
  else if ( dynamic_cast<QCaMotor*>(obj) && value.canConvert(QVariant::String) )
    dynamic_cast<QCaMotor*>(obj)->setPv(value.toString());
  else if ( dynamic_cast<QButtonGroup*>(obj) && value.canConvert(QVariant::String) ) {
    foreach (QAbstractButton * but, dynamic_cast<QButtonGroup*>(obj)->buttons())
      if (but->text() == value.toString())
        but->setChecked(true);
  } else if (dynamic_cast<QTableWidget*>(obj)) {
    QTableWidget * twdg = dynamic_cast<QTableWidget*>(obj);
    int stepssize = config.beginReadArray(key);
    for (int i = 0; i < stepssize; ++i) {
      config.setArrayIndex(i);
      if ( twdg->item(i, 0) )
        twdg->item(i,0)->setText(config.value("position").toString());
    }
    config.endArray();
  } else if (dynamic_cast<PositionList*>(obj)) {
    PositionList *pl = dynamic_cast<PositionList*>(obj);
    load_cfg(pl->motui->motor(), key + "/motor", config);
    load_cfg(pl->ui->nof, key + "/nofsteps", config);
    load_cfg(pl->ui->step, key + "/step", config);
    load_cfg(pl->ui->irregular, key + "/irregular", config);
    load_cfg(pl->ui->list, key + "/positions", config);
  } else if (dynamic_cast<Shutter*>(obj)) {
    Shutter * shut = dynamic_cast<Shutter*>(obj);
    QVariant customValue = config.value(key+"_custom");
    if (customValue.canConvert(QVariant::StringList))
      shut->loadCustomDialog(customValue.toStringList());
    load_cfg(shut->ui->selection, key, config);
    shut->setShutter();
  } else {
    qDebug() << "Cannot restore the value of widget" << obj << "from value" << value << "in config.";
    return false;
  }

  return true;

}


void MainWindow::loadConfiguration(QString fileName) {

  if ( fileName.isEmpty() )
    fileName = QFileDialog::getOpenFileName(0, "Load configuration", QDir::currentPath());

  isLoadingState=true;
  QSettings config(fileName, QSettings::IniFormat);

  const QString fixprsName = config.value("scan/fixprs").toString();
  foreach (QAbstractSpinBox* prs, prsSelection)
    if ( fixprsName == prs->objectName() )
      selectPRS(prs);
  selectPRS(); // if nothing found

  foreach (QObject * obj, configNames.keys())
    load_cfg(obj, configNames[obj], config);

  if ( ui->expPath->text().isEmpty()  || fileName.isEmpty() )
    ui->expPath->setText( QFileInfo(fileName).absolutePath());

  isLoadingState=false;

}


void MainWindow::storeCurrentState() {
  if (!isLoadingState)
    saveConfiguration(storedState);
}


Detector::ImageFormat MainWindow::uiImageFormat() const {
  if (ui->tiffFormat->isChecked())
    return Detector::TIFF;
  else if (ui->hdfFormat->isChecked())
    return Detector::HDF;
  else
    return Detector::UNDEFINED; // should never happen
}



void MainWindow::addMessage(const QString & str) {
  str.size();
//  ui->messages->append(
//        QDateTime::currentDateTime().toString("dd/MM/yyyy_hh:mm:ss.zzz") +
//        " " + str);
}

static QString lastPathComponent(const QString & pth) {
  QString lastComponent = pth;
  if ( lastComponent.endsWith("\\") ) {
    lastComponent.chop(1);
    return lastComponent.remove(0, lastComponent.lastIndexOf('/')+1);
  }
  if ( lastComponent.endsWith("/") ) // can be win or lin path delimiter
    lastComponent.chop(1);
  lastComponent = QFileInfo(lastComponent).fileName();
  return lastComponent;
}

void MainWindow::updateUi_expPath() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_expPath());
    connect( ui->expPath, SIGNAL(textChanged(QString)), thisSlot );
    connect( ui->detPathSync, SIGNAL(toggled(bool)), thisSlot );
    connect( det, SIGNAL(connectionChanged(bool)), thisSlot);
  }

  const QString pth = ui->expPath->text();

  bool isOK;
  if (QDir::isAbsolutePath(pth)) {
    QFileInfo fi(pth);
    isOK = fi.isDir() && fi.isWritable();
  } else
    isOK = false;

  if (isOK) {

    QDir::setCurrent(pth);

    /* using "*.[tT][iI][fF][fF]" can be very time consuming if many files are present */
    bool sampleFileExists = false;
    QString zlab;
    while ( ! sampleFileExists  &&  (zlab += "0").size() < 10 )
      sampleFileExists  =  QFile::exists("SAMPLE_Z0_T"+zlab+".tif") || QFile::exists("SAMPLE_T"+zlab+".tif");

    ui->nonEmptyWarning->setVisible( sampleFileExists );


    if ( ui->detPathSync->isChecked() && det->isConnected() ) {

      QString lastComponent = lastPathComponent(pth);
      QString detDir = det->path(uiImageFormat());
      if ( detDir.endsWith("/") || detDir.endsWith("\\") ) // can be win or lin path delimiter
        detDir.chop(1);
      const int delidx = detDir.lastIndexOf( QRegExp("[/\\\\]") );
      if ( delidx >=0 )
        detDir.truncate(delidx+1);
      det->setPath(detDir + lastComponent);

    }

  }

  check(ui->expPath, isOK);

}


void MainWindow::updateUi_pathSync() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_pathSync());
    connect( ui->expPath, SIGNAL(textChanged(QString)), thisSlot );
    connect( ui->detPathSync, SIGNAL(toggled(bool)), thisSlot );
    connect( det, SIGNAL(parameterChanged()), thisSlot);
  }
  check( ui->detPathSync,
         ! ui->detPathSync->isChecked()  ||
         ! ui->detSelection->currentIndex() ||
         lastPathComponent(det->path(uiImageFormat())) == lastPathComponent(ui->expPath->text()));
}


void MainWindow::updateUi_aqMode() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_aqMode());
    connect( tct, SIGNAL(connectionChanged(bool)), thisSlot);
    connect( tct, SIGNAL(runningChanged(bool)), thisSlot);
    connect( ui->aqMode, SIGNAL(currentIndexChanged(int)), thisSlot);
  }

  const AqMode aqmd = (AqMode) ui->aqMode->currentIndex();

  if ( sender() == ui->aqMode ) {
    if ( aqmd == FLYHARD2B ) {
      tct->setPrefix("SR08ID01SST24:ROTATION:EQU");
      ui->checkExtTrig->setVisible(false);
    } else if ( aqmd == FLYHARD3BTABL ) {
      tct->setPrefix("SR08ID01SST01:ROTATION:EQU");
      ui->checkExtTrig->setVisible(false);
    } else if ( aqmd == FLYHARD3BLAPS ) {
      tct->setPrefix("SR08ID01ROB01:ROTATION:EQU");
      ui->checkExtTrig->setVisible(false);
    } else {
      tct->setPrefix("");
      ui->checkExtTrig->setVisible(true);
    }
  }

  bool isOK = tct->prefix().isEmpty() ||
      ( tct->isConnected() && ! tct->isRunning() );
  check(ui->aqMode, isOK);

  if ( ! tct->prefix().isEmpty() &&
       tct->isConnected() &&
       ! tct->motor().isEmpty() ) {
    if ( tct->motor() != thetaMotor->motor()->getPv() )
      thetaMotor->motor()->setPv(tct->motor());
    thetaMotor->lock(true);
  } else {
    thetaMotor->lock(false);
  }

  const bool sasmd = aqmd == STEPNSHOT;

  ui->ffOnEachScan->setVisible(!sasmd);
  ui->speedsLine->setVisible(!sasmd);
  ui->speedsLabel->setVisible(!sasmd);
  ui->timesLabel->setVisible(!sasmd);
  ui->normalSpeed->setVisible(!sasmd);
  ui->normalSpeedLabel->setVisible(!sasmd);
  ui->stepTime->setVisible(!sasmd);
  ui->stepTimeLabel->setVisible(!sasmd);
  ui->flyRatio->setVisible(!sasmd);
  ui->rotSpeedLabel->setVisible(!sasmd);
  ui->aqsSpeed->setVisible(!sasmd);
  ui->expOverStepLabel->setVisible(!sasmd);
  ui->maximumSpeed->setVisible(!sasmd);
  ui->maximumSpeedLabel->setVisible(!sasmd);
  ui->exposureInfo->setVisible(!sasmd);
  ui->exposureInfoLabel->setVisible(!sasmd);
  ui->checkDyno->setVisible(sasmd);
  ui->checkMulti->setVisible(sasmd);

  if ( ! sasmd ) {
    ui->checkDyno->setChecked(false);
    ui->checkMulti->setChecked(false);
  }

}


void MainWindow::updateUi_serials() {

  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_serials());
    connect(ui->endConditionButtonGroup, SIGNAL(buttonClicked(int)), thisSlot);
    connect(ui->serial2D, SIGNAL(toggled(bool)), thisSlot);
    connect(innearList, SIGNAL(amOKchanged(bool)), thisSlot);
    connect(outerList, SIGNAL(amOKchanged(bool)), thisSlot);
    connect(ui->swapSerialLists, SIGNAL(clicked(bool)), thisSlot); //make sure connected after onSwapSerials()

    connect(ui->serial2D, SIGNAL(toggled(bool)), ui->innearListPlace, SLOT(setVisible(bool)));
    connect(ui->serial2D, SIGNAL(toggled(bool)), ui->pre2DScript, SLOT(setVisible(bool)));
    connect(ui->serial2D, SIGNAL(toggled(bool)), ui->pre2DSlabel, SLOT(setVisible(bool)));
    connect(ui->serial2D, SIGNAL(toggled(bool)), ui->post2DScript, SLOT(setVisible(bool)));
    connect(ui->serial2D, SIGNAL(toggled(bool)), ui->post2DSlabel, SLOT(setVisible(bool)));
    connect(ui->endNumber, SIGNAL(toggled(bool)), ui->outerListPlace, SLOT(setVisible(bool)));
  }

  ui->swapSerialLists->setVisible( ui->serial2D->isChecked() && ui->endNumber->isChecked() );
  ui->serialTabSpacer->setHidden( ui->serial2D->isChecked() || ui->endNumber->isChecked() );
  innearList->putLabel("innear\nloop\n\n[Z]");
  outerList->putLabel(ui->serial2D->isChecked() ? "outer\nloop\n\n[Y]" : "[Y]");

  ui->acquisitionTimeWdg->setVisible(ui->endTime->isChecked());
  ui->acquisitionTimeLabel->setVisible(ui->endTime->isChecked());

  ui->conditionScript->setVisible(ui->endCondition->isChecked());
  ui->conditionScriptLabel->setVisible(ui->endCondition->isChecked());

  check(ui->serial2D, ! ui->serial2D->isChecked() || innearList->amOK() );
  check(ui->endNumber, ! ui->endNumber->isChecked() || outerList->amOK() );

}

void MainWindow::updateUi_ffOnEachScan() {
    if ( ! sender() ) // called from the constructor;
        connect( ui->ongoingSeries, SIGNAL(toggled(bool)), SLOT(updateUi_ffOnEachScan()));
    ui->ffOnEachScan->setEnabled( ! ui->ongoingSeries->isChecked() );
    if ( ui->ongoingSeries->isChecked() )
        ui->ffOnEachScan->setChecked(false);
}

void MainWindow::onSwapSerial() {
  ui->innearListPlace->layout()->addWidget( outerList );
  ui->outerListPlace->layout()->addWidget( innearList );
  configNames[innearList] = "serial/innearseries";
  configNames[outerList] = "serial/outerseries";
  storeCurrentState();
}












void MainWindow::updateUi_scanRange() {
  QCaMotor * mot = thetaMotor->motor();
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_scanRange());
    connect( ui->scanRange, SIGNAL(valueChanged(double)), thisSlot);
    connect( mot, SIGNAL(changedConnected(bool)), thisSlot);
    connect( mot, SIGNAL(changedUserPosition(double)), thisSlot);
    connect( mot, SIGNAL(changedPrecision(int)), thisSlot);
    connect( mot, SIGNAL(changedUnits(QString)), thisSlot);
    connect( mot, SIGNAL(changedUserLoLimit(double)), thisSlot);
    connect( mot, SIGNAL(changedUserHiLimit(double)), thisSlot);
    connect(ui->checkSerial, SIGNAL(toggled(bool)), thisSlot);
    connect(ui->swapSerialLists, SIGNAL(clicked(bool)), thisSlot); //make sure connected after onSwapSerials()
    connect(ui->ongoingSeries, SIGNAL(toggled(bool)), thisSlot);
    connect(ui->endNumber, SIGNAL(toggled(bool)), thisSlot);
    connect(ui->serial2D, SIGNAL(toggled(bool)), thisSlot);
    connect(innearList, SIGNAL(parameterChanged()), thisSlot);
    connect(outerList, SIGNAL(parameterChanged()), thisSlot);
  }

  if ( mot->isConnected() ) {
    ui->scanRange->setSuffix(mot->getUnits());
    ui->scanRange->setDecimals(mot->getPrecision());
  }

  double endpos = mot->getUserPosition();
  if ( ui->checkSerial->isChecked() &&
       ui->ongoingSeries->isChecked() &&
       ui->innearListPlace->isVisible() )
    endpos +=  ( innearList->ui->nof->value() - 1 ) * ui->scanRange->value();
  else
    endpos +=  ui->scanRange->value();

  check(ui->scanRange,
        ui->scanRange->value() != 0.0 &&
      endpos > mot->getUserLoLimit() &&
      endpos < mot->getUserHiLimit() );

}


void MainWindow::updateUi_aqsPP() {
  if ( ! sender() ) {
    const char* thisSlot = SLOT(updateUi_aqsPP());
    connect( ui->aqMode, SIGNAL(currentIndexChanged(int)), thisSlot);
    connect( ui->checkDyno, SIGNAL(toggled(bool)), thisSlot);
  }

  bool vis = ui->aqMode->currentIndex() == STEPNSHOT &&  ! ui->checkDyno->isChecked();
  ui->aqsPP->setVisible(vis);
  ui->aqsPPLabel->setVisible(vis);

}


void MainWindow::updateUi_scanStep() {
  QCaMotor * mot = thetaMotor->motor();
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_scanStep());
    connect( ui->scanRange, SIGNAL(valueChanged(double)), thisSlot);
    connect( ui->scanProjections, SIGNAL(valueChanged(int)), thisSlot);
    connect( mot, SIGNAL(changedConnected(bool)), thisSlot);
    connect( mot, SIGNAL(changedPrecision(int)), thisSlot);
    connect( mot, SIGNAL(changedUnits(QString)), thisSlot);
  }

  if ( mot->isConnected() ) {
    ui->scanStep->setSuffix(mot->getUnits());
    ui->scanStep->setDecimals(mot->getPrecision());
  }
  ui->scanStep->setValue( ui->scanRange->value() / ui->scanProjections->value() );

}



QAbstractSpinBox * MainWindow::selectPRS(QObject *prso) {

  QAbstractSpinBox * prs = dynamic_cast<QAbstractSpinBox*>(prso);
  if ( ! prs || ! prsSelection.contains(prs) ) {
    prs = prsSelection.at(0);
    foreach (QAbstractSpinBox* _prs, prsSelection)
      if (_prs->hasFrame())
        prs = _prs;
  }

  foreach (QAbstractSpinBox* _prs, prsSelection) {
    _prs->setFrame(_prs==prs);
    _prs->setButtonSymbols(
          _prs == prs ? QAbstractSpinBox::UpDownArrows : QAbstractSpinBox::NoButtons );
  }

  return prs;

}



void MainWindow::updateUi_expOverStep() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_expOverStep());
    connect( ui->aqMode, SIGNAL(currentIndexChanged(int)), thisSlot);
    connect( ui->flyRatio, SIGNAL(valueEdited(double)), thisSlot);
    connect( ui->stepTime, SIGNAL(valueEdited(double)), thisSlot);
    connect( ui->aqsSpeed, SIGNAL(valueEdited(double)), thisSlot);
    connect( ui->scanRange, SIGNAL(valueChanged(double)), thisSlot);
    connect( ui->scanProjections, SIGNAL(valueChanged(int)), thisSlot);
    connect( det, SIGNAL(parameterChanged()), thisSlot);
    connect( thetaMotor->motor(), SIGNAL(changedMaximumSpeed(double)), thisSlot);
  }

  ui->stepTime->setMinimum(det->exposure());
  const float stepSize = ui->scanRange->value() / ui->scanProjections->value();

  QAbstractSpinBox * prs = selectPRS(sender());
  if ( prs == ui->stepTime ) {
    ui->flyRatio->setValue( det->exposure() / ui->stepTime->value() );
    ui->aqsSpeed->setValue( stepSize / ui->stepTime->value() );
  } else if ( prs == ui->flyRatio ) {
    ui->stepTime->setValue( det->exposure() / ui->flyRatio->value() );
    ui->aqsSpeed->setValue( stepSize * ui->flyRatio->value() / det->exposure() );
  } else if (prs == ui->aqsSpeed ) {
    ui->stepTime->setValue( stepSize / ui->aqsSpeed->value() );
    ui->flyRatio->setValue( ui->aqsSpeed->value() * det->exposure() / stepSize );
  } else
    qDebug() << "ERROR! PRS selection is buggy. Report to developper. Must never happen.";

  bool isOK = ui->aqMode->currentIndex() == STEPNSHOT
      ||  ui->aqsSpeed->value() <= thetaMotor->motor()->getMaximumSpeed();
  foreach (QAbstractSpinBox* _prs, prsSelection)
    check( _prs, _prs != prs || isOK );

}

void MainWindow::updateUi_thetaMotor() {
  QCaMotor * mot = thetaMotor->motor();
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_thetaMotor());
    connect( mot, SIGNAL(changedConnected(bool)), thisSlot);
    connect( mot, SIGNAL(changedPrecision(int)), thisSlot);
    connect( mot, SIGNAL(changedUnits(QString)), thisSlot);
    connect( mot, SIGNAL(changedMoving(bool)), thisSlot);
    connect( mot, SIGNAL(changedNormalSpeed(double)), thisSlot);
    connect( mot, SIGNAL(changedMaximumSpeed(double)), thisSlot);
    connect( mot, SIGNAL(changedLoLimitStatus(bool)), thisSlot);
    connect( mot, SIGNAL(changedHiLimitStatus(bool)), thisSlot);
  }

  setenv("ROTATIONMOTORPV", mot->getPv().toAscii() , 1);

  check(thetaMotor->setupButton(),
        mot->isConnected() && ! mot->isMoving() && ! mot->getLimitStatus());

  if (!mot->isConnected())
    return;

  QString units = mot->getUnits();
  units += "/s";
  ui->normalSpeed->setSuffix(units);
  ui->maximumSpeed->setSuffix(units);
  ui->aqsSpeed->setSuffix(units);

  const int prec = mot->getPrecision();
  ui->normalSpeed->setDecimals(prec);
  ui->maximumSpeed->setDecimals(prec);
  ui->aqsSpeed->setDecimals(mot->getPrecision());

  ui->normalSpeed->setValue(mot->getNormalSpeed());
  ui->maximumSpeed->setValue(mot->getMaximumSpeed());

}

void MainWindow::updateUi_bgInterval() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_bgInterval());
    connect( ui->scanProjections, SIGNAL(valueChanged(int)), thisSlot);
    connect( ui->aqMode, SIGNAL(currentIndexChanged(int)), thisSlot);
    connect( ui->nofBGs, SIGNAL(valueChanged(int)), thisSlot);
    connect( ui->bgInterval, SIGNAL(valueChanged(int)), thisSlot);
    connect( ui->bgIntervalAfter, SIGNAL(toggled(bool)), thisSlot);
    connect( ui->bgIntervalBefore, SIGNAL(toggled(bool)), thisSlot);
  }

  const bool sasMode = ui->aqMode->currentIndex() == STEPNSHOT;

  ui->bgIntervalWdg->setEnabled(ui->nofBGs->value());
  if (sasMode)
    ui->bgIntervalWdg->setCurrentWidget(ui->bgIntervalSAS);
  else
    ui->bgIntervalWdg->setCurrentWidget(ui->bgIntervalContinious);

  bool itemOK;

  itemOK =
      ! sasMode ||
      ! ui->nofBGs->value() ||
      ui->bgInterval->value() <= ui->scanProjections->value();
  check(ui->bgInterval, itemOK);

  itemOK =
      sasMode ||
      ! ui->nofBGs->value() ||
      ( ui->bgIntervalAfter->isChecked() || ui->bgIntervalBefore->isChecked() );
  check(ui->bgIntervalAfter, itemOK );
  check(ui->bgIntervalBefore, itemOK );

}

void MainWindow::updateUi_dfInterval() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_dfInterval());
    connect( ui->scanProjections, SIGNAL(valueChanged(int)), thisSlot);
    connect( ui->aqMode, SIGNAL(currentIndexChanged(int)), thisSlot);
    connect( ui->nofDFs, SIGNAL(valueChanged(int)), thisSlot);
    connect( ui->dfInterval, SIGNAL(valueChanged(int)), thisSlot);
    connect( ui->dfIntervalAfter, SIGNAL(toggled(bool)), thisSlot);
    connect( ui->dfIntervalBefore, SIGNAL(toggled(bool)), thisSlot);
  }

  const bool sasMode = ui->aqMode->currentIndex() == STEPNSHOT;

  ui->dfIntervalWdg->setEnabled(ui->nofDFs->value());
  if (sasMode)
    ui->dfIntervalWdg->setCurrentWidget(ui->dfIntervalSAS);
  else
    ui->dfIntervalWdg->setCurrentWidget(ui->dfIntervalContinious);

  bool itemOK;

  itemOK =
      ! sasMode ||
      ! ui->nofDFs->value() ||
      ui->dfInterval->value() <= ui->scanProjections->value();
  check(ui->dfInterval, itemOK);

  itemOK =
      sasMode ||
      ! ui->nofDFs->value() ||
      ( ui->dfIntervalAfter->isChecked() || ui->dfIntervalBefore->isChecked() );
  check(ui->dfIntervalAfter, itemOK );
  check(ui->dfIntervalBefore, itemOK );


}

void MainWindow::updateUi_bgTravel() {
  QCaMotor * mot = bgMotor->motor();
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_bgTravel());
    connect( ui->scanProjections, SIGNAL(valueChanged(int)), thisSlot);
    connect( ui->nofBGs, SIGNAL(valueChanged(int)), thisSlot);
    connect( ui->bgTravel, SIGNAL(valueChanged(double)), thisSlot);
    connect( mot, SIGNAL(changedConnected(bool)), thisSlot);
    connect( mot, SIGNAL(changedUserLoLimit(double)), thisSlot);
    connect( mot, SIGNAL(changedUserHiLimit(double)), thisSlot);
    connect( mot, SIGNAL(changedPrecision(int)), thisSlot);
    connect( mot, SIGNAL(changedUnits(QString)), thisSlot);
  }

  if (mot->isConnected()) {
    ui->bgTravel->setSuffix(mot->getUnits());
    ui->bgTravel->setDecimals(mot->getPrecision());
  }

  const int nofbgs = ui->nofBGs->value();
  ui->bgTravel->setEnabled(nofbgs);
  const double endpos = mot->getUserPosition() + ui->bgTravel->value();
  const bool itemOK =
      ! nofbgs ||
      ( ui->bgTravel->value() != 0.0  &&
      endpos > mot->getUserLoLimit() && endpos < mot->getUserHiLimit() ) ;
  check(ui->bgTravel, itemOK);

}

void MainWindow::updateUi_bgMotor() {
  QCaMotor * mot = bgMotor->motor();
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_bgMotor());
    connect( ui->nofBGs, SIGNAL(valueChanged(int)), thisSlot);
    connect( ui->checkFF, SIGNAL(toggled(bool)), thisSlot);
    connect( mot, SIGNAL(changedConnected(bool)), thisSlot);
    connect( mot, SIGNAL(changedMoving(bool)), thisSlot);
    connect( mot, SIGNAL(changedLoLimitStatus(bool)), thisSlot);
    connect( mot, SIGNAL(changedHiLimitStatus(bool)), thisSlot);
  }

  bgMotor->setupButton()->setEnabled(ui->nofBGs->value());
  check(bgMotor->setupButton(),
        ! ui->checkFF->isChecked() || ! ui->nofBGs->value() ||
        ( mot->isConnected()  &&  ! mot->isMoving()  &&  ! mot->getLimitStatus() ) );

}



void MainWindow::updateUi_loopStep() {
  QCaMotor * mot = loopMotor->motor();
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_loopStep());
    connect( ui->loopStep, SIGNAL(valueChanged(double)), thisSlot);
    connect( ui->loopNumber, SIGNAL(valueChanged(int)), thisSlot);
    connect( ui->checkMulti, SIGNAL(toggled(bool)), thisSlot);
    connect( mot, SIGNAL(changedPrecision(int)), thisSlot);
    connect( mot, SIGNAL(changedUnits(QString)), thisSlot);
    connect( mot, SIGNAL(changedConnected(bool)), thisSlot);
    connect( mot, SIGNAL(changedUserLoLimit(double)), thisSlot);
    connect( mot, SIGNAL(changedUserHiLimit(double)), thisSlot);
  }

  ui->loopStep->setSuffix(mot->getUnits());
  ui->loopStep->setDecimals(mot->getPrecision());
  ui->loopStep->setEnabled(mot->isConnected());

  const double endpos = mot->getUserPosition() +
      ui->loopStep->value() * ( ui->loopNumber->value() - 1);
  const bool itemOK =
      ! ui->checkMulti->isChecked() ||
      ! mot->isConnected() ||
      ( ui->loopStep->value() != 0.0  &&
      endpos > mot->getUserLoLimit() && endpos < mot->getUserHiLimit() );
  check(ui->loopStep,itemOK);

}

void MainWindow::updateUi_loopMotor() {
  QCaMotor * mot = loopMotor->motor();
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_loopMotor());
    connect( ui->checkMulti, SIGNAL(toggled(bool)), thisSlot);
    connect( mot, SIGNAL(changedConnected(bool)), thisSlot);
    connect( mot, SIGNAL(changedPv(QString)), thisSlot);
    connect( mot, SIGNAL(changedMoving(bool)), thisSlot);
    connect( mot, SIGNAL(changedLoLimitStatus(bool)), thisSlot);
    connect( mot, SIGNAL(changedHiLimitStatus(bool)), thisSlot);
  }
  check(loopMotor->setupButton(),
        ! ui->checkMulti->isChecked()  ||  mot->getPv().isEmpty() ||
        ( mot->isConnected() && ! mot->isMoving()  &&  ! mot->getLimitStatus() ) );
}

void MainWindow::updateUi_subLoopStep() {
  QCaMotor * mot = subLoopMotor->motor();
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_subLoopStep());
    connect( ui->subLoopStep, SIGNAL(valueChanged(double)), thisSlot);
    connect( ui->checkMulti, SIGNAL(toggled(bool)), thisSlot);
    connect( ui->subLoop, SIGNAL(toggled(bool)), thisSlot);
    connect( ui->subLoopNumber, SIGNAL(valueChanged(int)), thisSlot);
    connect( mot, SIGNAL(changedPrecision(int)), thisSlot);
    connect( mot, SIGNAL(changedUnits(QString)), thisSlot);
    connect( mot, SIGNAL(changedConnected(bool)), thisSlot);
    connect( mot, SIGNAL(changedUserLoLimit(double)), thisSlot);
    connect( mot, SIGNAL(changedUserHiLimit(double)), thisSlot);
  }

  ui->subLoopStep->setSuffix(mot->getUnits());
  ui->subLoopStep->setDecimals(mot->getPrecision());
  ui->subLoopStep->setEnabled(mot->isConnected());

  const double endpos = mot->getUserPosition() +
      ui->subLoopStep->value() * ( ui->subLoopNumber->value() - 1 );
  const bool itemOK =
      ! ui->checkMulti->isChecked() ||
      ! ui->subLoop->isChecked() ||
      ! mot->isConnected() ||
      ( ui->subLoopStep->value() != 0.0  &&
      endpos > mot->getUserLoLimit() && endpos < mot->getUserHiLimit() );
  check(ui->subLoopStep,itemOK );

}

void MainWindow::updateUi_subLoopMotor() {
  QCaMotor * mot = subLoopMotor->motor();
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_subLoopMotor());
    connect( ui->checkMulti, SIGNAL(toggled(bool)), thisSlot);
    connect( ui->subLoop, SIGNAL(toggled(bool)), thisSlot);
    connect( mot, SIGNAL(changedConnected(bool)), thisSlot);
    connect( mot, SIGNAL(changedPrecision(int)), thisSlot);
    connect( mot, SIGNAL(changedPv(QString)), thisSlot);
    connect( mot, SIGNAL(changedUnits(QString)), thisSlot);
    connect( mot, SIGNAL(changedMoving(bool)), thisSlot);
    connect( mot, SIGNAL(changedLoLimitStatus(bool)), thisSlot);
    connect( mot, SIGNAL(changedHiLimitStatus(bool)), thisSlot);
  }
  check(subLoopMotor->setupButton(),
        ! ui->checkMulti->isChecked() || ! ui->subLoop->isChecked() ||  mot->getPv().isEmpty() ||
        ( mot->isConnected() && ! mot->isMoving()  &&  ! mot->getLimitStatus() ) );
}

void MainWindow::updateUi_dynoSpeed() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_dynoSpeed());
    connect( ui->dynoSpeed , SIGNAL(editingFinished()), thisSlot);
    connect( dynoMotor->motor(), SIGNAL(changedConnected(bool)), thisSlot);
    connect( dynoMotor->motor(), SIGNAL(changedPrecision(int)), thisSlot);
    connect( dynoMotor->motor(), SIGNAL(changedUnits(QString)), thisSlot);
    connect( dynoMotor->motor(), SIGNAL(changedNormalSpeed(double)), thisSlot);
    connect( dynoMotor->motor(), SIGNAL(changedMaximumSpeed(double)), thisSlot);
    connect( ui->dynoSpeedLock, SIGNAL(toggled(bool)), thisSlot);
    connect( dyno2Motor->motor(), SIGNAL(changedMaximumSpeed(double)), thisSlot);
    connect( dyno2Motor->motor(), SIGNAL(changedConnected(bool)), thisSlot);
    connect( ui->dyno2, SIGNAL(toggled(bool)), thisSlot);
  }

  if ( ! dynoMotor->motor()->isConnected() )
    return;

  if ( ui->dynoSpeed->value()==0.0 )
    ui->dynoSpeed->setValue(dynoMotor->motor()->getNormalSpeed());
  ui->dynoSpeed->setSuffix(dynoMotor->motor()->getUnits()+"/s");
  ui->dynoSpeed->setDecimals(dynoMotor->motor()->getPrecision());


  double maximum = dynoMotor->motor()->getMaximumSpeed();
  if (ui->dyno2->isChecked() && ui->dynoSpeedLock->isChecked() &&
      dyno2Motor->motor()->isConnected())
    maximum = qMax(maximum, dynoMotor->motor()->getMaximumSpeed());
  check( ui->dynoSpeed, ui->dyno2Speed->value() <= maximum );

}

void MainWindow::updateUi_dynoMotor() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_dynoMotor());
    connect( ui->checkDyno, SIGNAL(toggled(bool)), thisSlot);
    connect( dynoMotor->motor(), SIGNAL(changedConnected(bool)), thisSlot);
    connect( dynoMotor->motor(), SIGNAL(changedPrecision(int)), thisSlot);
    connect( dynoMotor->motor(), SIGNAL(changedUnits(QString)), thisSlot);
    connect( dynoMotor->motor(), SIGNAL(changedMoving(bool)), thisSlot);
    connect( dynoMotor->motor(), SIGNAL(changedLoLimitStatus(bool)), thisSlot);
    connect( dynoMotor->motor(), SIGNAL(changedHiLimitStatus(bool)), thisSlot);

    connect(dynoMotor->motor(), SIGNAL(changedNormalSpeed(double)),
            ui->dynoNormalSpeed, SLOT(setValue(double)));

  }

  check(dynoMotor->setupButton(),
        ! ui->checkDyno->isChecked() ||
        ( dynoMotor->motor()->isConnected() && ! dynoMotor->motor()->isMoving() && ! dynoMotor->motor()->getLimitStatus()) );

  if ( dynoMotor->motor()->isConnected() ) {
    ui->dynoNormalSpeed->setDecimals(dynoMotor->motor()->getPrecision());
    ui->dynoNormalSpeed->setSuffix(dynoMotor->motor()->getUnits()+"/s");
  }


}

void MainWindow::updateUi_dyno2Speed() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_dyno2Speed());
    connect( ui->dyno2Speed , SIGNAL(editingFinished()), thisSlot);
    connect( dyno2Motor->motor(), SIGNAL(changedConnected(bool)), thisSlot);
    connect( dyno2Motor->motor(), SIGNAL(changedPrecision(int)), thisSlot);
    connect( dyno2Motor->motor(), SIGNAL(changedUnits(QString)), thisSlot);
    connect( dyno2Motor->motor(), SIGNAL(changedNormalSpeed(double)), thisSlot);
    connect( dyno2Motor->motor(), SIGNAL(changedMaximumSpeed(double)), thisSlot);
    connect( ui->dynoSpeedLock, SIGNAL(toggled(bool)), thisSlot);
  }

  if ( ! dyno2Motor->motor()->isConnected() )
    return;

  if ( ui->dyno2Speed->value()==0.0 )
    ui->dyno2Speed->setValue(dyno2Motor->motor()->getNormalSpeed());
  ui->dyno2Speed->setSuffix(dyno2Motor->motor()->getUnits()+"/s");
  ui->dyno2Speed->setDecimals(dyno2Motor->motor()->getPrecision());
  ui->dyno2Speed->setDisabled(ui->dynoSpeedLock->isChecked());

  check(ui->dyno2Speed,
        ui->dyno2Speed->value() <= dyno2Motor->motor()->getMaximumSpeed() );

}

void MainWindow::updateUi_dyno2Motor() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_dyno2Motor());
    connect( ui->checkDyno, SIGNAL(toggled(bool)), thisSlot);
    connect( ui->dyno2, SIGNAL(toggled(bool)), thisSlot);
    connect( dyno2Motor->motor(), SIGNAL(changedConnected(bool)), thisSlot);
    connect( dyno2Motor->motor(), SIGNAL(changedPrecision(int)), thisSlot);
    connect( dyno2Motor->motor(), SIGNAL(changedUnits(QString)), thisSlot);
    connect( dyno2Motor->motor(), SIGNAL(changedMoving(bool)), thisSlot);
    connect( dyno2Motor->motor(), SIGNAL(changedLoLimitStatus(bool)), thisSlot);
    connect( dyno2Motor->motor(), SIGNAL(changedHiLimitStatus(bool)), thisSlot);

    connect(dyno2Motor->motor(), SIGNAL(changedNormalSpeed(double)),
            ui->dyno2NormalSpeed, SLOT(setValue(double)));

  }

  check(dyno2Motor->setupButton(),
        ! ui->checkDyno->isChecked() || ! ui->dyno2->isChecked() ||
        ( dyno2Motor->motor()->isConnected() && ! dyno2Motor->motor()->isMoving() && ! dyno2Motor->motor()->getLimitStatus()) );

  if ( dyno2Motor->motor()->isConnected() ) {
    ui->dyno2NormalSpeed->setDecimals(dyno2Motor->motor()->getPrecision());
    ui->dyno2NormalSpeed->setSuffix(dyno2Motor->motor()->getUnits()+"/s");
  }


}

void MainWindow::updateUi_detector() {
  if ( ! sender() ) {
    const char* thisSlot = SLOT(updateUi_detector());
    connect(ui->imageFormatGroup, SIGNAL(buttonClicked(int)), thisSlot);
    connect(ui->detSelection, SIGNAL(currentIndexChanged(int)), SLOT(onDetectorSelection()));
    connect(ui->detSelection, SIGNAL(currentIndexChanged(int)), thisSlot);
    connect(det, SIGNAL(connectionChanged(bool)), thisSlot);
    connect(det, SIGNAL(parameterChanged()), thisSlot);
    connect(det, SIGNAL(counterChanged(int)), ui->detProgress, SLOT(setValue(int)));
    connect(det, SIGNAL(counterChanged(int)), ui->detImageCounter, SLOT(setValue(int)));
    connect(det, SIGNAL(lastNameChanged(QString)), ui->detFileLastName, SLOT(setText(QString)));
  }

  totalShots = det->number();
  currentShot  =  det->isAcquiring() ? det->counter() : -1;

  if ( ! ui->detSelection->currentIndex() )
    ui->detStatus->setText("always ready");
  else if ( ! det->isConnected() )
    ui->detStatus->setText("no link");
  else if ( det->isAcquiring() )
    ui->detStatus->setText("acquiring");
  else if ( det->isWriting() )
    ui->detStatus->setText("writing");
  else
    ui->detStatus->setText("idle");

  if (  det->isConnected() ) {

    bool enabme = det->imageFormat(Detector::TIFF);
    ui->tiffEnabled->setText( enabme ? "enabled" : "disabled" );
    ui->detFileNameTiff->setEnabled(enabme);
    ui->detFileTemplateTiff->setEnabled(enabme);
    ui->detPathTiff->setEnabled(enabme);

    enabme = det->imageFormat(Detector::HDF);
    ui->hdfEnabled->setText( enabme ? "enabled" : "disabled" );
    ui->detFileNameHdf->setEnabled(enabme);
    ui->detFileTemplateHdf->setEnabled(enabme);
    ui->detPathHdf->setEnabled(enabme);

    ui->detProgress->setMaximum(det->number());
    ui->detTotalImages->setValue(det->number());
    ui->exposureInfo->setValue(det->exposure());
    ui->detExposure->setValue(det->exposure());
    ui->detPeriod->setValue(det->period());
    ui->detTriggerMode->setText(det->triggerModeString());
    ui->detImageMode->setText(det->imageModeString());
    ui->detTotalImages->setValue(det->number());

    ui->detFileNameTiff->setText(det->name(Detector::TIFF));
    ui->detFileNameHdf->setText(det->name(Detector::HDF));
    ui->detFileTemplateTiff->setText(det->nameTemplate(Detector::TIFF));
    ui->detFileTemplateHdf->setText(det->nameTemplate(Detector::HDF));
    ui->detPathTiff->setText(det->path(Detector::TIFF));
    ui->detPathHdf->setText(det->path(Detector::HDF));

  }

  check (ui->detStatus, ! ui->detSelection->currentIndex() || ( det->isConnected() &&
         ( inCT || ( ! det->isAcquiring() && ! det->isWriting() ) ) ) );
  check (ui->detPathTiff, uiImageFormat() != Detector::TIFF  ||  det->pathExists(Detector::TIFF) || ! ui->detSelection->currentIndex() );
  check (ui->detPathHdf, uiImageFormat() != Detector::HDF  ||  det->pathExists(Detector::HDF) || ! ui->detSelection->currentIndex());


}





void MainWindow::onWorkingDirBrowse() {
  QDir startView( QDir::current() );
  startView.cdUp();
  const QString newdir = QFileDialog::getExistingDirectory(0, "Working directory", startView.path() );
  if ( ! newdir.isEmpty() )
    ui->expPath->setText(newdir);
}

void MainWindow::onSerialCheck() {
  ui->control->setTabVisible(ui->tabSerial, ui->checkSerial->isChecked());
  check( ui->tabSerial, true );
}

void MainWindow::onFFcheck() {
  ui->control->setTabVisible(ui->tabFF, ui->checkFF->isChecked());
  ui->ffOnEachScan->setVisible(ui->checkFF->isChecked());
  check( ui->tabFF, true );
}

void MainWindow::onDynoCheck() {
  ui->control->setTabVisible(ui->tabDyno, ui->checkDyno->isChecked());
  check( ui->tabDyno, true );

}

void MainWindow::onMultiCheck() {
  ui->control->setTabVisible(ui->tabMulti, ui->checkMulti->isChecked());
  check( ui->tabMulti, true );
}

void MainWindow::onSubLoop() {
  const bool dosl=ui->subLoop->isChecked();
  ui->placeSubLoopCurrent->setVisible(dosl);
  ui->placeSubLoopMotor->setVisible(dosl);
  ui->subLoopNumber->setVisible(dosl);
  ui->subLoopNumberLabel->setVisible(dosl);
  ui->subLoopStep->setVisible(dosl);
  ui->subLoopStepLabel->setVisible(dosl);
  ui->preSubLoopScript->setVisible(dosl);
  ui->preSubLoopScriptLabel->setVisible(dosl);
  ui->postSubLoopScript->setVisible(dosl);
  ui->postSubLoopScriptLabel->setVisible(dosl);
}

void MainWindow::onDyno2() {
  const bool dod2 = ui->dyno2->isChecked();
  ui->placeDyno2Motor->setVisible(dod2);
  ui->dynoDirectionLock->setVisible(dod2);
  ui->dynoSpeedLock->setVisible(dod2);
  ui->placeDyno2Current->setVisible(dod2);
  ui->dyno2Neg->setVisible(dod2);
  ui->dyno2NormalSpeed->setVisible(dod2);
  ui->dyno2Pos->setVisible(dod2);
  ui->dyno2Speed->setVisible(dod2);
  ui->dyno2NormalSpeedLabel->setVisible(dod2);
  ui->dyno2SpeedLabel->setVisible(dod2);
  ui->dyno2DirectionLabel->setVisible(dod2);
}

void MainWindow::onDynoSpeedLock() {
  ui->dyno2Speed->setDisabled(ui->dynoSpeedLock->isChecked());
  if (ui->dynoSpeedLock->isChecked()) {
    connect(ui->dynoSpeed, SIGNAL(valueChanged(double)),
            ui->dyno2Speed, SLOT(setValue(double)));
    ui->dyno2Speed->setValue(ui->dynoSpeed->value());
  } else
    disconnect(ui->dynoSpeed, SIGNAL(valueChanged(double)),
               ui->dyno2Speed, SLOT(setValue(double)));
}

void MainWindow::onDynoDirectionLock() {
  const bool lockDir = ui->dynoDirectionLock->isChecked();
  ui->dyno2Pos->setDisabled(lockDir);
  ui->dyno2Neg->setDisabled(lockDir);
  if (lockDir) {
    connect(ui->dynoPos, SIGNAL(toggled(bool)),
            ui->dyno2Pos, SLOT(setChecked(bool)));
    connect(ui->dynoNeg, SIGNAL(toggled(bool)),
            ui->dyno2Neg, SLOT(setChecked(bool)));
    ui->dyno2Pos->setChecked(ui->dynoPos->isChecked());
    ui->dyno2Neg->setChecked(ui->dynoNeg->isChecked());
  } else {
    disconnect(ui->dynoPos, SIGNAL(toggled(bool)),
               ui->dyno2Pos, SLOT(setChecked(bool)));
    disconnect(ui->dynoNeg, SIGNAL(toggled(bool)),
               ui->dyno2Neg, SLOT(setChecked(bool)));
  }
}











void MainWindow::onFFtest() {
  if (inFFTest) {
    stopMe=true;
    bgMotor->motor()->stop();
    det->stop();
  } else {
    ui->testFF->setText("Stop");
    inFFTest=true;
    stopMe=false;
    ui->ffWidget->setEnabled(false);
    check(ui->testFF, false);

    const QString origname = det->name(uiImageFormat());
    const int detimode = det->imageMode();

    acquireDetector( "SAMPLE" + origname ,
                    ( ui->aqMode->currentIndex() == STEPNSHOT && ! ui->checkDyno->isChecked() )  ?
                      ui->aqsPP->value() : 1);
    det->waitWritten();
    det->setName(uiImageFormat(), origname);
    acquireBG("");
    det->waitWritten();
    acquireDF("", shutter->state());
    det->waitWritten();

    det->setAutoSave(false);
    det->setName(uiImageFormat(), origname);
    det->setImageMode(detimode);

    check(ui->testFF, true);
    ui->ffWidget->setEnabled(true);
    inFFTest=false;
    ui->testFF->setText("Test");
    QTimer::singleShot(0, this, SLOT(updateUi_bgMotor()));
  }
}


void MainWindow::onLoopTest() {
  if (inMultiTest) {
    stopAll();
  } else {
    ui->testMulti->setText("Stop");
    inMultiTest=true;
    stopMe=false;
    check(ui->testMulti, false);
    acquireMulti("",  ( ui->aqMode->currentIndex() == STEPNSHOT  &&  ! ui->checkDyno->isChecked() )  ?
                      ui->aqsPP->value() : 1);
    det->waitWritten();
    check(ui->testMulti, true);
    inMultiTest=false;
    det->setAutoSave(false);
    ui->testMulti->setText("Test");
    QTimer::singleShot(0, this, SLOT(updateUi_loopMotor()));
    QTimer::singleShot(0, this, SLOT(updateUi_subLoopMotor()));
  }
}

void MainWindow::onDynoTest() {
  if (inDynoTest) {
    stopAll();
  } else {
    ui->testDyno->setText("Stop");
    stopMe=false;
    inDynoTest=true;
    check(ui->testDyno, false);
    acquireDyno("");
    check(ui->testDyno, true);
    inDynoTest=false;
    det->setAutoSave(false);
    ui->testDyno->setText("Test");
    QTimer::singleShot(0, this, SLOT(updateUi_dynoMotor()));
    QTimer::singleShot(0, this, SLOT(updateUi_dyno2Motor()));
  }
}




void MainWindow::onDetectorSelection() {
  const QString currentText = ui->detSelection->currentText();
  if (currentText.isEmpty()) {
    det->setCamera(Detector::NONE);
    setenv("DETECTORPV", det->pv().toAscii(), 1);
    return;
  } else {
    foreach (Detector::Camera cam , Detector::knownCameras)
      if (currentText==Detector::cameraName(cam)) {
        det->setCamera(cam);
        setenv("DETECTORPV", det->pv().toAscii(), 1);
        return;
      }
  }
  det->setCamera(currentText);
  setenv("DETECTORPV", det->pv().toAscii(), 1);
}


void MainWindow::onDetectorTest() {
  if ( ! det->isConnected() )
    return;
  else if (inAcquisitionTest) {
    stopAll();
  } else {
    ui->testDetector->setText("Stop");
    inAcquisitionTest=true;
    stopMe=false;
    check(ui->testDetector, false);
    acquireDetector(det->name(uiImageFormat()),
                    ( ui->aqMode->currentIndex() == STEPNSHOT && ! ui->checkDyno->isChecked() )  ?
                      ui->aqsPP->value() : 1);
    det->waitWritten();
    check(ui->testDetector, true);
    inAcquisitionTest=false;
    det->setAutoSave(false);
    ui->testDetector->setText("Test");
    updateUi_detector();
  }
}


















void MainWindow::check(QWidget * obj, bool status) {

  if ( inCT || ! obj )
    return;

  obj->setStyleSheet( status  ?  ""  :  warnStyle );

  QWidget * tab = 0;
  if ( ui->control->tabs().contains(obj) ) {

    tab = obj;

  } else if ( ! preReq.contains(obj) ) {

    foreach( QWidget * wdg, ui->control->tabs() )
      if ( wdg->findChildren< const QWidget* >().contains(obj) ) {
        tab = wdg;
        break;
      }

    if (tab)
      preReq[obj] = qMakePair(status, (const QWidget*) tab);

  } else {

    tab = (QWidget*) preReq[obj].second;
    preReq[obj].first = status;

  }

  bool tabOK=status;
  if ( status )
    foreach ( ReqP tabel, preReq)
      if ( tabel.second == tab )
        tabOK &= tabel.first;

  if (tab) {

    tabOK |= ui->control->indexOf(tab) == -1;
    preReq[tab] = qMakePair( tabOK, (const QWidget*) 0 );
    ui->control->setTabTextColor(tab, tabOK ? QColor() : QColor(Qt::red));

    const bool inRun = inCT || inAcquisitionTest || inDynoTest || inFFTest || inMultiTest;
    ui->testDetector->setEnabled ( inAcquisitionTest || ( ! inRun && preReq[ui->tabDetector].first ) );
    ui->testDyno->setEnabled ( inDynoTest || ( ! inRun &&
                                               preReq[ui->tabDetector].first &&
                                               preReq[ui->tabDyno].first ) );
    ui->testMulti->setEnabled ( inMultiTest ||
                                  ( ! inRun &&
                                    preReq[ui->tabDetector].first &&
                                    preReq[ui->tabDyno].first &&
                                    preReq[ui->tabMulti].first) );
    ui->testFF->setEnabled ( inFFTest ||
                             ( ! inRun &&
                               preReq[ui->tabDetector].first &&
                               preReq[ui->tabDyno].first &&
                               preReq[ui->tabMulti].first &&
                               preReq[ui->tabFF].first) );

  }

  readyToStartCT=status;
  if ( status )
    foreach ( ReqP tabel, preReq)
      if ( ! tabel.second )
        readyToStartCT &= tabel.first;
  ui->startStop->setEnabled(readyToStartCT || inCT);

}




















bool MainWindow::prepareDetector(const QString & filetemplate, int count) {
  const Detector::ImageFormat fmt = uiImageFormat();
  QString fileT = "%s%s";
  if (fmt == Detector::TIFF) {
    if (count>1)
      fileT += "%0" + QString::number(QString::number(count).length()) + "d";
    fileT+= ".tif";
  } else if (fmt == Detector::HDF) {
    fileT+= ".hdf";
  }

  return
      det->isConnected() &&
      det->setNameTemplate(fmt, fileT) &&
      det->setNumber(count) &&
      det->setName(fmt, filetemplate) &&
      det->prepareForAcq(fmt, count) &&
      ui->checkExtTrig->isVisible() &&
      ui->checkExtTrig->isChecked() ?
        det->setHardwareTriggering(true) : true ;

}

int MainWindow::acquireDetector() {

  int execStatus = -1;
  execStatus = ui->preAqScript->script->execute();
  if ( execStatus || stopMe )
    goto acquireDetectorExit;
  det->acquire();
  if ( ! stopMe )
    execStatus = ui->postAqScript->script->execute();

acquireDetectorExit:
  return execStatus;
}

int MainWindow::acquireDetector(const QString & filetemplate, int count) {
  if ( ! prepareDetector(filetemplate, count) || stopMe )
    return -1;
  return acquireDetector();
}


static void setMotorSpeed(QCaMotor* mot, double speed) {
  if ( ! mot )
    return;
  if ( speed >= 0.0 &&  mot->getNormalSpeed() != speed ) {
    mot->setNormalSpeed(speed);
    mot->waitUpdated<double>
        (".VELO", speed, &QCaMotor::getNormalSpeed, 500);
  }
}

static void setMotorSpeed(QCaMotorGUI* mot, double speed) {
  setMotorSpeed(mot->motor(), speed);
}




int MainWindow::acquireDyno(const QString & filetemplate, int count) {

  int ret=-1;

  if ( ! ui->checkDyno->isChecked()  || count < 1)
    return ret;

  dynoMotor->motor()->wait_stop();
  if (stopMe) return ret;
  if ( ui->dyno2->isChecked() ) {
    dyno2Motor->motor()->wait_stop();
    if (stopMe) return ret;
  }

  ui->dynoWidget->setEnabled(false);

  const double
      dStart = dynoMotor->motor()->getUserPosition(),
      d2Start = dyno2Motor->motor()->getUserPosition(),
      dnSpeed = dynoMotor->motor()->getNormalSpeed(),
      d2nSpeed = dyno2Motor->motor()->getNormalSpeed(),
      daSpeed = ui->dynoSpeed->value(),
      d2aSpeed = ui->dyno2Speed->value();

  if (count>1) {
    ui->dynoProgress->setMaximum(count);
    ui->dynoProgress->setValue(0);
    ui->dynoProgress->setVisible(true);
  }

 QString ftemplate;

  for (int curr=0 ; curr < count ; curr++) {

    if ( dynoMotor->motor()->getUserGoal() != dStart ) {
      setMotorSpeed(dynoMotor, dnSpeed);
      dynoMotor->motor()->goUserPosition(dStart, QCaMotor::STARTED);
      if (stopMe) goto acquireDynoExit;
    }
    if ( ui->dyno2->isChecked() &&
         dyno2Motor->motor()->getUserGoal() != d2Start ) {
      setMotorSpeed(dyno2Motor, d2nSpeed);
      dyno2Motor->motor()->goUserPosition(d2Start, QCaMotor::STARTED);
      if (stopMe) goto acquireDynoExit;
    }
    dynoMotor->motor()->wait_stop();
    if (stopMe) goto acquireDynoExit;
    if ( ui->dyno2->isChecked() ) {
      dyno2Motor->motor()->wait_stop();
      if (stopMe) goto acquireDynoExit;
    }

    ftemplate = filetemplate.isEmpty() ? det->name(uiImageFormat()) : filetemplate;
    if (count>1)
      ftemplate += QString("_N%1").arg(curr, QString::number(count).length(), 10, QChar('0'));
    // two stopMes below are reqiuired for the case it changes while the detector is being prepared
    if ( stopMe || ! prepareDetector(ftemplate) || stopMe )
      goto acquireDynoExit;

    setMotorSpeed(dynoMotor, daSpeed);
    if ( ui->dyno2->isChecked()  )
      setMotorSpeed(dyno2Motor, d2aSpeed);
    if (stopMe) goto acquireDynoExit;

    dynoMotor->motor()->goLimit( ui->dynoPos->isChecked() ? 1 : -1 );
    if ( ui->dyno2->isChecked() )
      dyno2Motor->motor()->goLimit( ui->dyno2Pos->isChecked() ? 1 : -1 );
    dynoMotor->motor()->wait_start();
    if ( ui->dyno2->isChecked() )
      dyno2Motor->motor()->wait_start();
    if (stopMe) goto acquireDynoExit;

    ret = acquireDetector();

    if (count>1)
      ui->dynoProgress->setValue(curr+1);

    if ( dynoMotor->motor()->getLimitStatus() ||
         ( ui->dyno2->isChecked() && dyno2Motor->motor()->getLimitStatus() ) )
      ret = 1;
    if (stopMe) goto acquireDynoExit;

  }

acquireDynoExit:

  if ( filetemplate.isEmpty() && ! ftemplate.isEmpty() )
    det->setName(uiImageFormat(), ftemplate) ;

  setMotorSpeed(dynoMotor, dnSpeed);
  if ( dynoMotor->motor()->getUserGoal() != dStart )
    dynoMotor->motor()->goUserPosition(dStart, QCaMotor::STARTED);
  if ( ui->dyno2->isChecked() ) {
    setMotorSpeed(dyno2Motor, d2nSpeed);
    if ( dyno2Motor->motor()->getUserGoal() != d2Start )
      dyno2Motor->motor()->goUserPosition(d2Start, QCaMotor::STARTED);
  }

  ui->dynoProgress->setVisible(false);
  ui->dynoWidget->setEnabled(true);

  return ret;

}







int MainWindow::acquireMulti(const QString & filetemplate, int count) {

  if ( ! ui->checkMulti->isChecked() )
    return -1;

  totalLoops = ui->loopNumber->value();
  totalSubLoops = ui->subLoop->isChecked() ? ui->subLoopNumber->value() : 1;
  currentLoop=0;
  currentSubLoop=0;

  const int
      loopDigs = QString::number(totalLoops).size(),
      subLoopDigs = QString::number(totalSubLoops).size();
  const bool
      moveLoop = loopMotor->motor()->isConnected(),
      moveSubLoop = ui->subLoop->isChecked() && subLoopMotor->motor()->isConnected();

  if (moveLoop)
    loopMotor->motor()->wait_stop();
  if (moveSubLoop)
    subLoopMotor->motor()->wait_stop();

  if (stopMe ||
      (moveLoop && loopMotor->motor()->getLimitStatus()) ||
      (moveSubLoop && subLoopMotor->motor()->getLimitStatus()) )
    return -1;

  int execStatus=-1;
  ui->multiWidget->setEnabled(false);

  const double
      lStart = loopMotor->motor()->getUserPosition(),
      slStart = subLoopMotor->motor()->getUserPosition();

  const QString ftemplate = ( filetemplate.isEmpty() ? det->name(uiImageFormat()) : filetemplate );

  for ( currentLoop = 0; currentLoop < totalLoops; currentLoop++) {

    setenv("CURRENTLOOP", QString::number(currentLoop).toAscii(), 1);

    for ( currentSubLoop = 0; currentSubLoop < totalSubLoops; currentSubLoop++) {

      setenv("CURRENTSUBLOOP", QString::number(currentLoop).toAscii(), 1);

      if (moveLoop)
        loopMotor->motor()->wait_stop();
      if (moveSubLoop)
        subLoopMotor->motor()->wait_stop();

      if (stopMe ||
          (moveLoop && loopMotor->motor()->getLimitStatus()) ||
          (moveSubLoop && subLoopMotor->motor()->getLimitStatus()) ) {
        execStatus = -1;
        goto acquireMultiExit;
      }

      QString filename = ftemplate +
          QString("_L%1").arg(currentLoop, loopDigs, 10, QChar('0')) +
          ( ui->subLoop->isChecked()  ?
              QString("_S%1").arg(currentSubLoop, subLoopDigs, 10, QChar('0')) : "");
      execStatus = ui->checkDyno->isChecked() ?
            acquireDyno(filename, count) : acquireDetector(filename, count);

      if (stopMe || execStatus )
        goto acquireMultiExit;

      if (moveSubLoop && currentSubLoop < totalSubLoops-1 )
        subLoopMotor->motor()->goUserPosition
            ( slStart + (currentSubLoop+1) * ui->subLoopStep->value(), QCaMotor::STARTED);

      if (stopMe) {
        execStatus = -1;
        goto acquireMultiExit;
      }

    }

    if (moveLoop && currentLoop < totalLoops-1)
      loopMotor->motor()->goUserPosition
          ( lStart + (currentLoop+1) * ui->loopStep->value(), QCaMotor::STARTED);
    if (moveSubLoop)
      subLoopMotor->motor()->goUserPosition( slStart, QCaMotor::STARTED);
    if (stopMe) {
      execStatus = -1;
      goto acquireMultiExit;
    }

  }

acquireMultiExit:

  if ( filetemplate.isEmpty()  &&  ! ftemplate.isEmpty() )
    det->setName(uiImageFormat(), ftemplate) ;

  if (moveLoop) {
    loopMotor->motor()->goUserPosition(lStart, QCaMotor::STARTED);
    if (!stopMe)
      loopMotor->motor()->wait_stop();
  }
  if (moveSubLoop) {
    subLoopMotor->motor()->goUserPosition(slStart, QCaMotor::STARTED);
    if (!stopMe)
      subLoopMotor->motor()->wait_stop();
  }
  currentLoop=-1;
  currentSubLoop=-1;
  ui->multiWidget->setEnabled(true);
  return execStatus;

}





void MainWindow::stopAll() {
  stopMe=true;
  emit requestToStopAcquisition();
  det->stop();
  innearList->motui->motor()->stop();
  outerList->motui->motor()->stop();
  thetaMotor->motor()->stop();
  bgMotor->motor()->stop();
  loopMotor->motor()->stop();
  subLoopMotor->motor()->stop();
  dynoMotor->motor()->stop();
  dyno2Motor->motor()->stop();
  foreach( Script * script, findChildren<Script*>() )
    script->stop();
}

void MainWindow::onStartStop() {
  if ( inCT ) {
    stopAll();
  } else {
    ui->startStop->setText("Stop CT");
    ui->startStop->setStyleSheet(warnStyle);
    stopMe=false;
    engineRun();
    ui->startStop->setText("Start CT");
    ui->startStop->setStyleSheet("");
  }
}





void MainWindow::updateProgress () {

  if (currentScan>=0) {

    if ( ! ui->serialProgress->isVisible() ) {
      if ( totalScans1D * totalScans2D > 1 ) {
        ui->serialProgress->setMaximum(totalScans1D * totalScans2D);
        ui->serialProgress->setFormat("Series progress. %p% (scans complete: %v of %m)");
      } else if ( ui->endTime->isChecked() ) {
        ui->serialProgress->setMaximum(QTime(0, 0, 0, 0).msecsTo( ui->acquisitionTime->time() ));
      } else {
        ui->serialProgress->setMaximum(0);
        ui->serialProgress->setFormat("Series progress. Scans complete: %v.");
      }
    }

    if ( ui->endTime->isChecked() ) {
      QString format = "Series progress: %p% "
          + (QTime(0, 0, 0, 0).addMSecs(inCTtime.elapsed())).toString() + " of " + ui->acquisitionTime->time().toString()
          + " (scans complete: " + QString::number(currentScan) + ")";
      ui->serialProgress->setFormat(format);
      ui->serialProgress->setValue(inCTtime.elapsed());
    } else {
      ui->serialProgress->setValue(currentScan);
    }

  }

  ui->serialProgress->setVisible(currentScan>=0);

  if (currentProjection>=0) {
    if ( ! ui->scanProgress->isVisible() ) {
      ui->scanProgress->setMaximum(totalProjections + ( ui->scanAdd->isChecked() ? 1 : 0 ));
      ui->scanProgress->setVisible(true);
    }
    ui->scanProgress->setValue(currentProjection);
  } else
    ui->scanProgress->setVisible(false);



  if (currentLoop>=0) {

    const QString progressFormat = QString("Multishot progress: %p% ; %v of %m") +
        ( ui->subLoop->isChecked() ? " (%1,%2 of %3,%4)" : "" );

    if ( ! ui->multiProgress->isVisible() ) {
      ui->multiProgress->setMaximum(totalLoops*totalSubLoops);
      if ( ! ui->subLoop->isChecked() )
        ui->multiProgress->setFormat(progressFormat);
      ui->multiProgress->setVisible(true);
    }

    if ( ui->subLoop->isChecked() ) {
      ui->multiProgress->setFormat( progressFormat
                                    .arg(currentLoop+1).arg(currentSubLoop+1)
                                    .arg(totalLoops).arg(totalSubLoops) );
    }
    ui->multiProgress->setValue( 1 + currentSubLoop + totalSubLoops * currentLoop );

  } else
    ui->multiProgress->setVisible(false);

  ui->detProgress->setVisible( ui->detSelection->currentIndex() && currentShot>=0 && totalShots>1 && det->imageMode() == 1 );

  QTimer::singleShot(50, this, SLOT(updateProgress()));

}



int MainWindow::acquireProjection(const QString &filetemplate) {
  det->waitWritten();
  QString ftemplate = "SAMPLE_" + filetemplate;
  setenv("CONTRASTTYPE", "SAMPLE", 1);
  if (ui->checkMulti->isChecked())
    return acquireMulti(ftemplate, ui->aqsPP->value());
  else if (ui->checkDyno->isChecked())
    return acquireDyno(ftemplate);
  else
    return acquireDetector(ftemplate, ui->aqsPP->value());
}


int MainWindow::acquireBG(const QString &filetemplate) {

  int ret = -1;
  const int bgs = ui->nofBGs->value();
  const double bgTravel = ui->bgTravel->value();
  const double originalExposure = det->exposure();
  const QString detfilename=det->name(uiImageFormat());
  QString ftemplate;

  if ( ! bgMotor->motor()->isConnected() || bgs <1 || bgTravel == 0.0 )
    return ret;

  bgMotor->motor()->wait_stop();
  const double bgStart = bgMotor->motor()->getUserPosition();

  bgMotor->motor()->goUserPosition
      ( bgStart + bgTravel, QCaMotor::STOPPED );
  if (stopMe) goto onBgExit;

  det->waitWritten();
  if (stopMe) goto onBgExit;

  ftemplate = "BG_" +  ( filetemplate.isEmpty() ? det->name(uiImageFormat()) : filetemplate );
  setenv("CONTRASTTYPE", "BG", 1);

  if ( ui->bgExposure->value() != ui->bgExposure->minimum() )
    det->setExposure( ui->bgExposure->value() ) ;

  det->setPeriod(0);
  if (ui->checkMulti->isChecked() && ! ui->singleBg->isChecked() )
    ret = acquireMulti(ftemplate, bgs);
  else if (ui->checkDyno->isChecked())
    ret = acquireDyno(ftemplate, bgs);
  else
    ret = acquireDetector(ftemplate, bgs);

  bgMotor->motor()->goUserPosition
      ( bgStart, QCaMotor::STOPPED );

onBgExit:

  if (!stopMe)
    det->waitWritten();

  if ( filetemplate.isEmpty() && ! detfilename.isEmpty() )
    det->setName(uiImageFormat(), detfilename) ;

  if ( ui->bgExposure->value() != ui->bgExposure->minimum() )
    det->setExposure( originalExposure ) ;

  if (!stopMe)
    bgMotor->motor()->wait_stop();

  return ret;

}



int MainWindow::acquireDF(const QString &filetemplate, Shutter::State stateToGo) {

  int ret = -1;
  const int dfs = ui->nofDFs->value();
  const QString detfilename=det->name(uiImageFormat());
  QString ftemplate;

  if ( dfs<1 )
    return 0;

  shutter->close(true);
  if (stopMe) goto onDfExit;
  det->waitWritten();
  if (stopMe) goto onDfExit;

  ftemplate = "DF_" + ( filetemplate.isEmpty() ? det->name(uiImageFormat()) : filetemplate );
  setenv("CONTRASTTYPE", "DF", 1);

  det->setPeriod(0);

  ret = acquireDetector(ftemplate, dfs);

onDfExit:

  if (!stopMe)
    det->waitWritten();

  if ( filetemplate.isEmpty()  &&  ! detfilename.isEmpty() )
    det->setName(uiImageFormat(), detfilename) ;

  if (stateToGo == Shutter::OPEN)
    shutter->open(!stopMe);
  else if (stateToGo == Shutter::CLOSED)
    shutter->close(!stopMe);
  if ( ! stopMe && shutter->state() != stateToGo)
    qtWait(shutter, SIGNAL(stateUpdated(State)), 500); /**/

  return ret;

}


void MainWindow::engineRun () {

  if ( ! readyToStartCT || inCT )
    return;

  // config and log
  QString cfgName = "acquisition.configuration";
  QString logName = "acquisition.log";
  int attempt=0;
  while ( QFile::exists(cfgName) || QFile::exists(logName) ) {
    QString prefix = "acquisition." + QString::number(++attempt);
    cfgName = prefix + ".configuration";
    logName = prefix + ".log";
  }

  saveConfiguration(cfgName);

  QProcess logProc(this);
  logProc.setWorkingDirectory(QDir::currentPath());
  QTemporaryFile logExec(this);
  if ( ! logExec.open() )
    qDebug() << "ERROR! Unable to open temporary file for log proccess.";
  logExec.write( (  QCoreApplication::applicationDirPath().remove( QRegExp("bin/*$") ) + "/libexec/ctgui.log.sh"
                    + " " + det->pv()
                    + " " + thetaMotor->motor()->getPv()
                    + " >> " + logName ).toAscii() );
  logExec.flush();
  logProc.start( "/bin/sh " + logExec.fileName() );


  totalProjections = ui->scanProjections->value();

  QCaMotor * const inSMotor = innearList->motui->motor();
  QCaMotor * const outSMotor = outerList->motui->motor();

  const double
      inSerialStart = inSMotor->getUserPosition(),
      outSerialStart = outSMotor->getUserPosition(),
      bgStart =  bgMotor->motor()->getUserPosition(),
      thetaStart = thetaMotor->motor()->getUserPosition(),
      thetaRange = ui->scanRange->value(),
      thetaSpeed = thetaMotor->motor()->getNormalSpeed();
  double thetaInSeriesStart = thetaStart;

  const bool
      doSerial1D = ui->checkSerial->isChecked(),
      doSerial2D = doSerial1D && ui->serial2D->isChecked(),
      doBG = ui->checkFF->isChecked() && ui->nofBGs->value(),
      bgBefore = ui->bgIntervalBefore->isChecked(),
      bgAfter = ui->bgIntervalAfter->isChecked(),
      dfBefore = ui->dfIntervalBefore->isChecked(),
      dfAfter = ui->dfIntervalAfter->isChecked(),
      doDF = ui->checkFF->isChecked() && ui->nofDFs->value(),
      sasMode = (ui->aqMode->currentIndex() == STEPNSHOT),
      ongoingSeries = doSerial1D && ui->ongoingSeries->isChecked(),
      doTriggCT = ! tct->prefix().isEmpty();

  totalScans1D = doSerial1D ?
    ( ui->endNumber->isChecked() ? outerList->ui->nof->value() : 0 ) : 1 ;
  totalScans2D = doSerial2D ? innearList->ui->nof->value() : 1 ;


  const int
      bgInterval = ui->bgInterval->value(),
      dfInterval = ui->dfInterval->value(),
      scanDelay = QTime(0, 0, 0, 0).msecsTo( ui->scanDelay->time() ),
      scanTime = QTime(0, 0, 0, 0).msecsTo( ui->acquisitionTime->time() ),
      projectionDigs = QString::number(totalProjections).size(),
      series1Digs = QString::number(totalScans1D-1).size(),
      series2Digs = QString::number(totalScans2D-1).size(),
      doAdd = ui->scanAdd->isChecked() ? 1 : 0,
      detimode=det->imageMode(),
      dettmode=det->triggerMode();

  stopMe = false;
  inCT = true;
  foreach(PositionList * lst, findChildren<PositionList*>())
    lst->freezList(true);
  foreach(QWidget * tab, ui->control->tabs())
        tab->setEnabled(false);
  inCTtime.restart();
  currentScan1D=0;
  bool timeToStop=false;

  int
      beforeBG = 0,
      beforeDF = 0;

  if ( doSerial1D  &&  ui->endNumber->isChecked() &&
       outSMotor->isConnected() &&  outerList->ui->irregular->isChecked() ) // otherwise is already in the first point
    outSMotor->goUserPosition( outerList->ui->list->item(0, 0)->text().toDouble(), QCaMotor::STARTED);
  if (stopMe) goto onEngineExit;

  if ( doSerial2D && inSMotor->isConnected() &&  innearList->ui->irregular->isChecked() ) // otherwise is already in the first point
    inSMotor->goUserPosition( innearList->ui->list->item(0, 0)->text().toDouble(), QCaMotor::STARTED);
  if (stopMe) goto onEngineExit;


  if (doTriggCT) {
    tct->setStartPosition(thetaStart, true);
    //tct->setStep( thetaRange / ui->scanProjections->value(), true);
    tct->setRange( thetaRange , true);
    qtWait(500); // otherwise NofTrigs are set, but step is not recalculated ...((    int digs=0;
    tct->setNofTrigs(totalProjections + doAdd, true);
  }

  ui->preRunScript->script->execute();
  if (stopMe) goto onEngineExit;

  shutter->open(true);

  do { // serial scanning 1D

    setenv("CURRENTOSCAN", QString::number(currentScan1D).toAscii(), 1);

    QString seriesNamePrefix;
    if (totalScans1D==1)
      seriesNamePrefix = "";
    else if (totalScans1D>1)
      seriesNamePrefix = QString("Y%1_").arg(currentScan1D, series1Digs, 10, QChar('0') );
    else
      seriesNamePrefix = "Y" + QString::number(currentScan1D) + "_";
    QString seriesName=seriesNamePrefix;

    if (doSerial1D  && outSMotor->isConnected())
      outSMotor->wait_stop();
    if (stopMe) goto onEngineExit;

    currentScan2D=0;
    if(doSerial2D)
      ui->pre2DScript->script->execute();
    if (stopMe) goto onEngineExit;

    do { // serial scanning 2D

      if ( totalScans2D > 1 )
        seriesName = seriesNamePrefix + QString("Z%1_").arg(currentScan2D, series2Digs, 10, QChar('0') );

      setenv("CURRENTISCAN", QString::number(currentScan2D).toAscii(), 1);
      setenv("CURRENTSCAN", QString::number(currentScan).toAscii(), 1);

      if (doSerial2D  && inSMotor->isConnected())
        inSMotor->wait_stop();
      if (stopMe) goto onEngineExit;

      ui->preScanScript->script->execute();
      if (stopMe) goto onEngineExit;

      if ( sasMode ) { // STEP-AND-SHOT

        currentProjection = 0;
        if ( ! ongoingSeries ) {
          beforeBG=0;
          beforeDF=0;
        }
        QString projectionName;

        do {

          setenv("CURRENTPROJECTION", QString::number(currentProjection).toAscii(), 1);

          projectionName = seriesName +
              QString("T%1").arg(currentProjection, projectionDigs, 10, QChar('0') );

          if (doDF && ! beforeDF) {
            acquireDF(projectionName, Shutter::OPEN);
            beforeDF = dfInterval;
            if (stopMe) goto onEngineExit;
          }
          beforeDF--;
          if (doBG && ! beforeBG) {
            acquireBG(projectionName);
            beforeBG = bgInterval;
            if (stopMe) goto onEngineExit;
          }
          beforeBG--;

          thetaMotor->motor()->wait_stop();
          if ( ! currentProjection )
            thetaInSeriesStart = ongoingSeries ?
                  thetaMotor->motor()->getUserPosition() :
                  thetaStart;
          if (stopMe) goto onEngineExit;

          acquireProjection(projectionName);
          if (stopMe) goto onEngineExit;

          currentProjection++;
          if ( currentProjection < totalProjections + doAdd )
            thetaMotor->motor()->goUserPosition
                ( thetaInSeriesStart + thetaRange * currentProjection / totalProjections, QCaMotor::STARTED);
          else if ( ! ongoingSeries )
            thetaMotor->motor()->goUserPosition( thetaStart, QCaMotor::STARTED );
          if (stopMe) goto onEngineExit;

        } while ( currentProjection < totalProjections + doAdd );

        if (doBG && ! beforeBG) {
          acquireBG(projectionName);
          if (stopMe) goto onEngineExit;
          beforeBG = bgInterval;
        }
        if ( doDF && ! beforeDF ) {
          acquireDF(projectionName, Shutter::OPEN);
          if (stopMe) goto onEngineExit;
          beforeDF = dfInterval;
        }

      } else { // CONTINIOUS

        if ( doDF && dfBefore && (ui->ffOnEachScan->isChecked() || ! currentScan ) ) {
          acquireDF(seriesName + "BEFORE", Shutter::OPEN);
          if (stopMe) goto onEngineExit;
        }
        if ( doBG  && bgBefore && (ui->ffOnEachScan->isChecked() || ! currentScan ) ) {
          acquireBG(seriesName + "BEFORE");
          if (stopMe) goto onEngineExit;
        }

        const double speed = thetaRange * ui->flyRatio->value() / ( det->exposure() * totalProjections );
        const double accTime = thetaMotor->motor()->getAcceleration();
        const double accTravel = speed * accTime / 2;
        const double rotDir = copysign(1,thetaRange);
        const double addTravel = 2*rotDir*accTravel + (doTriggCT ? 2.0 : 0.0);
        // 2 coefficient in the above string is required to guarantee that the
        // motor has accelerated to the normal speed before the acquisition starts.


        det->waitWritten();
        if (stopMe) goto onEngineExit;

        prepareDetector("SAMPLE_"+seriesName+"T", totalProjections + doAdd);
        if (stopMe) goto onEngineExit;

        if (doTriggCT || ui->checkExtTrig->isChecked() ) {
          det->setHardwareTriggering(true);
          det->setPeriod(0);
        } else {
          det->setPeriod( qAbs(thetaRange) / (totalProjections * speed)  ) ;
        }

        if ( ! ongoingSeries || ! currentScan || doTriggCT ) {

          thetaMotor->motor()->wait_stop();
          if (doTriggCT)
            tct->stop(false);
          if (stopMe) goto onEngineExit;

          thetaMotor->motor()->goRelative( -addTravel, QCaMotor::STOPPED);
          if (stopMe) goto onEngineExit;

          setMotorSpeed(thetaMotor, speed);
          if (stopMe) goto onEngineExit;

          if ( ! doTriggCT ) { // should be started after tct and det
            thetaMotor->motor()-> /* goRelative( ( thetaRange + addTravel ) * 1.05 ) */
                goLimit( thetaRange > 0 ? 1 : -1 );
            if (stopMe) goto onEngineExit;
            // accTravel/speed in the below string is required to compensate
            // for the coefficient 2 in addTravel
            // qtWait(1000*(accTime + accTravel/speed));
            usleep(1000000*(accTime + accTravel/speed));

            if (stopMe) goto onEngineExit;
          }

        }

        setenv("CONTRASTTYPE", "SAMPLE", 1);
        ui->preAqScript->script->execute();
        det->start();
        if (doTriggCT) {
          tct->start(true);
          thetaMotor->motor()-> /* goRelative( ( thetaRange + addTravel ) * 1.05 ) */
              goLimit( thetaRange > 0 ? 1 : -1 );
        }

        if (stopMe) goto onEngineExit;
        qtWait( QList<ObjSig> ()
                << ObjSig(det, SIGNAL(done()))
                << ObjSig(thetaMotor->motor(), SIGNAL(stopped())) );
        if (stopMe) goto onEngineExit;
        if (det->isAcquiring()) {
          qtWait( det, SIGNAL(done()), 1000 );
          det->stop();
        }
        if (stopMe) goto onEngineExit;
        ui->postAqScript->script->execute();
        if (stopMe) goto onEngineExit;

        if ( ! ongoingSeries ) {

          thetaMotor->motor()->stop(QCaMotor::STOPPED);
          if (stopMe) goto onEngineExit;
          setMotorSpeed(thetaMotor, thetaSpeed);
          thetaMotor->motor()->goUserPosition( thetaStart, QCaMotor::STARTED );
          if (stopMe) goto onEngineExit;

          if ( doBG  && bgAfter && ui->ffOnEachScan->isChecked() ) {
            acquireBG(seriesName + "AFTER");
            if (stopMe) goto onEngineExit;
          }
          if ( doDF && dfAfter && ui->ffOnEachScan->isChecked() ) {
            acquireDF(seriesName + "AFTER", Shutter::OPEN);
            if (stopMe) goto onEngineExit;
          }

        }

      }

      ui->postScanScript->script->execute();
      if (stopMe) goto onEngineExit;

      currentProjection=-1;
      currentScan2D++;

      timeToStop = currentScan2D >= totalScans2D
          || ( ui->endTime->isChecked() &&  inCTtime.elapsed() + scanDelay >= scanTime )
          || ( ui->endCondition->isChecked()  &&  ui->conditionScript->script->execute() );

      if ( ! timeToStop ) {
        if (doSerial2D  && inSMotor->isConnected())
          inSMotor->goUserPosition( innearList->ui->list->item(currentScan2D, 0)->text().toDouble(), QCaMotor::STARTED);
        if (stopMe) goto onEngineExit;
        qtWait(this, SIGNAL(requestToStopAcquisition()), scanDelay);
        if (stopMe) goto onEngineExit;
      }


    } while ( ! timeToStop ); // innear loop


    if(doSerial2D)
      ui->post2DScript->script->execute();
    if (stopMe) goto onEngineExit;

    currentScan1D++;

    timeToStop =
        ! doSerial1D
        || ( ui->endNumber->isChecked()  &&  currentScan1D >= totalScans1D )
        || ( ui->endTime->isChecked()  &&  inCTtime.elapsed() + scanDelay >= scanTime )
        || ( ui->endCondition->isChecked()  &&  ui->conditionScript->script->execute() );

    if ( ! timeToStop ) {
      if ( doSerial1D  &&  ui->endNumber->isChecked()  &&  outSMotor->isConnected() )
        outSMotor->goUserPosition(outerList->ui->list->item(currentScan1D, 0)->text().toDouble(), QCaMotor::STARTED);
      if ( doSerial2D  &&  inSMotor->isConnected() )
        inSMotor->goUserPosition( innearList->ui->list->item(0, 0)->text().toDouble(), QCaMotor::STARTED);
      if (stopMe) goto onEngineExit;
      qtWait(this, SIGNAL(requestToStopAcquisition()), scanDelay);
      if (stopMe) goto onEngineExit;
    }

    if ( timeToStop && ! sasMode && ! ui->ffOnEachScan->isChecked() ) {
      if ( doBG && bgAfter ) {
        acquireBG(seriesName + "AFTER");
        if (stopMe) goto onEngineExit;
      }
      if ( doDF && dfAfter ) {
        acquireDF(seriesName + "AFTER", Shutter::OPEN);
        if (stopMe) goto onEngineExit;
      }
    }

  } while ( ! timeToStop );

  ui->postRunScript->script->execute();
  shutter->close();


onEngineExit:

  thetaMotor->motor()->stop(QCaMotor::STOPPED);
  setMotorSpeed(thetaMotor, thetaSpeed);
  thetaMotor->motor()->goUserPosition(thetaStart, QCaMotor::STARTED);

  if (doSerial1D && outSMotor->isConnected()) {
    outSMotor->stop(QCaMotor::STOPPED);
    outSMotor->goUserPosition(outSerialStart, QCaMotor::STARTED);
  }
  if (doSerial2D && inSMotor->isConnected()) {
    inSMotor->stop(QCaMotor::STOPPED);
    inSMotor->goUserPosition(inSerialStart, QCaMotor::STARTED);
  }
  if ( doBG ) {
    bgMotor->motor()->stop(QCaMotor::STOPPED);
    bgMotor->motor()->goUserPosition(bgStart, QCaMotor::STARTED);
  }

  det->stop();
  det->waitWritten();
  det->setName(uiImageFormat(), ".temp") ;
  det->setAutoSave(false);
  det->setImageMode(detimode);
  det->setTriggerMode(dettmode);

  QProcess::execute("killall -9 camonitor");
  //QProcess::execute(  QString("kill $( pgrep -l -P %1 | grep camonitor | cut -d' '  -f 1 )").arg( int(logProc.pid())) );
  logProc.terminate();
  logExec.close();

  QTimer::singleShot(0, this, SLOT(updateUi_thetaMotor()));
  QTimer::singleShot(0, this, SLOT(updateUi_bgMotor()));
  QTimer::singleShot(0, this, SLOT(updateUi_loopMotor()));
  QTimer::singleShot(0, this, SLOT(updateUi_subLoopMotor()));
  QTimer::singleShot(0, this, SLOT(updateUi_dynoMotor()));
  QTimer::singleShot(0, this, SLOT(updateUi_dyno2Motor()));
  QTimer::singleShot(0, this, SLOT(updateUi_detector()));
  QTimer::singleShot(0, this, SLOT(updateUi_serials()));
//  QTimer::singleShot(0, this, SLOT(updateUi_shutterStatus()));
//  QTimer::singleShot(0, this, SLOT(updateUi_expPath()));

  foreach(PositionList * lst, findChildren<PositionList*>())
    lst->freezList(false);
  foreach(QWidget * tab, ui->control->tabs())
        tab->setEnabled(true);
  currentScan1D = -1;
  currentScan2D = -1;
  currentProjection = -1;
  inCT = false;



}



