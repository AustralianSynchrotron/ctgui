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



const QString MainWindow::storedState=QDir::homePath()+"/.ctgui";

MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  isLoadingState(false),
  ui(new Ui::MainWindow),
  det(new Detector(this)),
  inAcquisitionTest(false),
  inDynoTest(false),
  inMultiTest(false),
  inFFTest(false),
  inCT(false),
  readyToStartCT(false),
  stopMe(true),
  totalScans(-1),
  currentScan(-1),
  totalProjections(-1),
  currentProjection(-1),
  totalLoops(-1),
  currentLoop(-1),
  totalSubLoops(-1),
  currentSubLoop(-1),
  totalShots(-1),
  currentShot(-1),
  logFile(0)
{

  ui->setupUi(this);
  ui->control->finilize();
  ui->control->setCurrentIndex(0);


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

  serialMotor = new QCaMotorGUI;
  ui->placeSerialMotor->layout()->addWidget(serialMotor->setupButton());
  thetaMotor = new QCaMotorGUI;
  ui->placeThetaMotor->layout()->addWidget(thetaMotor->setupButton());
  bgMotor = new QCaMotorGUI;
  ui->placeBGmotor->layout()->addWidget(bgMotor->setupButton());
  loopMotor = new QCaMotorGUI;
  ui->placeLoopMotor->layout()->addWidget(loopMotor->setupButton());
  subLoopMotor = new QCaMotorGUI;
  ui->placeSubLoopMotor->layout()->addWidget(subLoopMotor->setupButton());
  dynoMotor = new QCaMotorGUI;
  ui->placeDynoMotor->layout()->addWidget(dynoMotor->setupButton());
  dyno2Motor = new QCaMotorGUI;
  ui->placeDyno2Motor->layout()->addWidget(dyno2Motor->setupButton());

  sh1A = new Shutter1A(ui->tabFF);

  tct = new TriggCT(this);
  tct->setPrefix("INTEG01:EQU");

  updateUi_expPath();
  updateUi_pathSync();
  updateUi_checkDyno();
  updateUi_checkMulti();
  updateUi_nofScans();
  updateUi_acquisitionTime();
  updateUi_condtitionScript();
  updateUi_serialStep();
  updateUi_serialMotor();
  updateUi_serialList();
  updateUi_ffOnEachScan();
  updateUi_scanRange();
  updateUi_aqsPP();
  updateUi_scanStep();
  updateUi_rotSpeed();
  updateUi_stepTime();
  updateUi_expOverStep();
  updateUi_thetaMotor();
  updateUi_bgTravel();
  updateUi_bgInterval();
  updateUi_dfInterval();
  updateUi_bgMotor();
  updateUi_shutterStatus();
  updateUi_loopStep();
  updateUi_loopMotor();
  updateUi_subLoopStep();
  updateUi_subLoopMotor();
  updateUi_dynoSpeed();
  updateUi_dynoMotor();
  updateUi_dyno2Speed();
  updateUi_dyno2Motor();
  updateUi_detector();
  updateUi_triggCT();


  connect(ui->browseExpPath, SIGNAL(clicked()), SLOT(onWorkingDirBrowse()));
  connect(ui->continiousMode, SIGNAL(toggled(bool)), SLOT(onAcquisitionMode()));
  connect(ui->stepAndShotMode, SIGNAL(toggled(bool)), SLOT(onAcquisitionMode()));
  connect(ui->checkSerial, SIGNAL(toggled(bool)), SLOT(onSerialCheck()));
  connect(ui->checkFF, SIGNAL(toggled(bool)), SLOT(onFFcheck()));
  connect(ui->checkDyno, SIGNAL(toggled(bool)), SLOT(onDynoCheck()));
  connect(ui->checkMulti, SIGNAL(toggled(bool)), SLOT(onMultiCheck()));
  connect(ui->testFF, SIGNAL(clicked()), SLOT(onFFtest()));
  connect(ui->testMulti, SIGNAL(clicked()), SLOT(onLoopTest()));
  connect(ui->subLoop, SIGNAL(toggled(bool)), SLOT(onSubLoop()));
  connect(ui->testDyno, SIGNAL(clicked()), SLOT(onDynoTest()));
  connect(ui->dynoDirectionLock, SIGNAL(toggled(bool)), SLOT(onDynoDirectionLock()));
  connect(ui->dynoSpeedLock, SIGNAL(toggled(bool)), SLOT(onDynoSpeedLock()));
  connect(ui->dyno2, SIGNAL(toggled(bool)), SLOT(onDyno2()));
  connect(ui->testDetector, SIGNAL(clicked()), SLOT(onDetectorTest()));
  connect(ui->startStop, SIGNAL(clicked()), SLOT(onStartStop()));

  onAcquisitionMode();
  onSerialCheck();
  onFFcheck();
  onDynoCheck();
  onMultiCheck();
  onDyno2();
  onSubLoop();
  onDetectorSelection();

  loadConfiguration(storedState);
  connect( ui->expName, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->expDesc, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->sampleDesc, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->expPath, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->aqModeButtonGroup, SIGNAL(buttonClicked(int)), SLOT(storeCurrentState()));
  connect( ui->triggCT, SIGNAL(toggled(bool)), SLOT(storeCurrentState()));
  connect( ui->checkSerial, SIGNAL(toggled(bool)), SLOT(storeCurrentState()));
  connect( ui->checkFF, SIGNAL(toggled(bool)), SLOT(storeCurrentState()));
  connect( ui->checkDyno, SIGNAL(toggled(bool)), SLOT(storeCurrentState()));
  connect( ui->checkMulti, SIGNAL(toggled(bool)), SLOT(storeCurrentState()));
  connect( ui->postRunScript, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->endConditionButtonGroup, SIGNAL(buttonClicked(int)), SLOT(storeCurrentState()));
  connect( ui->nofScans, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->acquisitionTime, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->conditionScript, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->ongoingSeries, SIGNAL(toggled(bool)), SLOT(storeCurrentState()));
  connect( ui->ffOnEachScan, SIGNAL(toggled(bool)), SLOT(storeCurrentState()));
  connect( ui->scanDelay, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( serialMotor->motor(), SIGNAL(changedPv()), SLOT(storeCurrentState()));
  connect( ui->serialStep, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->irregularStep, SIGNAL(toggled(bool)), SLOT(storeCurrentState()));
  connect( ui->serialPositionsList, SIGNAL(itemChanged(QTableWidgetItem*)), SLOT(storeCurrentState()));
  connect( thetaMotor->motor(), SIGNAL(changedPv()), SLOT(storeCurrentState()));
  connect( ui->scanRange, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->scanProjections, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->aqsPP, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->rotSpeed, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->scanAdd, SIGNAL(toggled(bool)), SLOT(storeCurrentState()));
  connect( ui->preScanScript, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->postScanScript, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->nofBGs, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->bgInterval, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->bgIntervalBefore, SIGNAL(toggled(bool)), SLOT(storeCurrentState()));
  connect( ui->bgIntervalAfter, SIGNAL(toggled(bool)), SLOT(storeCurrentState()));
  connect( bgMotor->motor(), SIGNAL(changedPv()), SLOT(storeCurrentState()));
  connect( ui->bgTravel, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->nofDFs, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->dfInterval, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->dfIntervalBefore, SIGNAL(toggled(bool)), SLOT(storeCurrentState()));
  connect( ui->dfIntervalAfter, SIGNAL(toggled(bool)), SLOT(storeCurrentState()));
  connect( ui->singleBg, SIGNAL(toggled(bool)), SLOT(storeCurrentState()));
  connect( loopMotor->motor(), SIGNAL(changedPv()), SLOT(storeCurrentState()));
  connect( ui->loopNumber, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->loopStep, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->preLoopScript, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->postLoopScript, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->subLoop, SIGNAL(toggled(bool)), SLOT(storeCurrentState()));
  connect( subLoopMotor->motor(), SIGNAL(changedPv()), SLOT(storeCurrentState()));
  connect( ui->subLoopNumber, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->subLoopStep, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->preSubLoopScript, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->postSubLoopScript, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( dynoMotor->motor(), SIGNAL(changedPv()), SLOT(storeCurrentState()));
  connect( ui->dynoSpeed, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->dynoDirButtonGroup, SIGNAL(buttonClicked(int)), SLOT(storeCurrentState()));
  connect( ui->dyno2, SIGNAL(toggled(bool)), SLOT(storeCurrentState()));
  connect( dyno2Motor->motor(), SIGNAL(changedPv()), SLOT(storeCurrentState()));
  connect( ui->dynoSpeedLock, SIGNAL(toggled(bool)), SLOT(storeCurrentState()));
  connect( ui->dyno2Speed, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->dynoDirectionLock, SIGNAL(toggled(bool)), SLOT(storeCurrentState()));
  connect( ui->dyno2DirButtonGroup, SIGNAL(buttonClicked(int)), SLOT(storeCurrentState()));
  connect( ui->detSelection, SIGNAL(currentIndexChanged(int)), SLOT(storeCurrentState()));
  connect( ui->preAqScript, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->postAqScript, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->detPathSync, SIGNAL(toggled(bool)), SLOT(storeCurrentState()));


  updateProgress();


}


MainWindow::~MainWindow()
{
  delete ui;
}





static void setInConfig(QSettings & config, const QString & key, QWidget * wdg) {
  if (key.isEmpty())
    return;
  if (dynamic_cast<QLineEdit*>(wdg))
    config.setValue(key, dynamic_cast<QLineEdit*>(wdg)->text());
  else if (dynamic_cast<QPlainTextEdit*>(wdg))
    config.setValue(key, dynamic_cast<QPlainTextEdit*>(wdg)->toPlainText());
  else if (dynamic_cast<QAbstractButton*>(wdg))
    config.setValue(key, dynamic_cast<QAbstractButton*>(wdg)->isChecked());
  else if (dynamic_cast<Script*>(wdg))
    config.setValue(key, dynamic_cast<Script*>(wdg)->path());
  else if (dynamic_cast<QComboBox*>(wdg))
    config.setValue(key, dynamic_cast<QComboBox*>(wdg)->currentText());
  else if (dynamic_cast<QSpinBox*>(wdg))
    config.setValue(key, dynamic_cast<QSpinBox*>(wdg)->value());
  else if (dynamic_cast<QDoubleSpinBox*>(wdg))
    config.setValue(key, dynamic_cast<QDoubleSpinBox*>(wdg)->value());
  else if (dynamic_cast<QTimeEdit*>(wdg))
    config.setValue(key, dynamic_cast<QTimeEdit*>(wdg)->time());
  else if (dynamic_cast<QCaMotorGUI*>(wdg))
    config.setValue(key, dynamic_cast<QCaMotorGUI*>(wdg)->motor()->getPv());
  else
    qDebug() << "Cannot save the value of widget" << wdg << "into config";
}

static void restoreFromConfig(QSettings & config, const QString & key, QWidget * wdg) {
  if ( ! config.contains(key) ) {
    qDebug() << "Config does not contain key" << key;
    return;
  }
  QVariant value=config.value(key);
  if ( ! value.isValid() ) {
    qDebug() << "Value read from config key" << key << "is not valid";
    return;
  }

  if ( dynamic_cast<QLineEdit*>(wdg) && value.canConvert(QVariant::String) )
    dynamic_cast<QLineEdit*>(wdg)->setText(value.toString());
  else if ( dynamic_cast<QPlainTextEdit*>(wdg) && value.canConvert(QVariant::String) )
    dynamic_cast<QPlainTextEdit*>(wdg)->setPlainText(value.toString());
  else if ( dynamic_cast<QAbstractButton*>(wdg) && value.canConvert(QVariant::Bool) )
    dynamic_cast<QAbstractButton*>(wdg)->setChecked(value.toBool());
  else if ( dynamic_cast<Script*>(wdg) && value.canConvert(QVariant::String) )
    dynamic_cast<Script*>(wdg)->setPath(value.toString());
  else if ( dynamic_cast<QComboBox*>(wdg) && value.canConvert(QVariant::String) ) {
    const QString val = value.toString();
    QComboBox * box = dynamic_cast<QComboBox*>(wdg);
    const int idx = box->findText(val);
    if ( idx >= 0 )  box->setCurrentIndex(idx);
    else             box->setEditText(val);
  }
  else if ( dynamic_cast<QSpinBox*>(wdg) && value.canConvert(QVariant::Int) )
    dynamic_cast<QSpinBox*>(wdg)->setValue(value.toInt());
  else if ( dynamic_cast<QDoubleSpinBox*>(wdg) && value.canConvert(QVariant::Double) )
    dynamic_cast<QDoubleSpinBox*>(wdg)->setValue(value.toDouble());
  else if ( dynamic_cast<QTimeEdit*>(wdg) && value.canConvert(QVariant::Time) )
    dynamic_cast<QTimeEdit*>(wdg)->setTime(value.toTime());
  else if ( dynamic_cast<QCaMotorGUI*>(wdg) && value.canConvert(QVariant::String) )
    dynamic_cast<QCaMotorGUI*>(wdg)->motor()->setPv(value.toString());
  else
    qDebug() << "Cannot restore the value of widget" << wdg
             << "from value" << value << "in config.";
}


void MainWindow::saveConfiguration(QString fileName) {

  if ( fileName.isEmpty() )
    fileName = QFileDialog::getSaveFileName(0, "Save configuration", QDir::currentPath());

  QSettings config(fileName, QSettings::IniFormat);


  setInConfig(config, "title", ui->expName);
  setInConfig(config, "description", ui->expDesc);
  setInConfig(config, "sample", ui->sampleDesc);
  setInConfig(config, "workingdir", ui->expPath);
  setInConfig(config, "syncdetectordir", ui->detPathSync);
  config.setValue("acquisitionmode", ui->stepAndShotMode->isChecked() ?
                    "step-and-shot" : "continious");
  setInConfig(config, "hwtrigg", ui->triggCT);
  setInConfig(config, "doserialscans", ui->checkSerial);
  setInConfig(config, "doflatfield", ui->checkFF);
  setInConfig(config, "dodyno", ui->checkDyno);
  setInConfig(config, "domulti", ui->checkMulti);
  // setInConfig(config, "sampleFile", ui->sampleFile);
  // setInConfig(config, "backgroundFile", ui->bgFile);
  // setInConfig(config, "darkfieldFile", ui->dfFile);
  setInConfig(config, "prerun", ui->preRunScript);
  setInConfig(config, "postrun", ui->postRunScript);

  if (ui->checkSerial->isChecked()) {
    config.beginGroup("serial");
    if (ui->endNumber->isChecked())
      config.setValue("endCondition", "number");
    else if (ui->endTime->isChecked())
      config.setValue("endCondition", "time");
    else if (ui->endCondition->isChecked())
      config.setValue("endCondition", "script");
    setInConfig(config, "number", ui->nofScans);
    setInConfig(config, "time", ui->acquisitionTime);
    setInConfig(config, "script", ui->conditionScript);
    setInConfig(config, "ongoingseries",ui->ongoingSeries);
    setInConfig(config, "ffoneachscan",ui->ffOnEachScan);
    setInConfig(config, "scandelay",ui->scanDelay);
    setInConfig(config, "serialmotor",serialMotor);
    setInConfig(config, "serialstep",ui->serialStep);
    setInConfig(config, "serialirregularstep",ui->irregularStep);
    if ( ui->irregularStep->isChecked() ) {
      config.beginWriteArray("irregularsteps");
      int index = 0;
      foreach (QTableWidgetItem * item,
               ui->serialPositionsList->findItems("", Qt::MatchContains) ) {
        config.setArrayIndex(index++);
        config.setValue("position", item ? item->text() : "");
      }
      config.endArray();
    }
    config.endGroup();
  }

  config.beginGroup("scan");
  setInConfig(config, "motor", thetaMotor);
  setInConfig(config, "range", ui->scanRange);
  setInConfig(config, "steps", ui->scanProjections);
  setInConfig(config, "aquisitionsperprojection", ui->aqsPP);
  setInConfig(config, "add", ui->scanAdd);
  setInConfig(config, "prescan", ui->preScanScript);
  setInConfig(config, "postscan", ui->postScanScript);
  setInConfig(config, "rotationspeed", ui->rotSpeed);
  config.endGroup();

  if (ui->checkFF->isChecked()) {
    config.beginGroup("flatfield");
    setInConfig(config, "bgs", ui->nofBGs);
    setInConfig(config, "bginterval", ui->bgInterval);
    setInConfig(config, "bgBefore", ui->bgIntervalBefore);
    setInConfig(config, "bgAfter", ui->bgIntervalAfter);
    setInConfig(config, "motor", bgMotor);
    setInConfig(config, "bgtravel", ui->bgTravel);
    setInConfig(config, "dfs", ui->nofDFs);
    setInConfig(config, "dfinterval", ui->dfInterval);
    setInConfig(config, "dfBefore", ui->dfIntervalBefore);
    setInConfig(config, "dfAfter", ui->dfIntervalAfter);
    config.endGroup();
  }

  if (ui->checkMulti->isChecked()) {
    config.beginGroup("loop");
    setInConfig(config, "singlebg", ui->singleBg);
    setInConfig(config, "motor", loopMotor);
    setInConfig(config, "shots", ui->loopNumber);
    setInConfig(config, "step", ui->loopStep);
    setInConfig(config, "preloop", ui->preLoopScript);
    setInConfig(config, "postloop", ui->postLoopScript);
    setInConfig(config, "subloop", ui->subLoop);
    if (ui->subLoop->isChecked()) {
      config.beginGroup("subloop");
      setInConfig(config, "motor", subLoopMotor);
      setInConfig(config, "shots", ui->subLoopNumber);
      setInConfig(config, "step", ui->subLoopStep);
      setInConfig(config, "presubloop", ui->preSubLoopScript);
      setInConfig(config, "postsubloop", ui->postSubLoopScript);
      config.endGroup();
    }
    config.endGroup();
  }

  if (ui->checkDyno->isChecked()) {
    config.beginGroup("dyno");
    setInConfig(config, "motor", dynoMotor);
    setInConfig(config, "speed", ui->dynoSpeed);
    config.setValue("direction", ui->dynoPos->isChecked() ?
                      "positive" : "negative");
    setInConfig(config, "dyno2", ui->dyno2);
    if (ui->dyno2->isChecked()) {
      config.beginGroup("dyno2");
      setInConfig(config, "motor", dyno2Motor);
      setInConfig(config, "speedLock", ui->dynoSpeedLock);
      setInConfig(config, "speed", ui->dyno2Speed);
      setInConfig(config, "dirLock", ui->dynoDirectionLock);
      config.setValue("direction", ui->dyno2Pos->isChecked() ?
                        "positive" : "negative");
      config.endGroup();
    }
    config.endGroup();
  }

  setInConfig(config, "detector", ui->detSelection);
  setInConfig(config, "preacquire", ui->preAqScript);
  setInConfig(config, "postacquire", ui->postAqScript);

}


void MainWindow::loadConfiguration(QString fileName) {

  if ( fileName.isEmpty() )
    fileName = QFileDialog::getOpenFileName(0, "Load configuration", QDir::currentPath());

  if ( ! QFile::exists(fileName) ) {
    appendMessage(ERROR, "Configuration file \"" + fileName + "\" does not exist.");
    return;
  }
  isLoadingState=true;
  QSettings config(fileName, QSettings::IniFormat);

  QStringList groups = config.childGroups();

  restoreFromConfig(config, "title", ui->expName);
  restoreFromConfig(config, "description", ui->expDesc);
  restoreFromConfig(config, "sample", ui->sampleDesc);
  restoreFromConfig(config, "workingdir", ui->expPath);
  restoreFromConfig(config, "syncdetectordir", ui->detPathSync);
  if (config.contains("acquisitionmode")) {
    if ( config.value("acquisitionmode").toString() == "step-and-shot" )
      ui->stepAndShotMode->setChecked(true);
    else if ( config.value("acquisitionmode").toString() == "continious" )
      ui->continiousMode->setChecked(true);
  }
  restoreFromConfig(config, "hwtrigg", ui->triggCT);
  restoreFromConfig(config, "doserialscans", ui->checkSerial);
  restoreFromConfig(config, "doflatfield", ui->checkFF);
  restoreFromConfig(config, "dodyno", ui->checkDyno);
  restoreFromConfig(config, "domulti", ui->checkMulti);
  restoreFromConfig(config, "prerun", ui->preRunScript);
  restoreFromConfig(config, "postrun", ui->postRunScript);

  if (ui->checkSerial->isChecked() && groups.contains("serial") ) {
    config.beginGroup("serial");
    if (config.contains("endCondition")) {
      const QString endCondition = config.value("endCondition").toString();
      if( endCondition == "number" )
        ui->endNumber->setChecked(true);
      else if( endCondition == "time" )
        ui->endTime->setChecked(true);
      else if( endCondition == "script" )
        ui->endCondition->setChecked(true);
    }
    restoreFromConfig(config, "number", ui->nofScans);
    restoreFromConfig(config, "time", ui->acquisitionTime);
    restoreFromConfig(config, "script", ui->conditionScript);
    restoreFromConfig(config, "ongoingseries",ui->ongoingSeries);
    restoreFromConfig(config, "ffoneachscan",ui->ffOnEachScan);
    restoreFromConfig(config, "scandelay",ui->scanDelay);
    restoreFromConfig(config, "serialmotor",serialMotor);
    restoreFromConfig(config, "serialstep",ui->serialStep);
    restoreFromConfig(config, "serialirregularstep",ui->irregularStep);
    if ( ui->irregularStep->isChecked() ) {
      int stepssize = config.beginReadArray("irregularsteps");
      for (int i = 0; i < stepssize; ++i) {
        config.setArrayIndex(i);
        if ( ui->serialPositionsList->item(i, 0) )
          ui->serialPositionsList->item(i,0)->setText
              (config.value("position").toString());
      }
      config.endArray();
    }


    config.endGroup();
  }

  if (groups.contains("scan")) {
    config.beginGroup("scan");
    restoreFromConfig(config, "motor", thetaMotor);
    restoreFromConfig(config, "range", ui->scanRange);
    restoreFromConfig(config, "steps", ui->scanProjections);
    restoreFromConfig(config, "aquisitionsperprojection", ui->aqsPP);
    restoreFromConfig(config, "add", ui->scanAdd);
    restoreFromConfig(config, "prescan", ui->preScanScript);
    restoreFromConfig(config, "postscan", ui->postScanScript);
    restoreFromConfig(config, "rotationspeed", ui->rotSpeed);
    config.endGroup();
  }


  if (ui->checkFF->isChecked() && groups.contains("flatfield")) {
    config.beginGroup("flatfield");
    restoreFromConfig(config, "bgs", ui->nofBGs);
    restoreFromConfig(config, "bginterval", ui->bgInterval);
    restoreFromConfig(config, "bgBefore", ui->bgIntervalBefore);
    restoreFromConfig(config, "bgAfter", ui->bgIntervalAfter);
    restoreFromConfig(config, "motor", bgMotor);
    restoreFromConfig(config, "bgtravel", ui->bgTravel);
    restoreFromConfig(config, "dfs", ui->nofDFs);
    restoreFromConfig(config, "dfinterval", ui->dfInterval);
    restoreFromConfig(config, "dfBefore", ui->dfIntervalBefore);
    restoreFromConfig(config, "dfAfter", ui->dfIntervalAfter);
    config.endGroup();
  }

  if (ui->checkMulti->isChecked() && groups.contains("loop")) {
    config.beginGroup("loop");
    restoreFromConfig(config, "singlebg", ui->singleBg);
    restoreFromConfig(config, "motor", loopMotor);
    restoreFromConfig(config, "shots", ui->loopNumber);
    restoreFromConfig(config, "step", ui->loopStep);
    restoreFromConfig(config, "preloop", ui->preLoopScript);
    restoreFromConfig(config, "postloop", ui->postLoopScript);
    restoreFromConfig(config, "subloop", ui->subLoop);
    if (ui->subLoop->isChecked()) {
      config.beginGroup("subloop");
      restoreFromConfig(config, "motor", subLoopMotor);
      restoreFromConfig(config, "shots", ui->subLoopNumber);
      restoreFromConfig(config, "step", ui->subLoopStep);
      restoreFromConfig(config, "presubloop", ui->preSubLoopScript);
      restoreFromConfig(config, "postsubloop", ui->postSubLoopScript);
      config.endGroup();
    }
    config.endGroup();
  }

  if (ui->checkDyno->isChecked() && groups.contains("dyno")) {
    config.beginGroup("dyno");
    restoreFromConfig(config, "motor", dynoMotor);
    restoreFromConfig(config, "speed", ui->dynoSpeed);
    if ( config.contains("direction") &&
         config.value("direction").canConvert(QVariant::String) ) {
      if (config.value("direction").toString() == "positive")
        ui->dynoPos->setChecked(true);
      else if (config.value("direction").toString() == "negative")
        ui->dynoNeg->setChecked(true);
    }
    restoreFromConfig(config, "dyno2", ui->dyno2);
    if (ui->dyno2->isChecked()) {
      config.beginGroup("dyno2");
      restoreFromConfig(config, "motor", dyno2Motor);
      restoreFromConfig(config, "speedLock", ui->dynoSpeedLock);
      if ( ! ui->dynoSpeedLock->isChecked() )
        restoreFromConfig(config, "speed", ui->dyno2Speed);
      restoreFromConfig(config, "dirLock", ui->dynoDirectionLock);
      if ( ! ui->dynoDirectionLock->isChecked() &&
           config.contains("direction") &&
           config.value("direction").canConvert(QVariant::String) ) {
        if (config.value("direction").toString() == "positive")
          ui->dyno2Pos->setChecked(true);
        else if (config.value("direction").toString() == "negative")
          ui->dyno2Neg->setChecked(true);
      }
      config.endGroup();
    }
    config.endGroup();
  }

  restoreFromConfig(config, "detector", ui->detSelection);
  restoreFromConfig(config, "preacquire", ui->preAqScript);
  restoreFromConfig(config, "postacquire", ui->postAqScript);


  if ( ui->expPath->text().isEmpty()  || fileName.isEmpty() )
    ui->expPath->setText( QFileInfo(fileName).absolutePath());

  isLoadingState=false;

}


void MainWindow::storeCurrentState() {
  if (!isLoadingState)
    saveConfiguration(storedState);
}


void MainWindow::addMessage(const QString & str) {
  ui->messages->append(
        QDateTime::currentDateTime().toString("dd/MM/yyyy_hh:mm:ss.zzz") +
        " " + str);
}

static QString lastPathComponent(const QString & pth) {
  QString lastComponent = pth;
  if ( lastComponent.endsWith("/") )
    lastComponent.chop(1);
  if (lastComponent.contains('/'))
    lastComponent.remove(0, lastComponent.lastIndexOf('/')+1);
  return lastComponent;
}

void MainWindow::updateUi_expPath() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_expPath());
    connect( ui->expPath, SIGNAL(textChanged(QString)), SLOT(updateUi_expPath()) );
    connect( ui->detPathSync, SIGNAL(toggled(bool)), thisSlot );
    connect( det, SIGNAL(pathChanged(QString)), thisSlot);
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
    ui->nonEmptyWarning->setVisible(
          QDir::current().entryList(
            QStringList() << "*.[tT][iI][fF]" << "*.[tT][iI][fF][fF]" ) . size() );

    if ( ui->detPathSync->isChecked() && det->isConnected() ) {

      QString lastComponent = lastPathComponent(pth);
      QString detDir = det->path();
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
    connect( det, SIGNAL(pathChanged(QString)), thisSlot);
  }

  check( ui->detPathSync, ! ui->detPathSync->isChecked() ||
         lastPathComponent(det->path()) == lastPathComponent(ui->expPath->text()));

}


