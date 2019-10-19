#include <QDebug>
#include <QFileDialog>
#include <QVector>
#include <QProcess>
#include <QtCore>
#include <QSettings>
#include <QFile>
#include <QTime>
#include <unistd.h>
#include <QApplication>
#include <QMessageBox>

#include "additional_classes.h"
#include "ctgui_mainwindow.h"
#include "ui_ctgui_mainwindow.h"




#define innearList dynamic_cast<PositionList*> ( ui->innearListPlace->layout()->itemAt(0)->widget() )
#define outerList dynamic_cast<PositionList*> ( ui->outerListPlace->layout()->itemAt(0)->widget() )
#define loopList dynamic_cast<PositionList*> ( ui->loopListPlace->layout()->itemAt(0)->widget() )
#define sloopList dynamic_cast<PositionList*> ( ui->sloopListPlace->layout()->itemAt(0)->widget() )



static const QString warnStyle = "background-color: rgba(255, 0, 0, 128);";
static const QString ssText = "Start experiment";

const QString MainWindow::storedState = QDir::homePath()+"/.ctgui";


class HWstate {

private:
  Detector * det;
  Shutter * shut;
  Shutter::State shutsate;
  QString dettifname;
  QString dethdfname;
  int detimode;
  int dettmode;
  float detperiod;

public:

  HWstate(Detector *_det, Shutter * _shut)
    : det(_det), shut(_shut)
  {
    store();
  }

  void store() {
    shutsate = shut->state();
    dettifname = det->name(Detector::TIFF);
    dethdfname = det->name(Detector::HDF);
    detimode = det->imageMode();
    dettmode = det->triggerMode();
    detperiod = det->period();
  }

  void restore() {
    // shut->setState(shutsate);
    det->setName(Detector::TIFF, dettifname) ;
    det->setName(Detector::HDF, dethdfname) ;
    det->setImageMode(detimode);
    det->setTriggerMode(dettmode);
    det->setPeriod(detperiod);
  }

};



MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  isLoadingState(false),
  ui(new Ui::MainWindow),
  shutter(new Shutter(this)),
  det(new Detector(this)),
  tct(new TriggCT(this)),
  thetaMotor(new QCaMotorGUI),
  bgMotor(new QCaMotorGUI),
  dynoMotor(new QCaMotorGUI),
  dyno2Motor(new QCaMotorGUI),
  stopMe(true)
{

  ui->setupUi(this);
  ui->control->finilize();
  ui->control->setCurrentIndex(0);
  foreach ( QMDoubleSpinBox * spb , findChildren<QMDoubleSpinBox*>() )
    spb->setConfirmationRequired(false);
  foreach ( QMSpinBox * spb , findChildren<QMSpinBox*>() )
    spb->setConfirmationRequired(false);

  prsSelection << ui->aqsSpeed << ui->stepTime << ui->flyRatio;
  selectPRS(); // initial selection only
  foreach (QMDoubleSpinBox* _prs, prsSelection)
    connect( _prs, SIGNAL(entered()), SLOT(selectPRS()) );

  ColumnResizer * resizer;
  resizer = new ColumnResizer(this);
  ui->preRunScript->addToColumnResizer(resizer);
  ui->postRunScript->addToColumnResizer(resizer);
  resizer = new ColumnResizer(this);
  ui->preScanScript->addToColumnResizer(resizer);
  ui->postScanScript->addToColumnResizer(resizer);
  resizer = new ColumnResizer(this);
  ui->preSubLoopScript->addToColumnResizer(resizer);
  ui->postSubLoopScript->addToColumnResizer(resizer);
  resizer = new ColumnResizer(this);
  ui->preAqScript->addToColumnResizer(resizer);
  ui->postAqScript->addToColumnResizer(resizer);

  ui->startStop->setText(ssText);

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
  loopList->putMotor(new QCaMotorGUI);
  sloopList->putMotor(new QCaMotorGUI);
  ui->placeThetaMotor->layout()->addWidget(thetaMotor->setupButton());
  ui->placeScanCurrent->layout()->addWidget(thetaMotor->currentPosition(true));
  ui->placeBGmotor->layout()->addWidget(bgMotor->setupButton());
  ui->placeBGcurrent->layout()->addWidget(bgMotor->currentPosition(true));
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
  connect(ui->testSerial, SIGNAL(clicked(bool)), SLOT(onSerialTest()));
  connect(ui->testFF, SIGNAL(clicked()), SLOT(onFFtest()));
  connect(ui->testScan, SIGNAL(clicked()), SLOT(onScanTest()));
  connect(ui->testMulti, SIGNAL(clicked()), SLOT(onLoopTest()));
  connect(ui->testDyno, SIGNAL(clicked()), SLOT(onDynoTest()));
  connect(ui->dynoDirectionLock, SIGNAL(toggled(bool)), SLOT(onDynoDirectionLock()));
  connect(ui->dynoSpeedLock, SIGNAL(toggled(bool)), SLOT(onDynoSpeedLock()));
  connect(ui->dyno2, SIGNAL(toggled(bool)), SLOT(onDyno2()));
  connect(ui->testDetector, SIGNAL(clicked()), SLOT(onDetectorTest()));
  connect(ui->startStop, SIGNAL(clicked()), SLOT(onStartStop()));
  connect(ui->swapSerialLists, SIGNAL(clicked(bool)), SLOT(onSwapSerial()));
  connect(ui->swapLoopLists, SIGNAL(clicked(bool)), SLOT(onSwapLoops()));



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
  updateUi_loops();
  updateUi_dynoSpeed();
  updateUi_dynoMotor();
  updateUi_dyno2Speed();
  updateUi_dyno2Motor();
  updateUi_detector();
  updateUi_aqMode();
  updateUi_hdf();

  }

  onSerialCheck();
  onFFcheck();
  onDynoCheck();
  onMultiCheck();
  onDyno2();
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
  configNames[ui->subLoop] = "loop/subloop";
  configNames[loopList] = "loop/loopseries";
  configNames[sloopList] = "loop/subloopseries";
  configNames[ui->preSubLoopScript] = "loop/presubscript";
  configNames[ui->postSubLoopScript] = "loop/postsubscript";

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

  config.setValue("version", QString::number(APP_VERSION));

  foreach (QObject * obj, configNames.keys())
    save_cfg(obj, configNames[obj], config);

  const QMDoubleSpinBox * prs = selectedPRS();
  if (prs)
    config.setValue("scan/fixprs", prs->objectName());

  config.beginGroup("hardware");
  if (det) {
    config.setValue("detectorpv", det->pv());
    config.setValue("detectorexposure", det->exposure());
    config.setValue("detectorsavepath", det->path(uiImageFormat()));
  }
  config.endGroup();

  setenv("SERIALMOTORIPV", innearList->motui->motor()->getPv().toLatin1() , 1);
  setenv("SERIALMOTOROPV", outerList->motui->motor()->getPv().toLatin1() , 1);
  setenv("LOOPMOTORIPV", loopList->motui->motor()->getPv().toLatin1() , 1);
  setenv("SUBLOOPMOTOROPV", sloopList->motui->motor()->getPv().toLatin1() , 1);


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
  } else if ( dynamic_cast<QSpinBox*>(obj) && value.canConvert(QVariant::Int) )
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
    load_cfg(pl->ui->irregular, key + "/irregular", config);
    load_cfg(pl->ui->nof, key + "/nofsteps", config);
    load_cfg(pl->ui->step, key + "/step", config);
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
  selectPRS(selectedPRS()); // if nothing found

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
    lastComponent.replace("\\", "/");
  } else if ( lastComponent.endsWith("/") ) // can be win or lin path delimiter
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