void MainWindow::updateUi_triggCT() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_triggCT());
    connect( tct, SIGNAL(connectionChanged(bool)), thisSlot);
    connect( tct, SIGNAL(runningChanged(bool)), thisSlot);
    connect( ui->triggCT, SIGNAL(toggled(bool)), thisSlot);
    connect( ui->continiousMode, SIGNAL(toggled(bool)), thisSlot);
  }

  bool isOK =
      ! ui->continiousMode->isChecked()  ||
      ! ui->triggCT->isChecked()  ||
      ( tct->isConnected() && ! tct->isRunning() );
  check(ui->triggCT, isOK);

  if ( ui->continiousMode->isChecked() &&
       ui->triggCT->isChecked() &&
       tct->isConnected() &&
       ! tct->motor().isEmpty() ) {
    if ( tct->motor() != thetaMotor->motor()->getPv() )
      thetaMotor->motor()->setPv(tct->motor());
    thetaMotor->lock(true);
  } else {
    thetaMotor->lock(false);
  }

  ui->triggCT->setEnabled( ui->continiousMode->isChecked() &&
                           ( ui->triggCT->isChecked() || tct->isConnected() ) );

}


void MainWindow::updateUi_checkDyno() {
  if ( ! sender() ) // called from the constructor;
    connect( ui->stepAndShotMode, SIGNAL(toggled(bool)), SLOT(updateUi_checkDyno()) );
  const bool sasMode = ui->stepAndShotMode->isChecked();
  if ( ! sasMode )
    ui->checkDyno->setChecked(false);
  ui->checkDyno->setVisible(sasMode);
}


void MainWindow::updateUi_checkMulti() {
  if ( ! sender() ) // called from the constructor;
    connect( ui->stepAndShotMode, SIGNAL(toggled(bool)), SLOT(updateUi_checkMulti()) );
  const bool sasMode = ui->stepAndShotMode->isChecked();
  if ( ! sasMode )
    ui->checkMulti->setChecked(false);
  ui->checkMulti->setVisible(sasMode);
}


void MainWindow::updateUi_nofScans() {
  if ( ! sender() ) // called from the constructor;
    connect(ui->endNumber, SIGNAL(toggled(bool)), SLOT(updateUi_nofScans()));
  const bool visible = ui->endNumber->isChecked();
  ui->nofScans->setVisible(visible);
  ui->nofScansLabel->setVisible(visible);
}

void MainWindow::updateUi_acquisitionTime() {
  if ( ! sender() ) // called from the constructor;
    connect(ui->endTime, SIGNAL(toggled(bool)), SLOT(updateUi_acquisitionTime()));
  const bool visible = ui->endTime->isChecked();
  ui->acquisitionTimeWdg->setVisible(visible);
  ui->acquisitionTimeLabel->setVisible(visible);
}

void MainWindow::updateUi_condtitionScript() {
  if ( ! sender() ) // called from the constructor;
    connect(ui->endCondition, SIGNAL(toggled(bool)), SLOT(updateUi_condtitionScript()));
  const bool visible = ui->endCondition->isChecked();
  ui->conditionScript->setVisible(visible);
  ui->conditionScriptLabel->setVisible(visible);
}