void MainWindow::updateUi_hdf() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_hdf());
    connect(ui->aqMode, SIGNAL(currentIndexChanged(int)), thisSlot);
    connect(ui->imageFormatGroup, SIGNAL(buttonClicked(int)), thisSlot);
    connect(ui->detSelection, SIGNAL(currentIndexChanged(int)), thisSlot);
    connect(det, SIGNAL(connectionChanged(bool)), thisSlot);
    connect(det, SIGNAL(parameterChanged()), thisSlot);
  }
  const bool flymode = (AqMode) ui->aqMode->currentIndex() != STEPNSHOT;
  ui->hdfFormat->setEnabled( det->hdfReady() && flymode );
  check( ui->hdfFormat, ! ui->hdfFormat->isChecked() ||
         ( flymode && ( det->hdfReady() || ! det->isConnected() ) ) );
}


void MainWindow::updateUi_serials() {

  if ( ! sender() ) { // called from the constructor;

    const char* thisSlot = SLOT(updateUi_serials());
    connect(ui->endConditionButtonGroup, SIGNAL(buttonClicked(int)), thisSlot);
    connect(ui->serial2D, SIGNAL(toggled(bool)), thisSlot);
    connect(innearList, SIGNAL(amOKchanged(bool)), thisSlot);
    connect(outerList, SIGNAL(amOKchanged(bool)), thisSlot);

    connect(ui->serial2D, SIGNAL(toggled(bool)), ui->innearListPlace, SLOT(setVisible(bool)));
    connect(ui->serial2D, SIGNAL(toggled(bool)), ui->pre2DScript, SLOT(setVisible(bool)));
    connect(ui->serial2D, SIGNAL(toggled(bool)), ui->pre2DSlabel, SLOT(setVisible(bool)));
    connect(ui->serial2D, SIGNAL(toggled(bool)), ui->post2DScript, SLOT(setVisible(bool)));
    connect(ui->serial2D, SIGNAL(toggled(bool)), ui->post2DSlabel, SLOT(setVisible(bool)));
    ui->serial2D->toggle(); ui->serial2D->toggle(); // twice to keep initial state

    connect(ui->endNumber, SIGNAL(toggled(bool)), ui->outerListPlace, SLOT(setVisible(bool)));
    connect(ui->endTime,   SIGNAL(toggled(bool)), ui->acquisitionTimeWdg, SLOT(setVisible(bool)));
    connect(ui->endTime,   SIGNAL(toggled(bool)), ui->acquisitionTimeLabel, SLOT(setVisible(bool)));
    connect(ui->endCondition, SIGNAL(toggled(bool)), ui->conditionScript, SLOT(setVisible(bool)));
    connect(ui->endCondition, SIGNAL(toggled(bool)), ui->conditionScriptLabel, SLOT(setVisible(bool)));
    ui->endTime->toggle();
    ui->endCondition->toggle();
    ui->endNumber->toggle();

  }


  ui->swapSerialLists->setVisible( ui->serial2D->isChecked() && ui->endNumber->isChecked() );
  ui->serialTabSpacer->setHidden( ui->serial2D->isChecked() || ui->endNumber->isChecked() );
  innearList->putLabel("innear\nloop\n\n[Z]");
  outerList->putLabel(ui->serial2D->isChecked() ? "outer\nloop\n\n[Y]" : "[Y]");

  check(ui->serial2D, ! ui->serial2D->isChecked() || innearList->amOK() );
  check(ui->endNumber, ! ui->endNumber->isChecked() || outerList->amOK() );

}

void MainWindow::onSwapSerial() {
  PositionList * ol = outerList;
  PositionList * il = innearList;
  ui->innearListPlace->layout()->addWidget(ol);
  ui->outerListPlace->layout()->addWidget(il);
  configNames[innearList] = "serial/innearseries";
  configNames[outerList] = "serial/outerseries";
  updateUi_serials();
  storeCurrentState();
}