void MainWindow::updateUi_serialStep() {
  QCaMotor * mot = serialMotor->motor();
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_serialStep());
    connect( mot, SIGNAL(changedConnected(bool)), thisSlot);
    connect( mot, SIGNAL(changedPrecision(int)), thisSlot);
    connect( mot, SIGNAL(changedUnits(QString)), thisSlot);
    connect( mot, SIGNAL(changedUserLoLimit(double)), thisSlot);
    connect( mot, SIGNAL(changedUserHiLimit(double)), thisSlot);
    connect( mot, SIGNAL(changedUserPosition(double)), thisSlot);
    connect( ui->serialStep, SIGNAL(valueChanged(double)), thisSlot);
    connect( ui->checkSerial, SIGNAL(toggled(bool)), thisSlot);
    connect( ui->irregularStep, SIGNAL(toggled(bool)), thisSlot);
    connect( ui->endNumber, SIGNAL(toggled(bool)), thisSlot);
    connect( ui->nofScans, SIGNAL(valueChanged(int)), thisSlot);
  }

  const double endpos = mot->getUserPosition()
      + ( ui->nofScans->value() - 1 ) * ui->serialStep->value();
  const bool itemOK =
      ! ui->checkSerial->isChecked() ||
      ! mot->isConnected() ||
      ( ui->serialStep->value() != 0.0  &&
      ( ! ui->endNumber->isChecked() || ui->irregularStep->isChecked() ||
        ( endpos > mot->getUserLoLimit() && endpos < mot->getUserHiLimit() ) ) ) ;
  check(ui->serialStep, itemOK);

  if(mot->isConnected()) {
    ui->serialStep->setSuffix(mot->getUnits());
    ui->serialStep->setDecimals(mot->getPrecision());
  }

}


void MainWindow::updateUi_serialList() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_serialList());
    connect(ui->nofScans, SIGNAL(valueChanged(int)), thisSlot);
    connect(ui->irregularStep, SIGNAL(toggled(bool)), thisSlot);
    connect(ui->endNumber, SIGNAL(toggled(bool)), thisSlot);
    connect(ui->serialStep, SIGNAL(valueChanged(double)), thisSlot);
    connect(ui->serialPositionsList, SIGNAL(itemChanged(QTableWidgetItem*)), thisSlot);
    connect(serialMotor->motor(), SIGNAL(changedUserHiLimit(double)), thisSlot);
    connect(serialMotor->motor(), SIGNAL(changedUserLoLimit(double)), thisSlot);
    connect(serialMotor->motor(), SIGNAL(changedPv(QString)), thisSlot);
    connect(serialMotor->motor(), SIGNAL(changedUserPosition(double)), thisSlot);
    connect(serialMotor->motor(), SIGNAL(changedMoving(bool)), thisSlot);

    ui->serialPositionsList->setItemDelegate(new NTableDelegate(ui->serialPositionsList));

  }

  if (inCT)
    return;

  ui->serialPositionsList->setVisible( ui->endNumber->isChecked() );
  ui->irregularStep->setVisible( ui->endNumber->isChecked() );
  ui->irregularOrLabel->setVisible( ui->endNumber->isChecked() );
  ui->serialPositionsList->setEnabled( ui->irregularStep->isChecked() );
  ui->serialStep->setEnabled( ! ui->endNumber->isChecked()  ||
                              ! ui->irregularStep->isChecked() );

  const int nofScans = ui->nofScans->value();
  while ( nofScans < ui->serialPositionsList->rowCount() )
    ui->serialPositionsList->removeRow( ui->serialPositionsList->rowCount()-1 );
  while ( nofScans > ui->serialPositionsList->rowCount() ) {
    ui->serialPositionsList->insertRow(ui->serialPositionsList->rowCount());
    ui->serialPositionsList->setItem(
          ui->serialPositionsList->rowCount()-1, 0,
          new QTableWidgetItem( QString::number(serialMotor->motor()->getUserPosition() ) ) );
  }

  bool allOK=true;
  foreach (QTableWidgetItem * item,
           ui->serialPositionsList->findItems("", Qt::MatchContains) ) {

    if ( ! ui->irregularStep->isChecked() ) {
      if ( ! inCT && ! serialMotor->motor()->isMoving() ) {
        double pos = serialMotor->motor()->getUserPosition()
            + item->row() * ui->serialStep->value();
        if ( pos != item->text().toDouble() )
          item->setText( QString::number(pos) );
      }
    }

    bool isDouble;
    const double pos = item->text().toDouble(&isDouble);
    const bool isOK = isDouble &&
        (pos > serialMotor->motor()->getUserLoLimit() ) &&
        (pos < serialMotor->motor()->getUserHiLimit() );
    item->setBackground( isOK ? QBrush() : QBrush(QColor(Qt::red)));
    allOK &= isOK;
  }

  check( ui->irregularStep,  ! ui->irregularStep->isChecked() || allOK );

}

void MainWindow::updateUi_serialMotor() {
  QCaMotor * mot = serialMotor->motor();
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_serialMotor());
    connect( mot, SIGNAL(changedConnected(bool)), thisSlot);
    connect( mot, SIGNAL(changedPrecision(int)), thisSlot);
    connect( mot, SIGNAL(changedUnits(QString)), thisSlot);
    connect( mot, SIGNAL(changedPv()), thisSlot);
    connect( mot, SIGNAL(changedMoving(bool)), thisSlot);
    connect( mot, SIGNAL(changedLoLimitStatus(bool)), thisSlot);
    connect( mot, SIGNAL(changedHiLimitStatus(bool)), thisSlot);
    connect( ui->checkSerial, SIGNAL(toggled(bool)), thisSlot);

    connect( mot, SIGNAL(changedUserPosition(double)),
             ui->serialCurrent, SLOT(setValue(double)));
  }

  setenv("SERIALMOTORPV", mot->getPv().toAscii() , 1);

  check(serialMotor->setupButton(),
        ! ui->checkSerial->isChecked() ||
        mot->getPv().isEmpty() ||
        ( mot->isConnected() &&
          ! mot->isMoving() ) );
  check(ui->serialCurrent,
        ! ui->checkSerial->isChecked() ||
        mot->getPv().isEmpty() ||
        ! mot->getLimitStatus() );

  if ( ! mot->isConnected() ) {
    ui->serialCurrent->setText("no link");
    return;
  }
  ui->serialCurrent->setValue(mot->getUserPosition());
  ui->serialCurrent->setSuffix(mot->getUnits());
  ui->serialCurrent->setDecimals(mot->getPrecision());

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
    connect(ui->ongoingSeries, SIGNAL(toggled(bool)), thisSlot);
    connect(ui->endNumber, SIGNAL(toggled(bool)), thisSlot);
    connect(ui->nofScans, SIGNAL(valueChanged(int)), thisSlot);
  }

  if ( mot->isConnected() ) {
    ui->scanRange->setSuffix(mot->getUnits());
    ui->scanRange->setDecimals(mot->getPrecision());
  }

  double endpos = mot->getUserPosition();
  if ( ui->checkSerial->isChecked() &&
       ui->ongoingSeries->isChecked() &&
       ui->endNumber->isChecked() )
    endpos +=  ( ui->nofScans->value() - 1 ) * ui->scanRange->value();
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
    connect( ui->stepAndShotMode, SIGNAL(toggled(bool)), thisSlot);
    connect( ui->checkDyno, SIGNAL(toggled(bool)), thisSlot);
  }

  bool vis = ui->stepAndShotMode->isChecked() && ! ui->checkDyno->isChecked();
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

void MainWindow::updateUi_rotSpeed() {
  QCaMotor * mot = thetaMotor->motor();
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_rotSpeed());
    connect( ui->stepAndShotMode, SIGNAL(toggled(bool)), thisSlot);
    connect( ui->rotSpeed, SIGNAL(valueChanged(double)), thisSlot);
    connect( mot, SIGNAL(changedConnected(bool)), thisSlot);
    connect( mot, SIGNAL(changedPrecision(int)), thisSlot);
    connect( mot, SIGNAL(changedUnits(QString)), thisSlot);
    connect( mot, SIGNAL(changedNormalSpeed(double)), thisSlot);
    connect( mot, SIGNAL(changedMaximumSpeed(double)), thisSlot);
  }

  const bool sasMode = ui->stepAndShotMode->isChecked();
  ui->rotSpeedLabel->setVisible(!sasMode);
  ui->rotSpeed->setVisible(!sasMode);

  const bool motIsConnected = mot->isConnected();

  if ( ui->rotSpeed->value()==0.0 )
    ui->rotSpeed->setValue(mot->getNormalSpeed());

  if ( motIsConnected ) {
    ui->rotSpeed->setSuffix(mot->getUnits()+"/s");
    ui->rotSpeed->setDecimals(mot->getPrecision());
  }

  bool itemOK =
      sasMode ||
      ui->rotSpeed->value() > 0 ||
      ui->rotSpeed->value() <= mot->getMaximumSpeed();
  check( ui->rotSpeed, itemOK );

}

void MainWindow::updateUi_stepTime() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_stepTime());
    connect( ui->rotSpeed, SIGNAL(valueChanged(double)), thisSlot);
    connect( ui->scanRange, SIGNAL(valueChanged(double)), thisSlot);
    connect( ui->scanProjections, SIGNAL(valueChanged(int)), thisSlot);
    connect( det, SIGNAL(exposureChanged(double)), thisSlot);
  }

  if ( ui->rotSpeed->value() > 0 && ui->scanRange->value() != 0.0 ) {
    const double stepTime = qAbs(ui->scanRange->value()) /
        (ui->scanProjections->value() * ui->rotSpeed->value());
    ui->stepTime->setValue(stepTime);
    ui->expOverStep->setValue( det->exposure() / stepTime );
  } else {
    ui->stepTime->setText("");
    ui->expOverStep->setText("");
  }
}

void MainWindow::updateUi_expOverStep() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_expOverStep());
    connect( ui->stepAndShotMode, SIGNAL(toggled(bool)), thisSlot);
    connect( ui->expOverStep, SIGNAL(somethingChanged(QString)), thisSlot);
  }
  bool ok;
  double num = ui->expOverStep->text().toDouble(&ok);
  check( ui->expOverStep,  ui->stepAndShotMode->isChecked() ||
                           ( ok && num > 0 && num < 1.0 ) );

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
    connect( mot, SIGNAL(changedUserPosition(double)),
            ui->scanCurrent, SLOT(setValue(double)));
  }

  check(thetaMotor->setupButton(),
        mot->isConnected() &&
        ! mot->isMoving() );
  check(ui->scanCurrent,
        ! mot->getLimitStatus() );

  if (!mot->isConnected()) {
    ui->scanCurrent->setText("no link");
    return;
  }
  ui->scanCurrent->setValue(mot->getUserPosition());

  const QString units = mot->getUnits();
  ui->scanCurrent->setSuffix(units);
  ui->normalSpeed->setSuffix(units+"/s");
  ui->maximumSpeed->setSuffix(units+"/s");

  const int prec = mot->getPrecision();
  ui->scanCurrent->setDecimals(prec);
  ui->normalSpeed->setDecimals(prec);
  ui->maximumSpeed->setDecimals(prec);

  ui->normalSpeed->setValue(mot->getNormalSpeed());
  ui->maximumSpeed->setValue(mot->getMaximumSpeed());

}

void MainWindow::updateUi_bgInterval() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_bgInterval());
    connect( ui->scanProjections, SIGNAL(valueChanged(int)), thisSlot);
    connect( ui->stepAndShotMode, SIGNAL(toggled(bool)), thisSlot);
    connect( ui->continiousMode, SIGNAL(toggled(bool)), thisSlot);
    connect( ui->nofBGs, SIGNAL(valueChanged(int)), thisSlot);
    connect( ui->bgInterval, SIGNAL(valueChanged(int)), thisSlot);
    connect( ui->bgIntervalAfter, SIGNAL(toggled(bool)), thisSlot);
    connect( ui->bgIntervalBefore, SIGNAL(toggled(bool)), thisSlot);
  }

  ui->bgIntervalWdg->setEnabled(ui->nofBGs->value());
  if (ui->stepAndShotMode->isChecked())
    ui->bgIntervalWdg->setCurrentWidget(ui->bgIntervalSAS);
  else
    ui->bgIntervalWdg->setCurrentWidget(ui->bgIntervalContinious);

  bool itemOK;

  itemOK =
      ! ui->stepAndShotMode->isChecked() ||
      ! ui->nofBGs->value() ||
      ui->bgInterval->value() <= ui->scanProjections->value();
  check(ui->bgInterval, itemOK);

  itemOK =
      ! ui->continiousMode->isChecked() ||
      ! ui->nofBGs->value() ||
      ( ui->bgIntervalAfter->isChecked() || ui->bgIntervalBefore->isChecked() );
  check(ui->bgIntervalAfter, itemOK );
  check(ui->bgIntervalBefore, itemOK );

}

void MainWindow::updateUi_dfInterval() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_dfInterval());
    connect( ui->scanProjections, SIGNAL(valueChanged(int)), thisSlot);
    connect( ui->stepAndShotMode, SIGNAL(toggled(bool)), thisSlot);
    connect( ui->continiousMode, SIGNAL(toggled(bool)), thisSlot);
    connect( ui->nofDFs, SIGNAL(valueChanged(int)), thisSlot);
    connect( ui->dfInterval, SIGNAL(valueChanged(int)), thisSlot);
    connect( ui->dfIntervalAfter, SIGNAL(toggled(bool)), thisSlot);
    connect( ui->dfIntervalBefore, SIGNAL(toggled(bool)), thisSlot);
  }

  ui->dfIntervalWdg->setEnabled(ui->nofDFs->value());
  if (ui->stepAndShotMode->isChecked())
    ui->dfIntervalWdg->setCurrentWidget(ui->dfIntervalSAS);
  else
    ui->dfIntervalWdg->setCurrentWidget(ui->dfIntervalContinious);

  bool itemOK;

  itemOK =
      ! ui->stepAndShotMode->isChecked() ||
      ! ui->nofDFs->value() ||
      ui->dfInterval->value() <= ui->scanProjections->value();
  check(ui->dfInterval, itemOK);

  itemOK =
      ! ui->continiousMode->isChecked() ||
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
    connect( ui->stepAndShotMode, SIGNAL(toggled(bool)), thisSlot);
    connect( ui->nofBGs, SIGNAL(valueChanged(int)), thisSlot);
    connect( ui->bgTravel, SIGNAL(valueChanged(double)), thisSlot);
    connect( mot, SIGNAL(changedPrecision(int)), thisSlot);
    connect( mot, SIGNAL(changedUnits(QString)), thisSlot);
    connect( mot, SIGNAL(changedConnected(bool)), thisSlot);
    connect( mot, SIGNAL(changedUserLoLimit(double)), thisSlot);
    connect( mot, SIGNAL(changedUserHiLimit(double)), thisSlot);
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
    connect( mot, SIGNAL(changedPrecision(int)), thisSlot);
    connect( mot, SIGNAL(changedUnits(QString)), thisSlot);
    connect( mot, SIGNAL(changedMoving(bool)), thisSlot);
    connect( mot, SIGNAL(changedLoLimitStatus(bool)), thisSlot);
    connect( mot, SIGNAL(changedHiLimitStatus(bool)), thisSlot);

    connect(mot, SIGNAL(changedUserPosition(double)),
            ui->bgCurrent, SLOT(setValue(double)));
  }

  const int nofbgs = ui->nofBGs->value();
  const bool motIsConnected = mot->isConnected();
  const bool doFF = ui->checkFF->isChecked();
  bgMotor->setupButton()->setEnabled(nofbgs);

  check(bgMotor->setupButton(), ! doFF || ! nofbgs ||
        ( motIsConnected && ! mot->isMoving() ) );
  check(ui->bgCurrent, ! doFF || ! nofbgs ||
        ! mot->getLimitStatus() );

  if ( ! motIsConnected ) {
    ui->bgCurrent->setText("no link");
    return;
  }
  ui->bgCurrent->setValue(mot->getUserPosition());
  ui->bgCurrent->setSuffix(mot->getUnits());
  ui->bgCurrent->setDecimals(mot->getPrecision());

}