void MainWindow::updateUi_ffOnEachScan() {
    if ( ! sender() ) // called from the constructor;
        connect( ui->ongoingSeries, SIGNAL(toggled(bool)), SLOT(updateUi_ffOnEachScan()));
    ui->ffOnEachScan->setEnabled( ! ui->ongoingSeries->isChecked() );
    if ( ui->ongoingSeries->isChecked() )
        ui->ffOnEachScan->setChecked(false);
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



QMDoubleSpinBox * MainWindow::selectPRS(QObject *prso) {

  QMDoubleSpinBox * prs;
  prs = dynamic_cast<QMDoubleSpinBox*>(prso);
  if ( ! prs )
    prs = dynamic_cast<QMDoubleSpinBox*>(sender());
  if ( ! prs || ! prsSelection.contains(prs) )
    prs=selectedPRS();
  if ( ! prs )
    prs=prsSelection.at(0);

  foreach (QMDoubleSpinBox* _prs, prsSelection) {
    _prs->setFrame(_prs==prs);
    _prs->setButtonSymbols(
          _prs == prs ? QAbstractSpinBox::UpDownArrows : QAbstractSpinBox::NoButtons );
  }

  return prs;

}


QMDoubleSpinBox * MainWindow::selectedPRS() const {
  QMDoubleSpinBox * prs=0;
  int aprs=0;
  foreach (QMDoubleSpinBox* _prs, prsSelection)
    if (_prs->hasFrame()) {
      prs = _prs;
      aprs++;
    }
  return  (aprs == 1)  ?  prs  :  0 ;
}



void MainWindow::updateUi_expOverStep() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_expOverStep());
    foreach (QMDoubleSpinBox* _prs, prsSelection)
      connect( _prs, SIGNAL(valueChanged(double)), thisSlot);
    connect( ui->aqMode, SIGNAL(currentIndexChanged(int)), thisSlot);
    connect( ui->scanRange, SIGNAL(valueChanged(double)), thisSlot);
    connect( ui->scanProjections, SIGNAL(valueChanged(int)), thisSlot);
    connect( det, SIGNAL(parameterChanged()), thisSlot);
    connect( thetaMotor->motor(), SIGNAL(changedMaximumSpeed(double)), thisSlot);
  }

  const float stepSize = ui->scanRange->value() / ui->scanProjections->value();
  const float exposure = det->exposure();
  float stepTime = ui->stepTime->value();
  float aqsSpeed = ui->aqsSpeed->value();
  float flyRatio = ui->flyRatio->value();
  const float maxSpeed = thetaMotor->motor()->getMaximumSpeed();

  bool isOK =  true;
  const QMDoubleSpinBox * prs=selectedPRS();
  if ( prs == ui->stepTime ) {
    flyRatio = exposure / stepTime;
    ui->flyRatio->setValue(flyRatio);
    aqsSpeed = stepSize / stepTime;
    ui->aqsSpeed->setValue(aqsSpeed);
  } else if ( prs == ui->flyRatio ) {
    stepTime = exposure / flyRatio ;
    ui->stepTime->setValue(stepTime);
    aqsSpeed = stepSize * flyRatio / exposure ;
    ui->aqsSpeed->setValue(aqsSpeed);
  } else if ( prs == ui->aqsSpeed ) {
    stepTime = stepSize / aqsSpeed;
    ui->stepTime->setValue(stepTime);
    flyRatio = aqsSpeed * exposure / stepSize;
    ui->flyRatio->setValue(flyRatio);
  } else {
    qDebug() << "ERROR! PRS selection is buggy. Report to developper. Must never happen.";
    isOK=false;
  }

  ui->stepTime->setMinimum(exposure);
  ui->aqsSpeed->setMaximum(maxSpeed);

  isOK &=  flyRatio <= 1  &&  aqsSpeed <= maxSpeed  &&  stepTime >= exposure;
  isOK |= ui->aqMode->currentIndex() == STEPNSHOT;
  foreach (QMDoubleSpinBox* _prs, prsSelection)
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

  setenv("ROTATIONMOTORPV", mot->getPv().toLatin1() , 1);

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



void MainWindow::updateUi_loops() {
  if ( ! sender() ) { // called from the constructor;

    const char* thisSlot = SLOT(updateUi_loops());
    connect(ui->checkMulti, SIGNAL(toggled(bool)), thisSlot);
    connect(ui->subLoop, SIGNAL(toggled(bool)), thisSlot);
    connect(loopList, SIGNAL(amOKchanged(bool)), thisSlot);
    connect(sloopList, SIGNAL(amOKchanged(bool)), thisSlot);

    connect(ui->subLoop, SIGNAL(toggled(bool)), ui->sloopListPlace, SLOT(setVisible(bool)));
    connect(ui->subLoop, SIGNAL(toggled(bool)), ui->preSubLoopScript, SLOT(setVisible(bool)));
    connect(ui->subLoop, SIGNAL(toggled(bool)), ui->preSubLoopScriptLabel, SLOT(setVisible(bool)));
    connect(ui->subLoop, SIGNAL(toggled(bool)), ui->postSubLoopScript, SLOT(setVisible(bool)));
    connect(ui->subLoop, SIGNAL(toggled(bool)), ui->postSubLoopScriptLabel, SLOT(setVisible(bool)));
    connect(ui->subLoop, SIGNAL(toggled(bool)), ui->swapLoopLists, SLOT(setVisible(bool)));
    ui->subLoop->toggle(); ui->subLoop->toggle(); // twice to keep original

  }

  loopList->putLabel("loop\n\n[L]");
  sloopList->putLabel("sub\nloop\n\n[S]");

  const bool loopisOk = ! ui->checkMulti->isChecked()  ||  ! ui->subLoop->isChecked()  || sloopList->amOK();
  check(ui->subLoop, loopisOk );
  check(sloopList->ui->label, loopisOk );
  check(loopList->ui->label, ! ui->checkMulti->isChecked()  ||  loopList->amOK() );

}


void MainWindow::onSwapLoops() {
  PositionList * ll = loopList;
  PositionList * sl = sloopList;
  ui->loopListPlace->layout()->addWidget(sl);
  ui->sloopListPlace->layout()->addWidget(ll);
  configNames[loopList] = "loop/loopseries";
  configNames[sloopList] = "loop/subloopseries";
  updateUi_loops();
  storeCurrentState();
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
    connect(ui->detSelection, SIGNAL(currentIndexChanged(int)), SLOT(onDetectorSelection()));
    connect(ui->detSelection, SIGNAL(currentIndexChanged(int)), thisSlot);
    connect(det, SIGNAL(connectionChanged(bool)), thisSlot);
    connect(det, SIGNAL(parameterChanged()), thisSlot);
    connect(det, SIGNAL(counterChanged(int)), ui->detProgress, SLOT(setValue(int)));
    connect(det, SIGNAL(counterChanged(int)), ui->detImageCounter, SLOT(setValue(int)));
    connect(det, SIGNAL(lastNameChanged(QString)), ui->detFileLastName, SLOT(setText(QString)));
  }

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
    ui->detProgress->setValue(det->counter());
    ui->detProgress->setVisible( det->isAcquiring()  &&  det->imageMode() == 1 );
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
         ( inRun(ui->startStop) || ( ! det->isAcquiring() && ! det->isWriting() ) ) ) );
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


void MainWindow::onDetectorSelection() {
  const QString currentText = ui->detSelection->currentText();
  if (currentText.isEmpty()) {
    det->setCamera(Detector::NONE);
    setenv("DETECTORPV", det->pv().toLatin1(), 1);
    return;
  } else {
    foreach (Detector::Camera cam , Detector::knownCameras)
      if (currentText==Detector::cameraName(cam)) {
        det->setCamera(cam);
        setenv("DETECTORPV", det->pv().toLatin1(), 1);
        return;
      }
  }
  det->setCamera(currentText);
  setenv("DETECTORPV", det->pv().toLatin1(), 1);
}