void MainWindow::updateUi_shutterStatus() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_shutterStatus());
    connect( ui->checkFF, SIGNAL(toggled(bool)), thisSlot);
    connect( ui->nofDFs, SIGNAL(valueChanged(int)), thisSlot);
    connect(sh1A, SIGNAL(stateChanged(Shutter1A::State)), thisSlot);
    connect(sh1A, SIGNAL(enabledChanged(bool)), thisSlot);
    connect(sh1A, SIGNAL(connectionChanged(bool)), thisSlot);
  }

  if ( ! sh1A->isConnected() )
    ui->shutterStatus->setText("no link");
  else
    switch (sh1A->state()) {
      case Shutter1A::BETWEEN :
        ui->shutterStatus->setText("in progress");
        break;
      case Shutter1A::OPENED :
        ui->shutterStatus->setText("opened");
        break;
      case Shutter1A::CLOSED :
        ui->shutterStatus->setText("closed");
        break;
    }

  check( ui->shutterStatus,
         ! ui->nofDFs->value() || ! ui->checkFF->isChecked() ||
        ( sh1A->isConnected() && sh1A->isEnabled() && sh1A->state() != Shutter1A::BETWEEN ) );

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
    connect( mot, SIGNAL(changedPrecision(int)), thisSlot);
    connect( mot, SIGNAL(changedPv(QString)), thisSlot);
    connect( mot, SIGNAL(changedUnits(QString)), thisSlot);
    connect( mot, SIGNAL(changedMoving(bool)), thisSlot);
    connect( mot, SIGNAL(changedLoLimitStatus(bool)), thisSlot);
    connect( mot, SIGNAL(changedHiLimitStatus(bool)), thisSlot);

    connect(mot, SIGNAL(changedUserPosition(double)),
            ui->loopCurrent, SLOT(setValue(double)));
  }

  check(loopMotor->setupButton(),
        ! ui->checkMulti->isChecked() ||
        mot->getPv().isEmpty() ||
        ( mot->isConnected() &&
          ! mot->isMoving() ) );
  check(ui->loopCurrent,
        ! ui->checkMulti->isChecked() ||
        mot->getPv().isEmpty() ||
        ! mot->getLimitStatus() );

  if ( ! mot->isConnected() ) {
    ui->loopCurrent->setText("no link");
    return;
  }
  ui->loopCurrent->setValue(mot->getUserPosition());
  ui->loopCurrent->setSuffix(mot->getUnits());
  ui->loopCurrent->setDecimals(mot->getPrecision());

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

    connect(mot, SIGNAL(changedUserPosition(double)),
            ui->subLoopCurrent, SLOT(setValue(double)));
  }

  check(subLoopMotor->setupButton(),
        ! ui->checkMulti->isChecked() ||
        ! ui->subLoop->isChecked() ||
        mot->getPv().isEmpty() ||
        ( mot->isConnected() &&
          ! mot->isMoving() ) );

  check(ui->subLoopCurrent,
        ! ui->checkMulti->isChecked() ||
        ! ui->subLoop->isChecked() ||
        mot->getPv().isEmpty() ||
        ! mot->getLimitStatus() );

  if ( ! mot->isConnected() ) {
    ui->subLoopCurrent->setText("no link");
    return;
  }
  ui->subLoopCurrent->setValue(mot->getUserPosition());
  ui->subLoopCurrent->setSuffix(mot->getUnits());
  ui->subLoopCurrent->setDecimals(mot->getPrecision());

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

    connect(dynoMotor->motor(), SIGNAL(changedUserPosition(double)),
            ui->dynoCurrent, SLOT(setValue(double)));
    connect(dynoMotor->motor(), SIGNAL(changedNormalSpeed(double)),
            ui->dynoNormalSpeed, SLOT(setValue(double)));

  }

  check(dynoMotor->setupButton(),
        ! ui->checkDyno->isChecked() ||
        ( dynoMotor->motor()->isConnected() &&
          ! dynoMotor->motor()->isMoving() ) );
  check(ui->dynoCurrent,
        ! ui->checkDyno->isChecked() ||
        ! dynoMotor->motor()->getLimitStatus() );

  if ( ! dynoMotor->motor()->isConnected() ) {
    ui->dynoCurrent->setText("no link");
    return;
  }
  ui->dynoCurrent->setValue(dynoMotor->motor()->getUserPosition());
  ui->dynoCurrent->setSuffix(dynoMotor->motor()->getUnits());
  ui->dynoCurrent->setDecimals(dynoMotor->motor()->getPrecision());

  ui->dynoNormalSpeed->setDecimals(dynoMotor->motor()->getPrecision());
  ui->dynoNormalSpeed->setSuffix(dynoMotor->motor()->getUnits()+"/s");

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

    connect(dyno2Motor->motor(), SIGNAL(changedUserPosition(double)),
            ui->dyno2Current, SLOT(setValue(double)));
    connect(dyno2Motor->motor(), SIGNAL(changedNormalSpeed(double)),
            ui->dyno2NormalSpeed, SLOT(setValue(double)));

  }

  check(dyno2Motor->setupButton(),
        ! ui->checkDyno->isChecked() ||
        ! ui->dyno2->isChecked() ||
        ( dyno2Motor->motor()->isConnected() &&
          ! dyno2Motor->motor()->isMoving() ) );
  check(ui->dyno2Current,
        ! ui->checkDyno->isChecked() ||
        ! ui->dyno2->isChecked() ||
        ! dyno2Motor->motor()->getLimitStatus() );

  if ( ! dyno2Motor->motor()->isConnected() ) {
    ui->dyno2Current->setText("no link");
    return;
  }
  ui->dyno2Current->setValue(dyno2Motor->motor()->getUserPosition());
  ui->dyno2Current->setSuffix(dyno2Motor->motor()->getUnits());
  ui->dyno2Current->setDecimals(dyno2Motor->motor()->getPrecision());

  ui->dyno2NormalSpeed->setDecimals(dyno2Motor->motor()->getPrecision());
  ui->dyno2NormalSpeed->setSuffix(dyno2Motor->motor()->getUnits()+"/s");

}

void MainWindow::updateUi_detector() {
  if ( ! sender() ) {
    const char* thisSlot = SLOT(updateUi_detector());
    connect(ui->detSelection, SIGNAL(currentIndexChanged(int)), SLOT(onDetectorSelection()));
    connect(det, SIGNAL(connectionChanged(bool)), thisSlot);
    connect(det, SIGNAL(parameterChanged()), thisSlot);
    connect(det, SIGNAL(counterChanged(int)), ui->detProgress, SLOT(setValue(int)));
    connect(det, SIGNAL(totalImagesChanged(int)), ui->detProgress, SLOT(setMaximum(int)));
    connect(det, SIGNAL(counterChanged(int)), ui->detImageCounter, SLOT(setValue(int)));
    connect(det, SIGNAL(totalImagesChanged(int)), ui->detTotalImages, SLOT(setValue(int)));
    connect(det, SIGNAL(exposureChanged(double)), ui->exposureInfo, SLOT(setValue(double)));
    connect(det, SIGNAL(exposureChanged(double)), ui->detExposure, SLOT(setValue(double)));
    connect(det, SIGNAL(periodChanged(double)), ui->detPeriod, SLOT(setValue(double)));
    connect(det, SIGNAL(nameChanged(QString)), ui->detFileName, SLOT(setText(QString)));
    connect(det, SIGNAL(lastNameChanged(QString)), ui->detFileLastName, SLOT(setText(QString)));
    connect(det, SIGNAL(templateChanged(QString)), ui->detFileTemplate, SLOT(setText(QString)));
    connect(det, SIGNAL(pathChanged(QString)), ui->detPath, SLOT(setText(QString)));
    connect(det, SIGNAL(counterChanged(int)), SLOT(accumulateLog()));
    connect(det, SIGNAL(lastNameChanged(QString)), this, SLOT(nameToLog(QString)));
  }

  totalShots = det->number();
  currentShot  =  det->isAcquiring() ? det->counter() : -1;

  if ( ! det->isConnected() )
    ui->detStatus->setText("no link");
  else if ( det->isAcquiring() )
    ui->detStatus->setText("acquiring");
  else if ( det->isWriting() )
    ui->detStatus->setText("writing");
  else
    ui->detStatus->setText("idle");

  if (  det->isConnected() ) {
    ui->detTriggerMode->setText(det->triggerModeString());
    ui->detImageMode->setText(det->imageModeString());
    ui->detTotalImages->setValue(det->number());
  }

  check (ui->detStatus, det->isConnected() &&
         ( inCT || ( ! det->isAcquiring() && ! det->isWriting() ) ) );
  check (ui->detPath, det->pathExists() );


}





void MainWindow::onWorkingDirBrowse() {
  ui->expPath->setText(
        QFileDialog::getExistingDirectory(0, "Working directory", QDir::currentPath()) );
}

void MainWindow::onAcquisitionMode() {
  const bool sasMode = ui->stepAndShotMode->isChecked();
  ui->ffOnEachScan->setVisible(!sasMode);
  ui->speedsLine->setVisible(!sasMode);
  ui->speedsLabel->setVisible(!sasMode);
  ui->timesLabel->setVisible(!sasMode);
  ui->normalSpeed->setVisible(!sasMode);
  ui->normalSpeedLabel->setVisible(!sasMode);
  ui->stepTime->setVisible(!sasMode);
  ui->stepTimeLabel->setVisible(!sasMode);
  ui->expOverStep->setVisible(!sasMode);
  ui->expOverStepLabel->setVisible(!sasMode);
  ui->maximumSpeed->setVisible(!sasMode);
  ui->maximumSpeedLabel->setVisible(!sasMode);
  ui->exposureInfo->setVisible(!sasMode);
  ui->exposureInfoLabel->setVisible(!sasMode);
}

void MainWindow::onSerialCheck() {
  ui->control->setTabVisible(ui->tabSerial, ui->checkSerial->isChecked());
}

void MainWindow::onFFcheck() {
  ui->control->setTabVisible(ui->tabFF, ui->checkFF->isChecked());
}

void MainWindow::onDynoCheck() {
  ui->control->setTabVisible(ui->tabDyno, ui->checkDyno->isChecked());
}

void MainWindow::onMultiCheck() {
  ui->control->setTabVisible(ui->tabMulti, ui->checkMulti->isChecked());
}

void MainWindow::onSubLoop() {
  const bool dosl=ui->subLoop->isChecked();
  ui->subLoopCurrent->setVisible(dosl);
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
  ui->dyno2Current->setVisible(dod2);
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
    acquireBG("");
    acquireDF("", sh1A->state());
    check(ui->testFF, true);
    det->setAutoSave(false);
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
    acquireMulti("",  ui->stepAndShotMode->isChecked() && ! ui->checkDyno->isChecked()  ?
                   ui->aqsPP->value() : 1);
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
    acquireDetector(det->name(),
                    ui->stepAndShotMode->isChecked() && ! ui->checkDyno->isChecked() ?
                      ui->aqsPP->value() : 1);
    check(ui->testDetector, true);
    inAcquisitionTest=false;
    det->setAutoSave(false);
    ui->testDetector->setText("Test");
    updateUi_detector();
  }
}


















void MainWindow::check(QWidget * obj, bool status) {

  if ( ! obj )
    return;


  static const QString warnStyle = "background-color: rgba(255, 0, 0, 128);";
  obj->setStyleSheet( status  ?  ""  :  warnStyle );

  QWidget * tab = 0;
  if ( ! preReq.contains(obj) ) {

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
  QString fileT = "%s%s";
  if (count>1)
    fileT += "_%0" + QString::number(QString::number(count).length()) + "d";
  fileT+= ".tif";

  return
      det->setNameTemplate(fileT) &&
      det->isConnected() &&
      det->setNumber(count) &&
      det->setName(filetemplate) &&
      det->prepareForAcq() ;

}

int MainWindow::acquireDetector() {

  int execStatus = -1;
  execStatus = ui->preAqScript->execute();
  if ( execStatus || stopMe )
    goto acquireDetectorExit;
  det->acquire();
  if ( ! stopMe )
    execStatus = ui->postAqScript->execute();

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

    ftemplate = filetemplate.isEmpty() ? det->name() : filetemplate;
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
    det->setName(ftemplate) ;

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

  const QString ftemplate = ( filetemplate.isEmpty() ? det->name() : filetemplate );

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
    det->setName(ftemplate) ;

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






void MainWindow::appendMessage(MsgType tp, const QString &msg) {
  QString sText = QTime::currentTime().toString("hh:mm:ss.zzz") + ' ' + msg;
  QString colorS;
  switch (tp) {
  case (LOG) :
    colorS = "black" ; break ;
  case (ERROR) :
    colorS = "red" ; break ;
  case (CONTROL) :
    colorS = "blue" ; break ;
  }
  sText = "<font color=\"" + colorS + "\">" + sText + "</font>";
  ui->messages->append(sText);
}

void MainWindow::logMessage(const QString &msg) {
  appendMessage(LOG, msg);
}

void MainWindow::stopAll() {
  stopMe=true;
  emit requestToStopAcquisition();
  det->stop();
  serialMotor->motor()->stop();
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
    stopMe=false;
    engineRun();
    ui->startStop->setText("Start CT");
  }
}


/*
void MainWindow::appendScanListRow(Role rl, double pos, const QString & fn) {

  QList<QStandardItem *> items;
  QStandardItem * ti;

  ti = new QStandardItem(true);
  ti->setCheckable(true);
  ti->setCheckState(Qt::Checked);
  items << ti;

  ti = new QStandardItem(true);
  QString roleText = roleName[rl];
  if (rl == LOOP) roleText = "  " + roleText;
  if (rl == SLOOP) roleText = "    " + roleText;
  ti->setText(roleText);
  ti->setEditable(false);
  items << ti;

  ti = new QStandardItem(true);
  ti->setText( rl == DF  ?  "closed"  :  QString::number(pos) );
  ti->setEditable(false);
  items << ti;

  ti = new QStandardItem(true);
  ti->setText(fn);
  ti->setEditable(false);
  items << ti;

  // scanList->appendRow(items);

}
*/



void MainWindow::updateProgress () {

  if (currentScan>=0) {

    if ( ! ui->serialProgress->isVisible() ) {
      if ( totalScans ) {
        ui->serialProgress->setMaximum(totalScans);
        ui->serialProgress->setFormat("Series progress. %p% (scans complete: %v of %m)");
      } else if ( ui->endTime->isChecked() ) {
        ui->serialProgress->setMaximum(QTime(0, 0, 0, 0).msecsTo( ui->acquisitionTime->time() ));
      } else {
        ui->serialProgress->setMaximum(0);
        ui->serialProgress->setFormat("Series progress. Scans complete: %v.");
      }
      ui->serialProgress->setVisible(true);
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

  } else
    ui->serialProgress->setVisible(false);





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

  ui->detProgress->setVisible( currentShot>=0 && totalShots>1 && det->imageMode() == 1 );

  QTimer::singleShot(50, this, SLOT(updateProgress()));

}



int MainWindow::acquireProjection(const QString &filetemplate) {
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

  if ( ! bgMotor->motor()->isConnected() || bgs <1 || bgTravel == 0.0 )
    return ret;

  const QString detfilename=det->name();
  QString ftemplate = "BG_" +  ( filetemplate.isEmpty() ? det->name() : filetemplate );
  setenv("CONTRASTTYPE", "BG", 1);

  bgMotor->motor()->wait_stop();
  const double bgStart = bgMotor->motor()->getUserPosition();

  bgMotor->motor()->goUserPosition
      ( bgStart + bgTravel, QCaMotor::STOPPED );
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

  if ( filetemplate.isEmpty() && ! detfilename.isEmpty() )
    det->setName(detfilename) ;

  if (!stopMe)
    bgMotor->motor()->wait_stop();
  return ret;

}



int MainWindow::acquireDF(const QString &filetemplate, Shutter1A::State stateToGo) {

  int ret = -1;
  const int dfs = ui->nofDFs->value();
  const Shutter1A::State shState = sh1A->state();

  if ( dfs<1 )
    return 0;
  if ( ! sh1A->isEnabled() && shState != Shutter1A::CLOSED )
    return -1 ;

  const QString detfilename=det->name();
  QString ftemplate = "DF_" + ( filetemplate.isEmpty() ? det->name() : filetemplate );
  setenv("CONTRASTTYPE", "DF", 1);

  /*if (shState != Shutter1A::CLOSED)
    sh1A->close(true); /**/
  if (stopMe) goto onDfExit;

  det->setPeriod(0);

  ret = acquireDetector(ftemplate, dfs);

onDfExit:

  if ( filetemplate.isEmpty()  &&  ! detfilename.isEmpty() )
    det->setName(detfilename) ;

/*  if (stateToGo == Shutter1A::OPENED)
    sh1A->open(!stopMe);
  else if (stateToGo == Shutter1A::CLOSED)
    sh1A->close(!stopMe);
  if ( ! stopMe && sh1A->state() != stateToGo)
    qtWait(sh1A, SIGNAL(stateChanged(Shutter1A::State)), 500); /**/

  return ret;

}



void MainWindow::accumulateLog() {

  if ( ! inCT  || ! det->counter() )
    return;

  QString wrt = QDateTime::currentDateTime().toString("dd/MM/yyyy_hh:mm:ss.zzz");
  if ( ui->checkSerial->isChecked() && serialMotor->motor()->isConnected() )
    wrt += " " + QString::number(serialMotor->motor()->getUserPosition());
  wrt += " " + QString::number(thetaMotor->motor()->getUserPosition());
  if ( ui->checkFF->isChecked() && ui->nofBGs->value() && bgMotor->motor()->isConnected() )
    wrt += " " + QString::number(bgMotor->motor()->getUserPosition());
  if ( ui->checkMulti->isChecked() && loopMotor->motor()->isConnected() )
    wrt += " " + QString::number(loopMotor->motor()->getUserPosition());
  if ( ui->checkMulti->isChecked() &&
       ui->subLoop->isChecked() && subLoopMotor->motor()->isConnected() )
    wrt += " " + QString::number(subLoopMotor->motor()->getUserPosition());
  wrt += " ";

  accumulatedLog.append(wrt);

}


void MainWindow::nameToLog(const QString & fileName) {
  if ( accumulatedLog.isEmpty() || ! logFile || ! logFile->isWritable() )
    return;
  logFile->write( (accumulatedLog.takeFirst() + fileName +"\n") . toAscii() );
}





void MainWindow::engineRun () {

  if ( ! readyToStartCT || inCT )
    return;

  if (logFile) {
    qDebug() << "Log file is unexpectedly opened. Must be a bug. Will not proceed.";
    return;
  }
  logFile = new QFile("acquisition.log");
  if ( ! logFile || ! logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text) ) {
    qDebug() << "Could not open log file for writing. Will not proceed.";
    logFile = 0;
    return;
  }
  accumulatedLog.clear();

  QString configName = "acquisition.configuration";
  int attempt=0;
  while (QFile::exists(configName))
      configName = "acquisition." + QString::number(++attempt) + ".configuration";
  saveConfiguration(configName);

  totalProjections = ui->scanProjections->value();

  const double
      serialStart =  serialMotor->motor()->getUserPosition(),
      bgStart =  bgMotor->motor()->getUserPosition(),
      thetaStart = thetaMotor->motor()->getUserPosition(),
      thetaRange = ui->scanRange->value(),
      thetaSpeed = thetaMotor->motor()->getNormalSpeed();
  double thetaInSeriesStart = thetaStart;

  const bool
      doSerial = ui->checkSerial->isChecked(),
      doBG = ui->checkFF->isChecked() && ui->nofBGs->value(),
      bgBefore = ui->bgIntervalBefore->isChecked(),
      bgAfter = ui->bgIntervalAfter->isChecked(),
      dfBefore = ui->dfIntervalBefore->isChecked(),
      dfAfter = ui->dfIntervalAfter->isChecked(),
      doDF = ui->checkFF->isChecked() && ui->nofDFs->value(),
      ongoingSeries = doSerial && ui->ongoingSeries->isChecked(),
      doTriggCT = ui->continiousMode->isChecked() && ui->triggCT->isChecked();

  totalScans = doSerial ?
    ( ui->endNumber->isChecked() ? ui->nofScans->value() : 0 ) : 1 ;

  const int
      bgInterval = ui->bgInterval->value(),
      dfInterval = ui->dfInterval->value(),
      scanDelay = QTime(0, 0, 0, 0).msecsTo( ui->scanDelay->time() ),
      scanTime = QTime(0, 0, 0, 0).msecsTo( ui->acquisitionTime->time() ),
      projectionDigs = QString::number(totalProjections).size(),
      seriesDigs = QString::number(totalScans).size(),
      doAdd = ui->scanAdd->isChecked() ? 1 : 0,
      detimode=det->imageMode(),
      dettmode=det->triggerMode();

  // Log header
  QString wrt = "# Starting acquisition. " + QDateTime::currentDateTime().toString("dd/MM/yyyy_hh:mm:ss.zzz");
  logFile->write((wrt+"\n").toAscii());
  logFile->write("# Initial values:\n");
  wrt = "#   Working directory: " + QDir::currentPath();
  logFile->write((wrt+"\n").toAscii());
  wrt = "#   Detector saving path: " + det->path();
  logFile->write((wrt+"\n").toAscii());
  if ( doSerial && serialMotor->motor()->isConnected() ) {
    wrt = "#   Serial motor position: " + QString::number(serialMotor->motor()->getUserPosition());
    logFile->write((wrt+"\n").toAscii());
  }
  wrt = "#   Theta motor position: " + QString::number(thetaMotor->motor()->getUserPosition());
  logFile->write((wrt+"\n").toAscii());
  if (doBG && bgMotor->motor()->isConnected()) {
    wrt = "#   Background motor position: " + QString::number(bgMotor->motor()->getUserPosition());
    logFile->write((wrt+"\n").toAscii());
  }
  if ( ui->checkMulti->isChecked() && loopMotor->motor()->isConnected() ) {
    wrt = "#   Loop motor position: " + QString::number(loopMotor->motor()->getUserPosition());
    logFile->write((wrt+"\n").toAscii());
  }
  if ( ui->checkMulti->isChecked() &&
       ui->subLoop->isChecked() && subLoopMotor->motor()->isConnected() ) {
    wrt = "#   Sub-loop motor position: " + QString::number(subLoopMotor->motor()->getUserPosition());
    logFile->write((wrt+"\n").toAscii());
  }
  if ( ui->checkDyno->isChecked() && dynoMotor->motor()->isConnected() ) {
    wrt = "#   Dyno motor position: " + QString::number(dynoMotor->motor()->getUserPosition());
    logFile->write((wrt+"\n").toAscii());
  }
  if ( ui->checkDyno->isChecked() && ui->dyno2->isChecked() && dyno2Motor->motor()->isConnected() ) {
    wrt = "#   Dyno 2 motor position: " + QString::number(dyno2Motor->motor()->getUserPosition());
    logFile->write((wrt+"\n").toAscii());
  }
  // end logheader

  stopMe = false;
  inCT = true;
  foreach(QWidget * tab, ui->control->tabs())
        tab->setEnabled(false);


  inCTtime.restart();
  currentScan=0;
  bool timeToStop=false;

  int
      beforeBG = 0,
      beforeDF = 0;

  ui->preRunScript->execute();
  if (stopMe) goto onEngineExit;

  if ( doSerial  &&
       serialMotor->motor()->isConnected() &&
       ui->serialPositionsList->isVisible() ) // otherwise is already in the first point
    serialMotor->motor()->goUserPosition(
          ui->serialPositionsList->item(currentScan, 0)->text().toDouble(), QCaMotor::STARTED);
  if (stopMe) goto onEngineExit;

  /* sh1A->open(true); /**/





  do { // serial scanning

    setenv("CURRENTSCAN", QString::number(currentScan).toAscii(), 1);

    if (doSerial  && serialMotor->motor()->isConnected())
      serialMotor->motor()->wait_stop();

    if (stopMe) goto onEngineExit;

    ui->preScanScript->execute();

    if (stopMe) goto onEngineExit;

    QString seriesName;
    if (totalScans==1)
      seriesName = "";
    else if (totalScans>1)
      seriesName = QString("Z%1_").arg(currentScan, seriesDigs, 10, QChar('0') );
    else
      seriesName = "Z" + QString::number(currentScan) + "_";

    if (ui->stepAndShotMode->isChecked()) { // STEP-AND-SHOT

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
          acquireDF(projectionName, Shutter1A::OPENED);
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
        acquireDF(projectionName, Shutter1A::OPENED);
        if (stopMe) goto onEngineExit;
        beforeDF = dfInterval;
      }

    } else { // CONTINIOUS

      if ( doDF && dfBefore && (ui->ffOnEachScan->isChecked() || ! currentScan ) ) {
        acquireDF(seriesName + "BEFORE", Shutter1A::OPENED);
        if (stopMe) goto onEngineExit;
      }
      if ( doBG  && bgBefore && (ui->ffOnEachScan->isChecked() || ! currentScan ) ) {
        acquireBG(seriesName + "BEFORE");
        if (stopMe) goto onEngineExit;
      }

      const double speed = ui->rotSpeed->value();
      const double accTime = thetaMotor->motor()->getAcceleration();
      const double accTravel = speed * accTime / 2;
      const double rotDir = copysign(1,thetaRange);
      const double addTravel = 2*rotDir*accTravel +
          (doTriggCT ? 2.0 : 0.0);

      prepareDetector("SAMPLE_"+seriesName+"T", totalProjections + doAdd);
      if (stopMe) goto onEngineExit;

      if (doTriggCT) {
        thetaMotor->motor()->wait_stop();
        if (stopMe) goto onEngineExit;
        const double curpos = thetaMotor->motor()->getUserPosition();
        tct->setStartPosition(curpos, true);
        tct->setStep( thetaRange / ui->scanProjections->value(), true);
        tct->setNofTrigs(totalProjections + doAdd, true);
        det->setPeriod(0);
        det->setHardwareTriggering(true);
      } else {
        det->setPeriod( qAbs(thetaRange) / (totalProjections * speed) );
      }

      if ( ! ongoingSeries || ! currentScan || doTriggCT ) {

        thetaMotor->motor()->wait_stop();
        if (stopMe) goto onEngineExit;

        // 2 coefficient in the below string is required to guarantee that the
        // motor has accelerated to the normal speed before the acquisition starts.
        thetaMotor->motor()->goRelative( -addTravel, QCaMotor::STOPPED);
        if (stopMe) goto onEngineExit;

        setMotorSpeed(thetaMotor, speed);
        if (stopMe) goto onEngineExit;

        if ( ! doTriggCT ) { // should be started after tct and det
          thetaMotor->motor()->goRelative( thetaRange + addTravel );
          if (stopMe) goto onEngineExit;
          // accTravel/speed in the below string is required to compensate the coefficient 2
          // two strings above.
          // qtWait(1000*(accTime + accTravel/speed));
          usleep(1000000*(accTime + accTravel/speed));

          if (stopMe) goto onEngineExit;
        }

      }

      det->start();
      if (doTriggCT) {
        tct->start(true);
        thetaMotor->motor()->goRelative( thetaRange + addTravel );
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
      det->waitWritten();
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
          acquireDF(seriesName + "AFTER", Shutter1A::OPENED);
          if (stopMe) goto onEngineExit;
        }

      }



    }

    if (stopMe) goto onEngineExit;
    ui->postScanScript->execute();
    currentProjection = -1;
    if (stopMe) goto onEngineExit;

    currentScan++;
    if ( ! ui->checkSerial->isChecked() )
      timeToStop = true;
    else if ( ui->endNumber->isChecked() )
      timeToStop = currentScan >= totalScans;
    else if (ui->endTime->isChecked())
      timeToStop = inCTtime.elapsed() + scanDelay >= scanTime;
    else if (ui->endCondition->isChecked())
      timeToStop = ui->conditionScript->execute();
    else
      timeToStop = true;

    if ( ! timeToStop ) {
      if (doSerial  && serialMotor->motor()->isConnected()) {
        const double goal = ui->serialPositionsList->isVisible() ?
              ui->serialPositionsList->item(currentScan, 0)->text().toDouble() :
              serialStart + currentScan * ui->serialStep->value();


        serialMotor->motor()->goUserPosition(goal, QCaMotor::STARTED);
      }
      if (stopMe) goto onEngineExit;
      qtWait(this, SIGNAL(requestToStopAcquisition()), scanDelay);
    }
    if (stopMe) goto onEngineExit;

    if ( timeToStop && ! ui->stepAndShotMode->isChecked() && ! ui->ffOnEachScan->isChecked() ) {
      if ( doBG && bgAfter ) {
        acquireBG(seriesName + "AFTER");
        if (stopMe) goto onEngineExit;
      }
      if ( doDF && dfAfter ) {
        acquireDF(seriesName + "AFTER", Shutter1A::OPENED);
        if (stopMe) goto onEngineExit;
      }
    }


  } while ( ! timeToStop );

  ui->postRunScript->execute();
  /* sh1A->close(); /**/