void MainWindow::check(QWidget * obj, bool status) {

  if ( ! obj )
    return;

  obj->setStyleSheet( status  ?  ""  :  warnStyle );

  QWidget * tab = 0;
  if ( ui->control->tabs().contains(obj) ) {

    tab = obj;

  } else if ( ! preReq.contains(obj) ) {

    if (obj == ui->startStop)
      tab = ui->control;
    else
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

  const bool anyInRun = inRun(ui->startStop)
      || inRun(ui->testDetector)
      || inRun(ui->testDyno)
      || inRun(ui->testFF)
      || inRun(ui->testMulti)
      || inRun(ui->testSerial)
      || inRun(ui->testScan);

  if (tab) {

    tabOK |= ui->control->indexOf(tab) == -1;
    preReq[tab] = qMakePair( tabOK, (const QWidget*) 0 );
    ui->control->setTabTextColor(tab, tabOK ? QColor() : QColor(Qt::red));

    ui->testDetector->setEnabled ( inRun(ui->testDetector) ||
                                   ( ! anyInRun  &&  preReq[ui->tabDetector].first ) );
    ui->testDyno->setEnabled ( inRun(ui->testDyno) || ( ! anyInRun &&
                                               preReq[ui->tabDetector].first &&
                                               preReq[ui->tabDyno].first ) );
    ui->testMulti->setEnabled ( inRun(ui->testMulti) || ( ! anyInRun &&
                                                 preReq[ui->tabDetector].first &&
                                                 preReq[ui->tabDyno].first &&
                                                 preReq[ui->tabMulti].first) );
    ui->testFF->setEnabled ( inRun(ui->testFF) ||  ( ! anyInRun &&
                               preReq[ui->tabDetector].first &&
                               preReq[ui->tabDyno].first &&
                               preReq[ui->tabMulti].first &&
                               preReq[ui->tabFF].first) );
    ui->testSerial->setEnabled( inRun(ui->testSerial) || ( ! anyInRun &&
                                    preReq[ui->tabDetector].first &&
                                    preReq[ui->tabSerial].first  ) );
    ui->testScan->setEnabled( inRun(ui->testScan) || ( ! anyInRun &&
                                    preReq[ui->tabDetector].first &&
                                    preReq[ui->tabScan].first  ) );

  }

  bool readyToStartCT=status;
  if ( status )
    foreach ( ReqP tabel, preReq)
      if ( ! tabel.second )
        readyToStartCT &= tabel.first;
  ui->startStop->setEnabled(readyToStartCT || anyInRun );
  ui->startStop->setStyleSheet( anyInRun  ?  warnStyle  :  "" );
  ui->startStop->setText( anyInRun  ?  "Stop"  :  ssText );

}



// Marks the btn to indicate the process running or not.
// Used *Test functions
QString MainWindow::mkRun(QAbstractButton * btn, bool inr, const QString & txt) {
  if ( ! btn )
    return "";
  check(btn, ! inr);
  const QString ret = btn->text();
  if ( ! txt.isEmpty() )
    btn->setText(txt);
  return ret;
}

// Returns true if the marked btn indicates the process is running.
// Used *Test functions
bool MainWindow::inRun(const QAbstractButton * btn) {
  return btn && preReq.contains(btn) && ! preReq[btn].first;
}





void MainWindow::onSerialTest() {

  if (inRun(ui->testSerial)) {
    stopAll();
    return;
  }

  stopMe=false;
  const QString butText = mkRun(ui->testSerial, true, "Stop");
  det->waitWritten();
  HWstate hw(det, shutter);
  ui->ssWidget->setEnabled(false);

  shutter->open();
  engineRun();

  hw.restore();
  ui->ssWidget->setEnabled(true);
  mkRun(ui->testSerial, false, butText);

}


void MainWindow::onScanTest() {

  if (inRun(ui->testScan)) {
    stopAll();
    return;
  }

  stopMe=false;
  const QString butText = mkRun(ui->testScan, true, "Stop");
  ui->scanWidget->setEnabled(false);
  det->waitWritten();
  HWstate hw(det, shutter);

  shutter->open();
  engineRun();

  hw.restore();
  ui->scanWidget->setEnabled(true);
  mkRun(ui->testScan, false, butText);


}




void MainWindow::onFFtest() {

  if (inRun(ui->testFF)) {
    stopAll();
    return;
  }

  stopMe=false;
  const QString butText = mkRun(ui->testFF, true, "Stop");
  ui->ffWidget->setEnabled(false);
  det->waitWritten();
  HWstate hw(det, shutter);

  acquireBG("");
  det->waitWritten();
  acquireDF("", Shutter::CLOSED);
  det->waitWritten();

  det->setAutoSave(false);
  hw.restore();
  ui->ffWidget->setEnabled(true);
  mkRun(ui->testFF, false, butText);

  QTimer::singleShot(0, this, SLOT(updateUi_bgMotor()));

}


void MainWindow::onLoopTest() {

  if (inRun(ui->testMulti)) {
    stopAll();
    return;
  }

  stopMe=false;
  const QString butText = mkRun(ui->testMulti, true, "Stop");
  ui->multiWidget->setEnabled(false);
  det->waitWritten();
  HWstate hw(det, shutter);

  shutter->open();
  acquireMulti("",  ( ui->aqMode->currentIndex() == STEPNSHOT  &&  ! ui->checkDyno->isChecked() )  ?
                      ui->aqsPP->value() : 1);
  shutter->close();
  det->waitWritten();

  det->setAutoSave(false);
  hw.restore();
  ui->multiWidget->setEnabled(true);
  mkRun(ui->testMulti, false, butText);

}

void MainWindow::onDynoTest() {

  if (inRun(ui->testDyno)) {
    stopAll();
    return;
  }

  stopMe=false;
  const QString butText = mkRun(ui->testDyno, true, "Stop");
  ui->dynoWidget->setEnabled(false);
  det->waitWritten();
  HWstate hw(det, shutter);

  shutter->open();
  acquireDyno("");
  shutter->close();
  det->waitWritten();

  det->setAutoSave(false);
  hw.restore();
  ui->dynoWidget->setEnabled(true);
  mkRun(ui->testDyno, false, butText);
  QTimer::singleShot(0, this, SLOT(updateUi_dynoMotor()));
  QTimer::singleShot(0, this, SLOT(updateUi_dyno2Motor()));

}


void MainWindow::onDetectorTest() {

  if ( ! det->isConnected()  ||  inRun(ui->testDetector) ) {
    stopAll();
    return;
  }

  stopMe=false;
  const QString butText = mkRun(ui->testDetector, true, "Stop");
  ui->detectorWidget->setEnabled(false);
  det->waitWritten();
  HWstate hw(det, shutter);

  int nofFrames = 0;
  if ( ui->aqMode->currentIndex() != STEPNSHOT )
    nofFrames = ui->nofBGs->value();
  else if ( ui->checkDyno->isChecked() )
    nofFrames = 1;
  else
    nofFrames = ui->aqsPP->value();

  shutter->open();
  acquireDetector(det->name(uiImageFormat()) + "_SAMPLE", nofFrames);
  shutter->close();
  det->waitWritten();

  det->setAutoSave(false);
  hw.restore();
  ui->detectorWidget->setEnabled(true);
  mkRun(ui->testDetector, false, butText);
  updateUi_detector();

}



void MainWindow::onStartStop() {
  if ( inRun(ui->startStop) || ui->startStop->styleSheet() == warnStyle ) {
    stopAll();
    return;
  }
  stopMe=false;
  const QString butText = mkRun(ui->startStop, true, "Stop");
  engineRun();
  mkRun(ui->startStop, false, butText);
}



void MainWindow::stopAll() {
  stopMe=true;
  emit requestToStopAcquisition();
  det->stop();
  innearList->motui->motor()->stop();
  outerList->motui->motor()->stop();
  thetaMotor->motor()->stop();
  bgMotor->motor()->stop();
  loopList->motui->motor()->stop();
  sloopList->motui->motor()->stop();
  dynoMotor->motor()->stop();
  dyno2Motor->motor()->stop();
  foreach( Script * script, findChildren<Script*>() )
    script->stop();
}












static QString combineNames(const QString & str1, const QString & str2) {
  return ( ! str1.isEmpty()  &&  ! str2.isEmpty() )
      ?  str1 + "_" + str2  :  str1 + str2 ;
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
  const QString origname = det->name(uiImageFormat());

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

    ftemplate = filetemplate.isEmpty() ? origname + "_" : filetemplate;
    if (count>1)
      ftemplate += QString("%1").arg(curr, QString::number(count).length(), 10, QChar('0'));
    // two stopMes below are reqiuired for the case it changes while the detector is being prepared
    if ( stopMe || ! prepareDetector(ftemplate) || stopMe )
      goto acquireDynoExit;

    setMotorSpeed(dynoMotor, daSpeed);
    if ( ui->dyno2->isChecked()  )
      setMotorSpeed(dyno2Motor, d2aSpeed);
    if (stopMe) goto acquireDynoExit;

    dynoMotor->motor()->goLimit( ui->dynoPos->isChecked() ? QCaMotor::POSITIVE : QCaMotor::NEGATIVE );
    if ( ui->dyno2->isChecked() )
      dyno2Motor->motor()->goLimit( ui->dyno2Pos->isChecked() ? QCaMotor::POSITIVE : QCaMotor::NEGATIVE );
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

  det->setName(uiImageFormat(), origname) ;

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


  QCaMotor * const lMotor = loopList->motui->motor();
  QCaMotor * const sMotor = sloopList->motui->motor();

  const bool
      moveLoop = lMotor->isConnected(),
      moveSubLoop = ui->subLoop->isChecked() && sMotor->isConnected();
  const QString ftemplate = filetemplate.isEmpty() ? det->name(uiImageFormat()) : filetemplate;
  const QString origname = det->name(uiImageFormat());

  const double lStart = lMotor->getUserPosition();
  const double sStart = sMotor->getUserPosition();

  const int
      totalLoops = loopList->ui->nof->value(),
      totalSubLoops = ui->subLoop->isChecked() ? sloopList->ui->nof->value() : 1,
      loopDigs = QString::number(totalLoops-1).size(),
      subLoopDigs = QString::number(totalSubLoops-1).size();

  const QString progressFormat = QString("Multishot progress: %p% ; %v of %m") +
      ( ui->subLoop->isChecked() ? QString(" (%1,%2 of %3,%4)").arg(1).arg(+1).arg(totalLoops).arg(totalSubLoops)
                                 : "" );
  ui->multiProgress->setFormat(progressFormat);
  ui->multiProgress->setMaximum(totalLoops*totalSubLoops);
  ui->multiProgress->setVisible(true);

  int currentLoop=0;

  if (moveLoop)
    lMotor->goUserPosition( loopList->ui->list->item(0, 0)->text().toDouble(), QCaMotor::STARTED);
  if (stopMe) goto acquireMultiExit;

  if (moveSubLoop)
    sMotor->goUserPosition( sloopList->ui->list->item(0, 0)->text().toDouble(), QCaMotor::STARTED);
  if (stopMe) goto acquireMultiExit;

  do { // loop

    setenv("CURRENTLOOP", QString::number(currentLoop).toLatin1(), 1);
    loopList->emphasizeRow(currentLoop);

    const QString filenameL = combineNames(ftemplate, QString("L%1").arg(currentLoop, loopDigs, 10, QChar('0')));

    if (moveLoop)
      lMotor->wait_stop();
    if (stopMe) goto acquireMultiExit;

    int currentSubLoop=0;
    do { // subloop

      setenv("CURRENTSUBLOOP", QString::number(currentSubLoop).toLatin1(), 1);
      sloopList->emphasizeRow(currentSubLoop);

      QString filename = filenameL;
      if ( totalSubLoops > 1 )
        filename = combineNames(filename, QString("S%1").arg(currentSubLoop, subLoopDigs, 10, QChar('0') ) );

      if (moveSubLoop)
        sMotor->wait_stop();
      if (stopMe) goto acquireMultiExit;

      if ( inRun(ui->startStop) )
        ui->preSubLoopScript ->script->execute();
      if (stopMe) goto acquireMultiExit;


      if ( ui->checkDyno->isChecked() )
        acquireDyno(filename, count) ;
      else
        acquireDetector(filename, count);
      if (stopMe) goto acquireMultiExit;


      currentSubLoop++;
      if (moveSubLoop && currentSubLoop < totalSubLoops)
        sMotor->goUserPosition(sloopList->ui->list->item(currentSubLoop, 0)->text().toDouble(), QCaMotor::STARTED);
      if (stopMe) goto acquireMultiExit;

    } while (currentSubLoop < totalSubLoops) ;

    currentLoop++;
    if (currentLoop < totalLoops) {
      if (moveLoop)
        lMotor->goUserPosition(loopList->ui->list->item(currentLoop, 0)->text().toDouble(), QCaMotor::STARTED);
      if (stopMe) goto acquireMultiExit;
      if (moveSubLoop)
        sMotor->goUserPosition(sloopList->ui->list->item(0, 0)->text().toDouble(), QCaMotor::STARTED);
      if (stopMe) goto acquireMultiExit;
    }

  } while (currentLoop < totalLoops);


acquireMultiExit:


  ui->multiProgress->setVisible(false);

  det->setName(uiImageFormat(), origname) ;

  if (moveLoop) {
    lMotor->goUserPosition(lStart, QCaMotor::STARTED);
    if (!stopMe)
      lMotor->wait_stop();
  }
  if (moveSubLoop) {
    sMotor->goUserPosition(sStart, QCaMotor::STARTED);
    if (!stopMe)
      sMotor->wait_stop();
  }


  loopList->emphasizeRow();
  sloopList->emphasizeRow();

  return ! stopMe;

}








int MainWindow::acquireBG(const QString &filetemplate) {

  int ret = -1;
  const int bgs = ui->nofBGs->value();
  const double bgTravel = ui->bgTravel->value();
  const double originalExposure = det->exposure();
  const QString origname = det->name(uiImageFormat());

  if ( ! bgMotor->motor()->isConnected() || bgs <1 || bgTravel == 0.0 )
    return ret;

  QString ftemplate = filetemplate.isEmpty()
      ?  origname + "_BG"  :  "BG_" + filetemplate;
  if ( uiImageFormat() != Detector::HDF && bgs > 1)
    ftemplate += "_";

  bgMotor->motor()->wait_stop();
  const double bgStart = bgMotor->motor()->getUserPosition();

  bgMotor->motor()->goUserPosition
      ( bgStart + bgTravel, QCaMotor::STOPPED );
  if (stopMe) goto onBgExit;

  det->waitWritten();
  if (stopMe) goto onBgExit;

  setenv("CONTRASTTYPE", "BG", 1);

  if ( ui->bgExposure->value() != ui->bgExposure->minimum() )
    det->setExposure( ui->bgExposure->value() ) ;

  shutter->open();
  if (stopMe) goto onBgExit;

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
  det->setName(uiImageFormat(), origname) ;

  if ( ui->bgExposure->value() != ui->bgExposure->minimum() )
    det->setExposure( originalExposure ) ;

  if (!stopMe)
    bgMotor->motor()->wait_stop();

  return ret;

}



int MainWindow::acquireDF(const QString &filetemplate, Shutter::State stateToGo) {

  int ret = -1;
  const int dfs = ui->nofDFs->value();
  const QString origname=det->name(uiImageFormat());
  QString ftemplate;

  if ( dfs<1 )
    return 0;

  shutter->close();
  if (stopMe) goto onDfExit;
  det->waitWritten();
  if (stopMe) goto onDfExit;

  ftemplate = filetemplate.isEmpty()
      ?  origname + "_DF"  :  "DF_" + filetemplate;
  if ( uiImageFormat() != Detector::HDF && dfs > 1)
    ftemplate += "_";

  setenv("CONTRASTTYPE", "DF", 1);

  det->setPeriod(0);

  ret = acquireDetector(ftemplate, dfs);
  if (stopMe) goto onDfExit;

  if (stateToGo == Shutter::OPEN)
    shutter->open();
  else if (stateToGo == Shutter::CLOSED)
    shutter->close();
  if ( ! stopMe && shutter->state() != stateToGo)
    qtWait(shutter, SIGNAL(stateUpdated(State)), 500); /**/


onDfExit:

  if (!stopMe)
    det->waitWritten();
  det->setName(uiImageFormat(), origname) ;

  return ret;

}












/*/////////////////////////// Engine ///////////////////////////*/

void MainWindow::engineRun () {


  QCaMotor * const inSMotor = innearList->motui->motor();
  QCaMotor * const outSMotor = outerList->motui->motor();

  const QString origname = det->name(uiImageFormat());
  HWstate hw(det, shutter);

  const double
      inSerialStart = inSMotor->getUserPosition(),
      outSerialStart = outSMotor->getUserPosition(),
      bgStart =  bgMotor->motor()->getUserPosition(),
      thetaStart = thetaMotor->motor()->getUserPosition(),
      thetaRange = ui->scanRange->value(),
      thetaSpeed = thetaMotor->motor()->getNormalSpeed();

  const bool
      doSerial1D = ui->checkSerial->isChecked(),
      doSerial2D = doSerial1D && ui->serial2D->isChecked(),
      doFF = inRun(ui->startStop) && ! inRun(ui->testScan) && ui->checkFF->isChecked(),
      doBG = doFF && ui->nofBGs->value(),
      doDF = doFF && ui->nofDFs->value(),
      sasMode = inRun(ui->startStop)  &&  ui->aqMode->currentIndex() == STEPNSHOT,
      ongoingSeries = doSerial1D && ui->ongoingSeries->isChecked(),
      doTriggCT = inRun(ui->startStop) && ! tct->prefix().isEmpty(),
      splitData = uiImageFormat() != Detector::HDF;

  const int
      bgInterval = ui->bgInterval->value(),
      dfInterval = ui->dfInterval->value(),
      scanDelay = QTime(0, 0, 0, 0).msecsTo( ui->scanDelay->time() ),
      scanTime = QTime(0, 0, 0, 0).msecsTo( ui->acquisitionTime->time() ),
      totalProjections = ui->scanProjections->value(),
      projectionDigs = QString::number(totalProjections).size(),
      doAdd = ui->scanAdd->isChecked() ? 1 : 0,
      totalScans1D = doSerial1D ?
        ( ui->endNumber->isChecked() ? outerList->ui->nof->value() : 0 ) : 1 ,
      totalScans2D = doSerial2D ? innearList->ui->nof->value() : 1 ,
      series1Digs = QString::number(totalScans1D-1).size(),
      series2Digs = QString::number(totalScans2D-1).size();

  int
      currentScan1D = 0,
      currentScan2D = 0,
      currentScan = 0,
      beforeBG = 0,
      beforeDF = 0;


  foreach(PositionList * lst, findChildren<PositionList*>())
    lst->freezList(true);
  if ( inRun(ui->startStop) )
    foreach(QWidget * tab, ui->control->tabs())
      tab->setEnabled(false);
  QTime inCTtime;
  inCTtime.start();
  bool timeToStop=false;

  if ( totalScans1D * totalScans2D > 1 ) {
    ui->serialProgress->setMaximum(totalScans1D * totalScans2D);
    ui->serialProgress->setFormat("Series progress. %p% (scans complete: %v of %m)");
    ui->serialProgress->setValue(currentScan);
  } else if ( ui->endTime->isChecked() ) {
    ui->serialProgress->setMaximum(QTime(0, 0, 0, 0).msecsTo( ui->acquisitionTime->time() ));
    QString format = "Series progress: %p% "
        + (QTime(0, 0, 0, 0).addMSecs(inCTtime.elapsed())).toString() + " of " + ui->acquisitionTime->time().toString()
        + " (scans complete: " + QString::number(currentScan) + ")";
    ui->serialProgress->setFormat(format);
    ui->serialProgress->setValue(inCTtime.elapsed());
  } else {
    ui->serialProgress->setMaximum(0);
    ui->serialProgress->setFormat("Series progress. Scans complete: %v.");
    ui->serialProgress->setValue(currentScan);
  }
  ui->serialProgress->setVisible(true);


  QProcess logProc(this);
  QTemporaryFile logExec(this);
  if ( inRun(ui->startStop) ) {

    QString cfgName;
    QString logName;
    int attempt=0;
    do {
      cfgName = "acquisition." + QString::number(attempt) + ".configuration";
      logName = "acquisition." + QString::number(attempt) + ".log";
      attempt++;
    } while ( QFile::exists(cfgName) || QFile::exists(logName) ) ;

    if ( attempt > 1  &&  QMessageBox::No == QMessageBox::question
         (this, "Overwrite warning"
          , "Current directory seems to contain earlier scans: configuration and/or log files are present.\n\n"
            " Existing data may be overwritten.\n"
            " Do you want to proceed?\n"
          , QMessageBox::Yes | QMessageBox::No, QMessageBox::No)  )
      goto onEngineExit;

    saveConfiguration(cfgName);

    logProc.setWorkingDirectory(QDir::currentPath());
    if ( ! logExec.open() )
      qDebug() << "ERROR! Unable to open temporary file for log proccess.";
    logExec.write( (  QCoreApplication::applicationDirPath().remove( QRegExp("bin/*$") )
                      + "/libexec/ctgui.log.sh"
                      + " " + det->pv()
                      + " " + thetaMotor->motor()->getPv()
                      + " >> " + logName ).toLatin1() );
    logExec.flush();
    logProc.start( "/bin/sh " + logExec.fileName() );

  }


  if ( doSerial1D  &&  ui->endNumber->isChecked() && outSMotor->isConnected() )
    outSMotor->goUserPosition( outerList->ui->list->item(0, 0)->text().toDouble(), QCaMotor::STARTED);
  if (stopMe) goto onEngineExit;

  if ( doSerial2D && inSMotor->isConnected() )
    inSMotor->goUserPosition( innearList->ui->list->item(0, 0)->text().toDouble(), QCaMotor::STARTED);
  if (stopMe) goto onEngineExit;


  if (doTriggCT) {
    tct->setStartPosition(thetaStart, true);
    //tct->setStep( thetaRange / ui->scanProjections->value(), true);
    tct->setRange( thetaRange , true);
    qtWait(500); // otherwise NofTrigs are set, but step is not recalculated ...((    int digs=0;
    tct->setNofTrigs(totalProjections + doAdd, true);
    if (stopMe) goto onEngineExit;
  }

  if ( inRun(ui->startStop) )
    ui->preRunScript->script->execute();
  if (stopMe) goto onEngineExit;





  do { // serial scanning 1D

    setenv("CURRENTOSCAN", QString::number(currentScan1D).toLatin1(), 1);
    outerList->emphasizeRow(currentScan1D);

    QString sn1, sn2;
    if ( ! totalScans1D )
      sn1 = "Y" + QString::number(currentScan1D);
    else if (totalScans1D > 1)
      sn1 = QString("Y%1").arg(currentScan1D, series1Digs, 10, QChar('0') );
    else
      sn1 = QString();

    if (doSerial1D  && outSMotor->isConnected())
      outSMotor->wait_stop();
    if (stopMe) goto onEngineExit;

    currentScan2D=0;
    if(doSerial2D)
      ui->pre2DScript->script->execute();
    if (stopMe) goto onEngineExit;





    do { // serial scanning 2D

      sn2 = (totalScans2D <= 1)  ?
            sn1  :  combineNames(sn1, QString("Z%1").arg(currentScan2D, series2Digs, 10, QChar('0') ) );

      QString sampleName;
      if ( inRun(ui->testScan) )
        sampleName = origname + "_SAMPLE";
      else if ( inRun(ui->testSerial) )
        sampleName = combineNames( origname + "_SAMPLE", sn2);
      else
        sampleName = combineNames("SAMPLE", sn2);

      setenv("CURRENTISCAN", QString::number(currentScan2D).toLatin1(), 1);
      setenv("CURRENTSCAN", QString::number(currentScan).toLatin1(), 1);
      innearList->emphasizeRow(currentScan2D);

      if (doSerial2D  && inSMotor->isConnected())
        inSMotor->wait_stop();
      if (stopMe) goto onEngineExit;

      if ( inRun(ui->startStop) )
        ui->preScanScript->script->execute();
      if (stopMe) goto onEngineExit;





      if ( inRun(ui->testSerial) ) { // test series




        const QString projectionName = sampleName + ( splitData
                                 ? QString("_T%1").arg(0, projectionDigs, 10, QChar('0') )
                                 :  QString() );
        acquireDetector(projectionName, 1);
        if (stopMe) goto onEngineExit;





      } else if ( sasMode ) { // STEP-AND-SHOT





        int currentProjection = 0;
        if ( ! ongoingSeries ) {
          beforeBG=0;
          beforeDF=0;
        }

        ui->scanProgress->setMaximum(totalProjections + ( ui->scanAdd->isChecked() ? 1 : 0 ));
        ui->scanProgress->setValue(currentProjection);
        ui->scanProgress->setVisible(true);


        do {

          setenv("CURRENTPROJECTION", QString::number(currentProjection).toLatin1(), 1);

          QString projectionName = sampleName + QString("T%1").arg(currentProjection, projectionDigs, 10, QChar('0') );

          QString bgdfN = combineNames(sn2, "BEFORE");
          if (doDF && ! beforeDF) {
            acquireDF(bgdfN, Shutter::OPEN);
            beforeDF = dfInterval;
            if (stopMe) goto onEngineExit;
          }
          beforeDF--;
          if (doBG && ! beforeBG) {
            acquireBG(bgdfN);
            beforeBG = bgInterval;
            if (stopMe) goto onEngineExit;
          }
          beforeBG--;

          thetaMotor->motor()->wait_stop();
          if (stopMe) goto onEngineExit;

          det->waitWritten();
          setenv("CONTRASTTYPE", "SAMPLE", 1);
          if (stopMe) goto onEngineExit;

          if (ui->checkMulti->isChecked())
            acquireMulti(projectionName, ui->aqsPP->value());
          else if (ui->checkDyno->isChecked())
            acquireDyno(projectionName);
          else
            acquireDetector(projectionName, ui->aqsPP->value());
          if (stopMe) goto onEngineExit;

          currentProjection++;

          double motorTarget = thetaStart + thetaRange * currentProjection / totalProjections;
          if (currentProjection < totalProjections + doAdd)
            thetaMotor->motor()->goUserPosition( motorTarget + ( ongoingSeries ? thetaRange * currentScan : 0) , QCaMotor::STARTED );
          if (stopMe) goto onEngineExit;

          ui->scanProgress->setValue(currentProjection);


        } while ( currentProjection < totalProjections + doAdd );



        QString bgdfN = combineNames(sn2, "AFTER");
        if (doBG && ! beforeBG) {
          acquireBG(bgdfN);
          if (stopMe) goto onEngineExit;
          beforeBG = bgInterval;
        }
        if ( doDF && ! beforeDF ) {
          acquireDF(bgdfN, Shutter::CLOSED);
          if (stopMe) goto onEngineExit;
          beforeDF = dfInterval;
        }





      } else { // CONTINIOUS




        QString bgdfN = combineNames(sn2, "BEFORE");
        if ( doDF && ui->dfIntervalBefore->isChecked() && (ui->ffOnEachScan->isChecked() || ! currentScan ) ) {
          acquireDF(bgdfN, Shutter::OPEN);
          if (stopMe) goto onEngineExit;
        }
        if ( doBG  && ui->bgIntervalBefore->isChecked() && (ui->ffOnEachScan->isChecked() || ! currentScan ) ) {
          acquireBG(bgdfN);
          if (stopMe) goto onEngineExit;
        }

        const double
            speed = thetaRange * ui->flyRatio->value() / ( det->exposure() * totalProjections ),
            accTime = thetaMotor->motor()->getAcceleration(),
            accTravel = speed * accTime / 2,
            rotDir = copysign(1,thetaRange),
            addTravel = 2*rotDir*accTravel + (doTriggCT ? 2.0 : 0.0);
        // coefficient 2 in the above string is required to guarantee that the
        // motor has accelerated to the normal speed before the acquisition starts.


        det->waitWritten();
        if (stopMe) goto onEngineExit;

        prepareDetector(sampleName + (splitData ? "_T" : ""), totalProjections + doAdd);
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
                goLimit( thetaRange > 0 ? QCaMotor::POSITIVE : QCaMotor::NEGATIVE );
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
          thetaMotor->motor()->goLimit( thetaRange > 0 ? QCaMotor::POSITIVE : QCaMotor::NEGATIVE );
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
          bgdfN = combineNames(sn2, "AFTER");
          if ( doBG  && ui->bgIntervalAfter->isChecked() && ui->ffOnEachScan->isChecked() )
            acquireBG(bgdfN);
          if (stopMe) goto onEngineExit;
          if ( doDF && ui->dfIntervalAfter->isChecked() && ui->ffOnEachScan->isChecked() )
            acquireDF(bgdfN, Shutter::CLOSED);
          if (stopMe) goto onEngineExit;
        }




      } // Cont / SaS / Test





      if ( inRun(ui->startStop) )
        ui->postScanScript->script->execute();
      if (stopMe) goto onEngineExit;

      if ( ! ongoingSeries ) {
        thetaMotor->motor()->stop(QCaMotor::STOPPED);
        if (stopMe) goto onEngineExit;
        setMotorSpeed(thetaMotor, thetaSpeed);
        thetaMotor->motor()->goUserPosition( thetaStart, QCaMotor::STARTED );
        if (stopMe) goto onEngineExit;
      }

      currentScan2D++;
      currentScan++;

      if ( ui->endTime->isChecked() )
        ui->serialProgress->setValue(inCTtime.elapsed());
      else
        ui->serialProgress->setValue(currentScan);

      timeToStop = inRun(ui->testScan)
          || currentScan2D >= totalScans2D
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
        ! doSerial1D || inRun(ui->testScan)
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
      QString bgdfN = combineNames(sn2, "AFTER");
      if ( doBG && ui->bgIntervalAfter->isChecked() ) {
        acquireBG(bgdfN);
        if (stopMe) goto onEngineExit;
      }
      if ( doDF && ui->dfIntervalAfter->isChecked() ) {
        acquireDF(bgdfN, Shutter::CLOSED);
        if (stopMe) goto onEngineExit;
      }
    }




  } while ( ! timeToStop );
  if (stopMe) goto onEngineExit;


  if ( inRun(ui->startStop) )
    ui->postRunScript->script->execute();


onEngineExit:

  ui->scanProgress->setVisible(false);
  ui->serialProgress->setVisible(false);

  shutter->close();

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
  det->setAutoSave(false);
  hw.restore();

  if ( inRun(ui->startStop) ) {
    QProcess::execute("killall -9 camonitor");
    //QProcess::execute(  QString("kill $( pgrep -l -P %1 | grep camonitor | cut -d' '  -f 1 )").arg( int(logProc.pid())) );
    logProc.terminate();
    logExec.close();
  }

  QTimer::singleShot(0, this, SLOT(updateUi_thetaMotor()));
  QTimer::singleShot(0, this, SLOT(updateUi_bgMotor()));
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
  outerList->emphasizeRow();
  innearList->emphasizeRow();

}