onEngineExit:

  if (doSerial && serialMotor->motor()->isConnected()) {
    serialMotor->motor()->stop(QCaMotor::STOPPED);
    serialMotor->motor()->goUserPosition(serialStart, QCaMotor::STARTED);
  }
  thetaMotor->motor()->stop(QCaMotor::STOPPED);
  setMotorSpeed(thetaMotor, thetaSpeed);
  thetaMotor->motor()->goUserPosition(thetaStart, QCaMotor::STARTED);
  if ( doBG ) {
    bgMotor->motor()->stop(QCaMotor::STOPPED);
    bgMotor->motor()->goUserPosition(bgStart, QCaMotor::STARTED);
  }

  if ( logFile ) {
    if ( logFile->isWritable() ) {
      det->waitWritten();
      if ( accumulatedLog.size() )  { // smth left: sdkipped frames
        qDebug() << "Accamulated log is not empty in the end of the CT acquisition."
                    " Some frame names might be skipped.";
        while ( accumulatedLog.size() )
          nameToLog("<unknown>");
      }
      QString wrt = QDateTime::currentDateTime().toString("dd/MM/yyyy_hh:mm:ss.zzz");
      wrt += QString(" ") + ( stopMe ? "Interrupting" : "Finishing ") + "acquisition.\n\n" ;
      logFile->write(wrt.toAscii());
    }
    logFile->close();
    logFile=0;
  }

  foreach(QWidget * tab, ui->control->tabs())
        tab->setEnabled(true);
  currentScan = -1;
  currentProjection = -1;
  inCT = false;

  det->setName(".temp") ;
  det->setAutoSave(false);
  det->setImageMode(detimode);
  det->setTriggerMode(dettmode);


  QTimer::singleShot(0, this, SLOT(updateUi_thetaMotor()));
  QTimer::singleShot(0, this, SLOT(updateUi_bgMotor()));
  QTimer::singleShot(0, this, SLOT(updateUi_serialMotor()));
  QTimer::singleShot(0, this, SLOT(updateUi_loopMotor()));
  QTimer::singleShot(0, this, SLOT(updateUi_subLoopMotor()));
  QTimer::singleShot(0, this, SLOT(updateUi_dynoMotor()));
  QTimer::singleShot(0, this, SLOT(updateUi_dyno2Motor()));
  QTimer::singleShot(0, this, SLOT(updateUi_detector()));
  QTimer::singleShot(0, this, SLOT(updateUi_shutterStatus()));
  QTimer::singleShot(0, this, SLOT(updateUi_expPath()));


}



