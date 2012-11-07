#include <QDebug>
#include <QFileDialog>
#include <QVector>
#include <QProcess>
#include <QtCore>
#include <QSettings>
#include <QFile>

#include "additional_classes.h"

#include "ctgui_mainwindow.h"

#include "ui_ctgui_mainwindow.h"
#include "ui_ctgui_variables.h"


static void restoreMotor(QCaMotor* mot, double pos, double speed=0.0) {

  if ( ! mot || ! mot->isConnected() )
    return;

  mot->stop();
  mot->wait_stop();

  if ( speed >= 0.0  &&  mot->getNormalSpeed() != speed ) {
    mot->setNormalSpeed(speed);
    mot->waitUpdated<double>
        (".VELO", speed, &QCaMotor::getNormalSpeed, 200);
  }

  if (mot->getUserPosition() != pos)
    mot->goUserPosition(pos, QCaMotor::STARTED);

}

static int evalExpr(const QString & templ, QString & result) {

  QProcess proc;
  proc.start("/bin/sh -c \"eval echo -n " + templ + "\"");
  proc.waitForFinished();

  if ( proc.exitCode()  )
    result = proc.readAllStandardError();
  else
    result = proc.readAllStandardOutput();
  return proc.exitCode();


}

QHash<QString,QString> MainWindow::envDesc;
const QString MainWindow::storedState=QDir::homePath()+"/.ctgui";
const bool MainWindow::inited = MainWindow::init();
const QIcon MainWindow::badIcon = QIcon(":/warn.svg");
const QIcon MainWindow::goodIcon = QIcon();

MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  isLoadingState(false),
  ui(new Ui::MainWindow),
  hui(new Ui::HelpDialog),
  hDialog(new QDialog(this, Qt::Tool)),
  det(new Detector(this)),
  inAcquisition(false),
  inDyno(false),
  inMulti(false),
  inCT(false),
  readyToStartCT(false),
  stopMe(true)
{

  ui->setupUi(this);
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


  hui->setupUi(hDialog);

  insertVariableIntoMe=0;

  QPushButton * save = new QPushButton("Save");
  save->setFlat(true);
  connect(save, SIGNAL(clicked()), SLOT(saveConfiguration()));
  ui->statusBar->addPermanentWidget(save);

  QPushButton * load = new QPushButton("Load");
  load->setFlat(true);
  connect(load, SIGNAL(clicked()), SLOT(loadConfiguration()));
  ui->statusBar->addPermanentWidget(load);

  QCheckBox * showVariables = new QCheckBox("Show variables");
  connect(showVariables, SIGNAL(toggled(bool)), hDialog, SLOT(setVisible(bool)));
  showVariables->setCheckState(Qt::Unchecked);
  ui->statusBar->addPermanentWidget(showVariables);

  // FilterFileTemplateEvent * fileTemplateEventFilter = new FilterFileTemplateEvent(this);
  // ui->sampleFile->installEventFilter(fileTemplateEventFilter);
  // ui->bgFile->installEventFilter(fileTemplateEventFilter);
  // ui->dfFile->installEventFilter(fileTemplateEventFilter);

  ui->startStop->setText("Start CT");
  ui->scanProgress->hide();
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

  connect(ui->expPath, SIGNAL(textChanged(QString)), SLOT(onWorkingDirChange()));
  connect(ui->browseExpPath, SIGNAL(clicked()), SLOT(onWorkingDirBrowse()));
  connect(ui->continiousMode, SIGNAL(toggled(bool)), SLOT(onAcquisitionMode()));
  connect(ui->stepAndShotMode, SIGNAL(toggled(bool)), SLOT(onAcquisitionMode()));
  connect(ui->checkSerial, SIGNAL(toggled(bool)), SLOT(onSerialCheck()));
  connect(ui->checkFF, SIGNAL(toggled(bool)), SLOT(onFFcheck()));
  connect(ui->checkDyno, SIGNAL(toggled(bool)), SLOT(onDynoCheck()));
  connect(ui->checkMulti, SIGNAL(toggled(bool)), SLOT(onMultiCheck()));
  // connect(ui->sampleFile, SIGNAL(textChanged(QString)), SLOT(onSampleFile()));
  // connect(ui->bgFile, SIGNAL(textChanged(QString)), SLOT(onBGfile()));
  // connect(ui->dfFile, SIGNAL(textChanged(QString)), SLOT(onDFfile()));
  // connect(fileTemplateEventFilter, SIGNAL(focusInOut()), SLOT(onFileTemplateFocus()));

  connect(ui->endConditionButtonGroup, SIGNAL(buttonClicked(int)), SLOT(onEndCondition()));
  connect(serialMotor->motor(), SIGNAL(changedUserPosition(double)),
          ui->serialCurrent, SLOT(setValue(double)));
  connect(serialMotor->motor(), SIGNAL(changedUnits(QString)), SLOT(onSerialMotor()));
  connect(serialMotor->motor(), SIGNAL(changedPrecision(int)), SLOT(onSerialMotor()));
  connect(serialMotor->motor(), SIGNAL(changedConnected(bool)), SLOT(onSerialMotor()));
  connect(serialMotor->motor(), SIGNAL(changedPv()), SLOT(onSerialMotor()));
  connect(serialMotor->motor(), SIGNAL(changedDescription(QString)), SLOT(onSerialMotor()));
  connect(serialMotor->motor(), SIGNAL(changedUserLoLimit(double)), SLOT(onSerialMotor()));
  connect(serialMotor->motor(), SIGNAL(changedUserHiLimit(double)), SLOT(onSerialMotor()));
  connect(serialMotor->motor(), SIGNAL(changedMoving(bool)), SLOT(onSerialMotor()));
  connect(ui->serialStep, SIGNAL(valueChanged(double)), SLOT(onSerialStep()));

  connect(ui->scanRange, SIGNAL(valueChanged(double)), SLOT(onScanRange()));
  connect(thetaMotor->motor(), SIGNAL(changedUserPosition(double)),
          ui->scanCurrent, SLOT(setValue(double)));
  connect(thetaMotor->motor(), SIGNAL(changedUnits(QString)), SLOT(onThetaMotor()));
  connect(thetaMotor->motor(), SIGNAL(changedPrecision(int)), SLOT(onThetaMotor()));
  connect(thetaMotor->motor(), SIGNAL(changedConnected(bool)), SLOT(onThetaMotor()));
  connect(thetaMotor->motor(), SIGNAL(changedPv()), SLOT(onThetaMotor()));
  connect(thetaMotor->motor(), SIGNAL(changedDescription(QString)), SLOT(onThetaMotor()));
  connect(thetaMotor->motor(), SIGNAL(changedUserLoLimit(double)), SLOT(onThetaMotor()));
  connect(thetaMotor->motor(), SIGNAL(changedUserHiLimit(double)), SLOT(onThetaMotor()));
  connect(thetaMotor->motor(), SIGNAL(changedNormalSpeed(double)), SLOT(onThetaMotor()));
  connect(thetaMotor->motor(), SIGNAL(changedMaximumSpeed(double)), SLOT(onThetaMotor()));
  connect(thetaMotor->motor(), SIGNAL(changedMoving(bool)), SLOT(onThetaMotor()));
  connect(ui->scanProjections, SIGNAL(valueChanged(int)), SLOT(onProjections()));
  connect(ui->rotSpeed, SIGNAL(valueChanged(double)), SLOT(onRotSpeed()));

  connect(ui->nofBGs, SIGNAL(valueChanged(int)), SLOT(onNofBG()));
  connect(ui->bgTravel, SIGNAL(valueChanged(double)), SLOT(onBGtravel()));
  connect(bgMotor->motor(), SIGNAL(changedUserPosition(double)),
          ui->bgCurrent, SLOT(setValue(double)));
  connect(bgMotor->motor(), SIGNAL(changedUserPosition(double)), SLOT(onBGtravel()));
  connect(bgMotor->motor(), SIGNAL(changedUnits(QString)), SLOT(onBGmotor()));
  connect(bgMotor->motor(), SIGNAL(changedPrecision(int)), SLOT(onBGmotor()));
  connect(bgMotor->motor(), SIGNAL(changedConnected(bool)), SLOT(onBGmotor()));
  connect(bgMotor->motor(), SIGNAL(changedPv()), SLOT(onBGmotor()));
  connect(bgMotor->motor(), SIGNAL(changedDescription(QString)), SLOT(onBGmotor()));
  connect(bgMotor->motor(), SIGNAL(changedUserLoLimit(double)), SLOT(onBGmotor()));
  connect(bgMotor->motor(), SIGNAL(changedUserHiLimit(double)), SLOT(onBGmotor()));
  connect(bgMotor->motor(), SIGNAL(changedMoving(bool)), SLOT(onBGmotor()));
  connect(ui->nofDFs, SIGNAL(valueChanged(int)), SLOT(onNofDF()));
  connect(sh1A, SIGNAL(stateChanged(Shutter1A::State)), SLOT(onShutterStatus()));
  connect(sh1A, SIGNAL(enabledChanged(bool)), SLOT(onShutterStatus()));
  connect(sh1A, SIGNAL(connectionChanged(bool)), SLOT(onShutterStatus()));

  connect(ui->testMulti, SIGNAL(clicked()), SLOT(onLoopTest()));
  connect(loopMotor->motor(), SIGNAL(changedUserPosition(double)),
          ui->loopCurrent, SLOT(setValue(double)));
  connect(loopMotor->motor(), SIGNAL(changedUnits(QString)), SLOT(onLoopMotor()));
  connect(loopMotor->motor(), SIGNAL(changedPrecision(int)), SLOT(onLoopMotor()));
  connect(loopMotor->motor(), SIGNAL(changedConnected(bool)), SLOT(onLoopMotor()));
  connect(loopMotor->motor(), SIGNAL(changedPv()), SLOT(onLoopMotor()));
  connect(loopMotor->motor(), SIGNAL(changedDescription(QString)), SLOT(onLoopMotor()));
  connect(loopMotor->motor(), SIGNAL(changedUserLoLimit(double)), SLOT(onLoopMotor()));
  connect(loopMotor->motor(), SIGNAL(changedUserHiLimit(double)), SLOT(onLoopMotor()));
  connect(loopMotor->motor(), SIGNAL(changedMoving(bool)), SLOT(onLoopMotor()));
  connect(ui->loopStep, SIGNAL(valueChanged(double)), SLOT(onLoopStep()));

  connect(ui->subLoop, SIGNAL(toggled(bool)), SLOT(onSubLoop()));
  connect(subLoopMotor->motor(), SIGNAL(changedUserPosition(double)),
          ui->subLoopCurrent, SLOT(setValue(double)));
  connect(subLoopMotor->motor(), SIGNAL(changedUnits(QString)), SLOT(onSubLoopMotor()));
  connect(subLoopMotor->motor(), SIGNAL(changedPrecision(int)), SLOT(onSubLoopMotor()));
  connect(subLoopMotor->motor(), SIGNAL(changedConnected(bool)), SLOT(onSubLoopMotor()));
  connect(subLoopMotor->motor(), SIGNAL(changedPv()), SLOT(onSubLoopMotor()));
  connect(subLoopMotor->motor(), SIGNAL(changedDescription(QString)), SLOT(onSubLoopMotor()));
  connect(subLoopMotor->motor(), SIGNAL(changedUserLoLimit(double)), SLOT(onSubLoopMotor()));
  connect(subLoopMotor->motor(), SIGNAL(changedUserHiLimit(double)), SLOT(onSubLoopMotor()));
  connect(subLoopMotor->motor(), SIGNAL(changedMoving(bool)), SLOT(onSubLoopMotor()));
  connect(ui->subLoopStep, SIGNAL(valueChanged(double)), SLOT(onSubLoopStep()));

  connect(ui->testDyno, SIGNAL(clicked()), SLOT(onDynoTest()));
  connect(dynoMotor->motor(), SIGNAL(changedUserPosition(double)),
          ui->dynoCurrent, SLOT(setValue(double)));
  connect(dynoMotor->motor(), SIGNAL(changedNormalSpeed(double)), SLOT(onDynoMotor()));
  connect(dynoMotor->motor(), SIGNAL(changedMaximumSpeed(double)), SLOT(onDynoMotor()));
  connect(dynoMotor->motor(), SIGNAL(changedUnits(QString)), SLOT(onDynoMotor()));
  connect(dynoMotor->motor(), SIGNAL(changedPrecision(int)), SLOT(onDynoMotor()));
  connect(dynoMotor->motor(), SIGNAL(changedConnected(bool)), SLOT(onDynoMotor()));
  connect(dynoMotor->motor(), SIGNAL(changedPv()), SLOT(onDynoMotor()));
  connect(dynoMotor->motor(), SIGNAL(changedDescription(QString)), SLOT(onDynoMotor()));
  connect(dynoMotor->motor(), SIGNAL(changedUserLoLimit(double)), SLOT(onDynoMotor()));
  connect(dynoMotor->motor(), SIGNAL(changedUserHiLimit(double)), SLOT(onDynoMotor()));
  connect(dynoMotor->motor(), SIGNAL(changedMoving(bool)), SLOT(onDynoMotor()));

  connect(ui->dyno2, SIGNAL(toggled(bool)), SLOT(onDyno2()));
  connect(dyno2Motor->motor(), SIGNAL(changedUserPosition(double)),
          ui->dyno2Current, SLOT(setValue(double)));
  connect(dyno2Motor->motor(), SIGNAL(changedNormalSpeed(double)), SLOT(onDyno2Motor()));
  connect(dyno2Motor->motor(), SIGNAL(changedMaximumSpeed(double)), SLOT(onDyno2Motor()));
  connect(dyno2Motor->motor(), SIGNAL(changedUnits(QString)), SLOT(onDyno2Motor()));
  connect(dyno2Motor->motor(), SIGNAL(changedPrecision(int)), SLOT(onDyno2Motor()));
  connect(dyno2Motor->motor(), SIGNAL(changedConnected(bool)), SLOT(onDyno2Motor()));
  connect(dyno2Motor->motor(), SIGNAL(changedPv()), SLOT(onDyno2Motor()));
  connect(dyno2Motor->motor(), SIGNAL(changedDescription(QString)), SLOT(onDyno2Motor()));
  connect(dyno2Motor->motor(), SIGNAL(changedUserLoLimit(double)), SLOT(onDyno2Motor()));
  connect(dyno2Motor->motor(), SIGNAL(changedUserHiLimit(double)), SLOT(onDyno2Motor()));
  connect(dyno2Motor->motor(), SIGNAL(changedMoving(bool)), SLOT(onDyno2Motor()));
  connect(ui->dynoDirectionLock, SIGNAL(toggled(bool)), SLOT(onDynoDirectionLock()));
  connect(ui->dynoSpeedLock, SIGNAL(toggled(bool)), SLOT(onDynoSpeedLock()));

  connect(ui->detSelection, SIGNAL(currentIndexChanged(int)), SLOT(onDetectorSelection()));
  connect(ui->testDetector, SIGNAL(clicked()), SLOT(onDetectorTest()));
  connect(det, SIGNAL(connectionChanged(bool)), SLOT(onDetectorUpdate()));
  connect(det, SIGNAL(parameterChanged()), SLOT(onDetectorUpdate()));
  connect(det, SIGNAL(counterChanged(int)), ui->detProgress, SLOT(setValue(int)));
  connect(det, SIGNAL(nameChanged(QString)), ui->detFileName, SLOT(setText(QString)));
  connect(det, SIGNAL(lastNameChanged(QString)), ui->detFileLastName, SLOT(setText(QString)));
  connect(det, SIGNAL(templateChanged(QString)), ui->detFileTemplate, SLOT(setText(QString)));

  connect(ui->startStop, SIGNAL(clicked()), SLOT(onStartStop()));

  loadConfiguration(storedState);

  connect( ui->expName, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->expDesc, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->sampleDesc, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->expPath, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->aqModeButtonGroup, SIGNAL(buttonClicked(int)), SLOT(storeCurrentState()));
  connect( ui->checkSerial, SIGNAL(toggled(bool)), SLOT(storeCurrentState()));
  connect( ui->checkFF, SIGNAL(toggled(bool)), SLOT(storeCurrentState()));
  connect( ui->checkDyno, SIGNAL(toggled(bool)), SLOT(storeCurrentState()));
  connect( ui->checkMulti, SIGNAL(toggled(bool)), SLOT(storeCurrentState()));
  // connect( ui->sampleFile, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  // connect( ui->bgFile, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  // connect( ui->dfFile, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  // connect( ui->preRunScript, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->postRunScript, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->endConditionButtonGroup, SIGNAL(buttonClicked(int)), SLOT(storeCurrentState()));
  connect( ui->nofScans, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->scanTime, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->conditionScript, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->ongoingSeries, SIGNAL(toggled(bool)), SLOT(storeCurrentState()));
  connect( ui->ffOnEachScan, SIGNAL(toggled(bool)), SLOT(storeCurrentState()));
  connect( ui->scanDelay, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( serialMotor->motor(), SIGNAL(changedPv()), SLOT(storeCurrentState()));
  connect( ui->serialStep, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( thetaMotor->motor(), SIGNAL(changedPv()), SLOT(storeCurrentState()));
  connect( ui->scanRange, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->scanProjections, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->rotSpeed, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->scanAdd, SIGNAL(toggled(bool)), SLOT(storeCurrentState()));
  connect( ui->preScanScript, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->postScanScript, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->nofBGs, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->bgInterval, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( bgMotor->motor(), SIGNAL(changedPv()), SLOT(storeCurrentState()));
  connect( ui->bgTravel, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->nofDFs, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->dfInterval, SIGNAL(editingFinished()), SLOT(storeCurrentState()));
  connect( ui->multiBg, SIGNAL(toggled(bool)), SLOT(storeCurrentState()));
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

  //onFileTemplateFocus();
  onWorkingDirChange();
  onAcquisitionMode();
  onSerialCheck();
  onFFcheck();
  onDynoCheck();
  onMultiCheck();
  onDetectorUpdate();



  /*

  setEnv("FILE", ".temp.tif");
  setEnv("AQTYPE", "SINGLESHOT");
  setEnv("DFTYPE", "NONE");
  setEnv("DFBEFORECOUNT", 0);
  setEnv("DFAFTERCOUNT", 0);
  setEnv("DFCOUNT", 0);
  setEnv("GSCANPOS", 0);
  setEnv("BGCOUNT", 0);
  setEnv("GTRANSPOS", 0);
  setEnv("PCOUNT", 0);
  setEnv("LOOPCOUNT", 0);
  setEnv("GLOOPPOS", 0);
  setEnv("SUBLOOPCOUNT", 0);
  setEnv("GSUBLOOPPOS", 0);
  setEnv("COUNT", 0);


  connect( (QApplication*) QCoreApplication::instance(), SIGNAL(focusChanged(QWidget *, QWidget*)),
          SLOT(onFocusChange(QWidget*,QWidget*)));
  connect(hui->table, SIGNAL(cellClicked(int,int)),
          SLOT(onHelpClecked(int)));

*/


}


MainWindow::~MainWindow()
{
  delete ui;
}


bool MainWindow::init() {
  Q_INIT_RESOURCE(ctgui);
  envDesc["GTRANSPOS"] = "Directed position of the translation stage.";
  envDesc["FILE"] = "Name of the last aquired image.";
  envDesc["BGFILE"] = "Name of the last aquired background image.";
  envDesc["SAMPLEFILE"] = "Name of the last aquired image.";
  envDesc["AQTYPE"] = "Role of the image being acquired: DARKCURRENT, BACKGROUND, SAMPLE or SINGLESHOT.";
  envDesc["DFTYPE"] = "Role of the dark field image being acquired: BEFORE or AFTER the scan.";
  envDesc["DFBEFORECOUNT"] = "Count of the dark field image in the before-scan series.";
  envDesc["DFAFTERCOUNT"] = "Count of the dark field image in the after-scan series.";
  envDesc["DFCOUNT"] = "Count of the dark field image.";
  envDesc["GSCANPOS"] = "Directed position of the sample rotation stage.";
  envDesc["BGCOUNT"] = "Count of the background image.";
  envDesc["PCOUNT"] = "Sample projection counter.";
  envDesc["LOOPCOUNT"] = "Count of the image in the loop series.";
  envDesc["GLOOPPOS"] = "Directed position of the loop stage.";
  envDesc["SUBLOOPCOUNT"] = "Count of the image in the sub-loop series.";
  envDesc["GSUBLOOPPOS"] = "Directed position of the sub-loop stage.";
  envDesc["COUNT"] = "Total image counter (excluding dark field).";
  envDesc["SCANMOTORUNITS"] = "Units of the rotation stage.";
  envDesc["TRANSMOTORUNITS"] = "Units of the translation stage.";
  envDesc["LOOPMOTORUNITS"] = "Units of the loop stage.";
  envDesc["SUBLOOPMOTORUNITS"] = "Units of the sub-loop stage.";
  envDesc["SCANMOTORPREC"] = "Precsicion of the rotation stage.";
  envDesc["TRANSMOTORPREC"] = "Precsicion of the translation stage.";
  envDesc["LOOPMOTORPREC"] = "Precsicion of the loop stage.";
  envDesc["SUBLOOPMOTORPREC"] = "Precsicion of the sub-loop stage.";
  envDesc["EXPPATH"] = "Working directory.";
  envDesc["EXPNAME"] = "Experiment name.";
  envDesc["EXPDESC"] = "Description of the experiment.";
  envDesc["SAMPLEDESC"] = "Sample description.";
  envDesc["PEOPLE"] = "Experimentalists list.";
  envDesc["SCANMOTORPV"] = "EPICS PV of the rotation stage.";
  envDesc["SCANMOTORDESC"] = "EPICS descriptiopn of the rotation stage.";
  envDesc["SCANPOS"] = "Actual position of the rotation stage.";
  envDesc["SCANRANGE"] = "Travel range of the rotation stage in the scan.";
  envDesc["SCANSTEP"] = "Rotation step between two projections.";
  envDesc["PROJECTIONS"] = "Total number of projections.";
  envDesc["SCANSTART"] = "Start point of the rotation stage.";
  envDesc["SCANEND"] = "End point of rotation stage.";
  envDesc["TRANSMOTORPV"] = "EPICS PV of the translation stage.";
  envDesc["TRANSMOTORDESC"] = "EPICS descriptiopn of the translation stage.";
  envDesc["TRANSPOS"] = "Actual position of the translation stage.";
  envDesc["TRANSINTERVAL"] = "Interval (number of projections) between two background acquisitions.";
  envDesc["TRANSN"] = "Total number of backgrounds in the scan.";
  envDesc["TRANSDIST"] = "Travel of the translation stage.";
  envDesc["TRANSIN"] = "Position of the translation stage in-beam.";
  envDesc["TRANSOUT"] = "Position of the translation stage out of the beam.";
  envDesc["DFBEFORE"] = "Total number of dark current acquisitions before the scan.";
  envDesc["DFAFTER"] = "Total number of dark current acquisitions after the scan.";
  envDesc["DFTOTAL"] = "Total number of dark current acquisitions.";
  envDesc["LOOPMOTORPV"] = "EPICS PV of the loop stage.";
  envDesc["LOOPMOTORDESC"] = "EPICS descriptiopn of the loop stage.";
  envDesc["LOOPRANGE"] = "Total travel range of the loop stage.";
  envDesc["LOOPSTEP"] = "Single step between two acquisitions in the loop series.";
  envDesc["LOOPSTART"] = "Start point of the loop stage.";
  envDesc["LOOPEND"] = "End point of the loop stage.";
  envDesc["SUBLOOPMOTORPV"] = "EPICS PV of the sub-loop stage.";
  envDesc["SUBLOOPMOTORDESC"] = "EPICS descriptiopn of the sub-loop stage.";
  envDesc["SUBLOOPRANGE"] = "Total travel range of the sub-loop stage.";
  envDesc["SUBLOOPSTEP"] = "Single step between two acquisitions in the sub-loop series.";
  envDesc["SUBLOOPSTART"] = "Start point of the sub-loop stage.";
  envDesc["SUBLOOPEND"] = "End point of the sub-loop stage.";
  envDesc["LOOPPOS"] = "Actual position of the loop stage.";
  envDesc["LOOPN"] = "Total number of aqcuisitions in the loop series.";
  envDesc["SUBLOOPPOS"] = "Actual position of the sub-loop stage.";
  envDesc["SUBLOOPN"] = "Total number of aqcuisitions in the sub-loop series.";
  envDesc["DYNOPOS"] = "Actual position of the stage controlling dynamic shot.";
  envDesc["DYNOMOTORUNITS"] = "Units of the motor controlling dynamic shot.";
  envDesc["DYNOMOTORPREC"] = "Precsicion of the motor controlling dynamic shot.";
  envDesc["DYNOMOTORPV"] = "EPICS PV of the stage controlling dynamic shot";
  envDesc["DYNOMOTORDESC"] = "EPICS descriptiopn of the stage controlling dynamic shot.";
  envDesc["DYNORANGE"] = "Total travel range of the stage controlling dynamic shot.";
  envDesc["DYNOSTART"] = "Start point of the stage controlling dynamic shot.";
  envDesc["DYNOEND"] = "End point of the stage controlling dynamic shot.";
  envDesc["DYNO2POS"] = "Actual position of the second stage controlling dynamic shot.";
  envDesc["DYNO2MOTORUNITS"] = "Units of the motor controlling dynamic shot.";
  envDesc["DYNO2MOTORPREC"] = "Precsicion of the motor controlling dynamic shot.";
  envDesc["DYNO2MOTORPV"] = "EPICS PV of the second stage controlling dynamic shot";
  envDesc["DYNO2MOTORDESC"] = "EPICS descriptiopn of the second stage controlling dynamic shot.";
  envDesc["DYNO2RANGE"] = "Total travel range of the second stage controlling dynamic shot.";
  envDesc["DYNO2START"] = "Start point of the second stage controlling dynamic shot.";
  envDesc["DYNO2END"] = "End point of the second stage controlling dynamic shot.";

  return true;

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
  config.setValue("acquisitionmode", ui->stepAndShotMode->isChecked() ?
                    "step-and-shot" : "continious");
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
    setInConfig(config, "time", ui->scanTime);
    setInConfig(config, "script", ui->conditionScript);
    setInConfig(config, "ongoingseries",ui->ongoingSeries);
    setInConfig(config, "ffoneachscan",ui->ffOnEachScan);
    setInConfig(config, "scandelay",ui->scanDelay);
    setInConfig(config, "serialmotor",serialMotor);
    setInConfig(config, "serialstep",ui->serialStep);
    config.endGroup();
  }

  config.beginGroup("scan");
  setInConfig(config, "motor", thetaMotor);
  setInConfig(config, "range", ui->scanRange);
  setInConfig(config, "steps", ui->scanProjections);
  setInConfig(config, "add", ui->scanAdd);
  setInConfig(config, "prescan", ui->preScanScript);
  setInConfig(config, "postscan", ui->postScanScript);
  setInConfig(config, "rotationspeed", ui->rotSpeed);
  config.endGroup();

  if (ui->checkFF->isChecked()) {
    config.beginGroup("flatfield");
    setInConfig(config, "bgs", ui->nofBGs);
    setInConfig(config, "bginterval", ui->bgInterval);
    setInConfig(config, "motor", bgMotor);
    setInConfig(config, "bgtravel", ui->bgTravel);
    setInConfig(config, "dfs", ui->nofDFs);
    setInConfig(config, "dfinterval", ui->dfInterval);
    config.endGroup();
  }

  if (ui->checkMulti->isChecked()) {
    config.beginGroup("loop");
    setInConfig(config, "ffforeach", ui->multiBg);
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
  if (config.contains("acquisitionmode")) {
    if ( config.value("acquisitionmode").toString() == "step-and-shot" )
      ui->stepAndShotMode->setChecked(true);
    else if ( config.value("acquisitionmode").toString() == "continious" )
      ui->continiousMode->setChecked(true);
  }
  restoreFromConfig(config, "doserialscans", ui->checkSerial);
  restoreFromConfig(config, "doflatfield", ui->checkFF);
  restoreFromConfig(config, "dodyno", ui->checkDyno);
  restoreFromConfig(config, "domulti", ui->checkMulti);
  // restoreFromConfig(config, "sampleFile", ui->sampleFile);
  // restoreFromConfig(config, "backgroundFile", ui->bgFile);
  // restoreFromConfig(config, "darkfieldFile", ui->dfFile);
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
    restoreFromConfig(config, "time", ui->scanTime);
    restoreFromConfig(config, "script", ui->conditionScript);
    restoreFromConfig(config, "ongoingseries",ui->ongoingSeries);
    restoreFromConfig(config, "ffoneachscan",ui->ffOnEachScan);
    restoreFromConfig(config, "scandelay",ui->scanDelay);
    restoreFromConfig(config, "serialmotor",serialMotor);
    restoreFromConfig(config, "serialstep",ui->serialStep);
    config.endGroup();
  }

  if (groups.contains("scan")) {
    config.beginGroup("scan");
    restoreFromConfig(config, "motor", thetaMotor);
    restoreFromConfig(config, "range", ui->scanRange);
    restoreFromConfig(config, "steps", ui->scanProjections);
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
    restoreFromConfig(config, "motor", bgMotor);
    restoreFromConfig(config, "bgtravel", ui->bgTravel);
    restoreFromConfig(config, "dfs", ui->nofDFs);
    restoreFromConfig(config, "dfinterval", ui->dfInterval);
    config.endGroup();
  }

  if (ui->checkMulti->isChecked() && groups.contains("loop")) {
    config.beginGroup("loop");
    restoreFromConfig(config, "ffforeach", ui->multiBg);
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


/*
void MainWindow::onHelpClecked(int row) {
  if ( ! insertVariableIntoMe )
    return;
  QString inS = hui->table->item(row, 0)->text();
  if (  qobject_cast<QLineEdit*>(insertVariableIntoMe) )
    ( (QLineEdit*) insertVariableIntoMe )->insert(inS);
  else if (  qobject_cast<QPlainTextEdit*>(insertVariableIntoMe) )
    ( (QPlainTextEdit*) insertVariableIntoMe )->insertPlainText(inS);
  QApplication::setActiveWindow(this);
}
*/




void MainWindow::onWorkingDirChange() {

  const QString txt = ui->expPath->text();

  bool isOK;
  if (QDir::isAbsolutePath(txt)) {
    QFileInfo fi(txt);
    isOK = fi.isDir() && fi.isWritable();
  } else
    isOK = false;

  if (isOK)
    QDir::setCurrent(txt);

  check(ui->expPath, isOK);

}

void MainWindow::onWorkingDirBrowse() {
  ui->expPath->setText(
        QFileDialog::getExistingDirectory(0, "Working directory", QDir::currentPath()) );
}

void MainWindow::onAcquisitionMode() {
  const bool sasMode = ui->stepAndShotMode->isChecked();
  if (!sasMode) {
    ui->checkDyno->setChecked(false);
    ui->checkMulti->setChecked(false);
  }
  ui->checkDyno->setVisible(sasMode);
  ui->checkMulti->setVisible(sasMode);

  ui->ffOnEachScan->setVisible(!sasMode);
  ui->speedsLine->setVisible(!sasMode);
  ui->speedsLabel->setVisible(!sasMode);
  ui->timesLabel->setVisible(!sasMode);
  ui->rotSpeedLabel->setVisible(!sasMode);
  ui->rotSpeed->setVisible(!sasMode);
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

  onProjections();
  //onSampleFile();
}

void MainWindow::onSerialCheck() {
  static const QString tabSerialLabel =
      ui->control->tabText(ui->control->indexOf(ui->tabSerial));
  if (ui->checkSerial->isChecked())
    ui->control->insertTab(ui->control->indexOf(ui->tabScan), ui->tabSerial, tabSerialLabel);
  else
    ui->control->removeTab(ui->control->indexOf(ui->tabSerial));
  onSerialMotor();
  onEndCondition();
}

void MainWindow::onFFcheck() {
  static const QString tabFFLabel =
      ui->control->tabText(ui->control->indexOf(ui->tabFF));
  if (ui->checkFF->isChecked()) {
    ui->control->insertTab(ui->control->indexOf(ui->tabScan)+1, ui->tabFF, tabFFLabel);
    // ui->dfFile->setVisible(true);
    // ui->dfFileLabel->setVisible(true);
    // ui->bgFile->setVisible(true);
    // ui->bgFileLabel->setVisible(true);
  } else {
    ui->control->removeTab(ui->control->indexOf(ui->tabFF));
    // ui->dfFile->setVisible(false);
    // ui->dfFileLabel->setVisible(false);
    // ui->bgFile->setVisible(false);
    // ui->bgFileLabel->setVisible(false);
  }
  //onBGfile();
  //onDFfile();
  onBGmotor();
  onShutterStatus();
}

void MainWindow::onDynoCheck() {
  static const QString tabDynoLabel =
      ui->control->tabText(ui->control->indexOf(ui->tabDyno));
  if (ui->stepAndShotMode->isChecked() && ui->checkDyno->isChecked()) {
    int idx = qMax( ui->control->indexOf(ui->tabFF), ui->control->indexOf(ui->tabScan) );
    ui->control->insertTab(idx+1, ui->tabDyno, tabDynoLabel);
  } else
    ui->control->removeTab(ui->control->indexOf(ui->tabDyno));
  onDynoMotor();
  onDyno2();
}

void MainWindow::onMultiCheck() {
  static const QString tabMultiLabel =
      ui->control->tabText(ui->control->indexOf(ui->tabMulti));
  if (ui->stepAndShotMode->isChecked() && ui->checkMulti->isChecked())
    ui->control->insertTab(ui->control->indexOf(ui->tabDetector), ui->tabMulti, tabMultiLabel);
  else
    ui->control->removeTab(ui->control->indexOf(ui->tabMulti));
  onLoopMotor();
  onSubLoop();
}

// void MainWindow::onSampleFile() {
//   QString example;
//   bool isOK = ! evalExpr(ui->sampleFile->text(), example) &&
//       ! example.isEmpty();
//   ui->exampleFile->setText(example);
//   check(ui->sampleFile, isOK);
// }

// void MainWindow::onBGfile() {
//   QString example;
//   bool isOK =
//       ! ui->checkFF->isChecked() ||
//       ( ! evalExpr(ui->bgFile->text(), example) &&
//         ! example.isEmpty() );
//   ui->exampleFile->setText(example);
//   check(ui->bgFile, isOK);
// }

// void MainWindow::onDFfile() {
//   QString example;
//   bool isOK =
//       ! ui->checkFF->isChecked() ||
//       ( ! evalExpr(ui->dfFile->text(), example) &&
//         ! example.isEmpty() );
//   ui->exampleFile->setText(example);
//   check(ui->dfFile, isOK);
// }

// void MainWindow::onFileTemplateFocus() {
//   bool showSample = true;
//   if (ui->sampleFile->hasFocus())
//     onSampleFile();
//   else if (ui->bgFile->hasFocus())
//     onBGfile();
//   else if (ui->dfFile->hasFocus())
//     onDFfile();
//   else
//     showSample=false;
//   ui->exampleFile->setShown(showSample);
// }


void MainWindow::onEndCondition() {
  ui->nofScans->setVisible(ui->endNumber->isChecked());
  ui->nofScansLabel->setVisible(ui->endNumber->isChecked());
  ui->scanTimeWdg->setVisible(ui->endTime->isChecked());
  ui->scanTimeLabel->setVisible(ui->endTime->isChecked());
  ui->conditionScript->setVisible(ui->endCondition->isChecked());
  ui->conditionScriptLabel->setVisible(ui->endCondition->isChecked());
}

void MainWindow::onSerialMotor() {

  if (inCT)
    return;

  check(serialMotor->setupButton(),
        ! ui->checkSerial->isChecked() ||
        serialMotor->motor()->getPv().isEmpty() ||
        ( serialMotor->motor()->isConnected() &&
          ! serialMotor->motor()->isMoving() ) );
  check(ui->serialCurrent,
        ! ui->checkSerial->isChecked() ||
        serialMotor->motor()->getPv().isEmpty() ||
        ! serialMotor->motor()->getLimitStatus() );

  setEnv("SERIALMOTORPV", serialMotor->motor()->getPv());
  onSerialStep();

  ui->serialStep->setEnabled(serialMotor->motor()->isConnected());
  if ( ! serialMotor->motor()->isConnected() ) {
    ui->serialCurrent->setText("no link");
    return;
  }
  ui->serialCurrent->setValue(serialMotor->motor()->getUserPosition());

  setEnv("SERIALMOTORDESC", serialMotor->motor()->getDescription());

  const QString units = serialMotor->motor()->getUnits();
  setEnv("SERIALMOTORUNITS", units);
  ui->serialCurrent->setSuffix(units);
  ui->serialStep->setSuffix(units);

  const int prec = serialMotor->motor()->getPrecision();
  setEnv("SERIALMOTORPREC", prec);
  ui->serialCurrent->setDecimals(prec);
  ui->serialStep->setDecimals(prec);

}

void MainWindow::onSerialStep() {
  check(ui->serialStep,
        ! ui->checkSerial->isChecked() ||
        ! serialMotor->motor()->isConnected() ||
        ui->serialStep->value() != 0.0);
}


void MainWindow::onScanRange() {
  const double range = ui->scanRange->value();
  ui->scanStep->setValue(range/ui->scanProjections->value());
  setEnv("SCANRANGE", range);
  check(ui->scanRange, range != 0.0);
  onRotSpeed();
}

void MainWindow::onProjections() {
  const int projs =  ui->scanProjections->value();
  const int minInterval = ui->stepAndShotMode->isChecked() ?
        0 : projs - 1;
  ui->bgInterval->setRange(minInterval, projs);
  ui->dfInterval->setRange(minInterval, projs);
  onRotSpeed();
}

void MainWindow::onRotSpeed() {
  check( ui->rotSpeed,
         ! ui->continiousMode->isChecked() ||
         ui->rotSpeed->value() > 0 );
  if ( ui->rotSpeed->value() > 0 && ui->scanRange->value() != 0.0 ) {
    const double step = ui->scanRange->value() /
        (ui->scanProjections->value() * ui->rotSpeed->value());
    ui->stepTime->setValue(step);
    ui->expOverStep->setValue( det->exposure() / step );
  } else {
    ui->stepTime->setText("");
    ui->expOverStep->setText("");
  }
}


void MainWindow::onThetaMotor() {

  if (inCT)
    return;

  check(thetaMotor->setupButton(),
        thetaMotor->motor()->isConnected() &&
        ! thetaMotor->motor()->isMoving() );
  check(ui->scanCurrent,
        ! thetaMotor->motor()->getLimitStatus() );

  setEnv("SCANMOTORPV", thetaMotor->motor()->getPv());
  onScanRange();

  if (!thetaMotor->motor()->isConnected()) {
    ui->scanCurrent->setText("no link");
    ui->rotSpeed->setValue(0.0);
    return;
  }
  ui->scanCurrent->setValue(thetaMotor->motor()->getUserPosition());
  if (ui->rotSpeed->value()==0.0)
    ui->rotSpeed->setValue(thetaMotor->motor()->getNormalSpeed());

  setEnv("SCANMOTORDESC", thetaMotor->motor()->getDescription());

  const QString units = thetaMotor->motor()->getUnits();
  setEnv("SCANMOTORUNITS", units);
  ui->scanRange->setSuffix(units);
  ui->scanCurrent->setSuffix(units);
  ui->scanStep->setSuffix(units);
  ui->rotSpeed->setSuffix(units+"/s");
  ui->normalSpeed->setSuffix(units+"/s");
  ui->maximumSpeed->setSuffix(units+"/s");

  const int prec = thetaMotor->motor()->getPrecision();
  setEnv("SCANMOTORPREC", prec);
  ui->scanCurrent->setDecimals(prec);
  ui->scanStep->setDecimals(prec);
  ui->scanRange->setDecimals(prec);
  ui->rotSpeed->setDecimals(prec);
  ui->normalSpeed->setDecimals(prec);
  ui->maximumSpeed->setDecimals(prec);

  ui->normalSpeed->setValue(thetaMotor->motor()->getNormalSpeed());
  ui->maximumSpeed->setValue(thetaMotor->motor()->getMaximumSpeed());
  ui->rotSpeed->setMaximum(thetaMotor->motor()->getMaximumSpeed());

}


void MainWindow::onNofBG() {

  const int nofbgs = ui->nofBGs->value();
  ui->bgInterval->setEnabled(nofbgs);
  ui->bgTravel->setEnabled(nofbgs);
  bgMotor->setupButton()->setEnabled(nofbgs);

  check(bgMotor->setupButton(),
        ! ui->checkFF->isChecked() ||
        ! nofbgs ||
        ( bgMotor->motor()->isConnected() &&
          ! bgMotor->motor()->isMoving() ) );
  check(ui->bgCurrent,
        ! ui->checkFF->isChecked() ||
        ! nofbgs ||
        ! bgMotor->motor()->getLimitStatus() );

  onBGtravel();

}

void MainWindow::onBGmotor() {

  if (inCT)
    return;

  setEnv("BGMOTORPV", bgMotor->motor()->getPv());
  onNofBG();

  if (!bgMotor->motor()->isConnected()) {
    ui->bgCurrent->setText("no link");
    return;
  }
  ui->bgCurrent->setValue(bgMotor->motor()->getUserPosition());

  setEnv("BGMOTORDESC", bgMotor->motor()->getDescription());

  const QString units = bgMotor->motor()->getUnits();
  setEnv("BGMOTORUNITS", units);
  ui->bgCurrent->setSuffix(units);
  ui->bgTravel->setSuffix(units);

  const int prec = bgMotor->motor()->getPrecision();
  setEnv("BGMOTORPREC", prec);
  ui->bgCurrent->setDecimals(prec);
  ui->bgTravel->setDecimals(prec);

}

void MainWindow::onBGtravel() {
  check(ui->bgTravel,
        ! ui->nofBGs->value() ||
        ui->bgTravel->value() != 0.0 );
}

void MainWindow::onNofDF() {
  const int nofdfs = ui->nofDFs->value();
  ui->dfInterval->setEnabled(nofdfs);
  check( ui->nofDFs, ! nofdfs || sh1A->isEnabled() );
}

void MainWindow::onShutterStatus() {

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

  if (inCT)
    return;

  onNofDF();

}



void MainWindow::onLoopMotor() {

  if (inCT)
    return;

  check(loopMotor->setupButton(),
        ! ui->checkMulti->isChecked() ||
        loopMotor->motor()->getPv().isEmpty() ||
        ( loopMotor->motor()->isConnected() &&
          ! loopMotor->motor()->isMoving() ) );
  check(ui->loopCurrent,
        ! ui->checkMulti->isChecked() ||
        loopMotor->motor()->getPv().isEmpty() ||
        ! loopMotor->motor()->getLimitStatus() );

  if (inMulti)
    return;

  setEnv("LOOPMOTORPV", loopMotor->motor()->getPv());
  onLoopStep();

  ui->loopStep->setEnabled(loopMotor->motor()->isConnected());
  if ( ! loopMotor->motor()->isConnected() ) {
    ui->loopCurrent->setText("no link");
    return;
  }
  ui->loopCurrent->setValue(loopMotor->motor()->getUserPosition());

  setEnv("LOOPMOTORDESC", loopMotor->motor()->getDescription());

  const QString units = loopMotor->motor()->getUnits();
  setEnv("LOOPMOTORUNITS", units);
  ui->loopCurrent->setSuffix(units);
  ui->loopStep->setSuffix(units);

  const int prec = loopMotor->motor()->getPrecision();
  setEnv("LOOPMOTORPREC", prec);
  ui->loopCurrent->setDecimals(prec);
  ui->loopStep->setDecimals(prec);

}

void MainWindow::onLoopStep() {
  check(ui->loopStep,
        ! ui->checkMulti->isChecked() ||
        ! loopMotor->motor()->isConnected() ||
        ui->loopStep->value() != 0.0 );
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
  onSubLoopMotor();
}

void MainWindow::onSubLoopMotor() {

  if (inCT)
    return;

  check(subLoopMotor->setupButton(),
        ! ui->checkMulti->isChecked() ||
        ! ui->subLoop->isChecked() ||
        subLoopMotor->motor()->getPv().isEmpty() ||
        ( subLoopMotor->motor()->isConnected() &&
          ! subLoopMotor->motor()->isMoving() ) );
  check(ui->subLoopCurrent,
        ! ui->checkMulti->isChecked() ||
        ! ui->subLoop->isChecked() ||
        subLoopMotor->motor()->getPv().isEmpty() ||
        ! subLoopMotor->motor()->getLimitStatus() );

  if (inMulti)
    return;

  setEnv("SUBLOOPMOTORPV", subLoopMotor->motor()->getPv());
  onSubLoopStep();

  ui->subLoopStep->setEnabled(subLoopMotor->motor()->isConnected());
  if ( ! subLoopMotor->motor()->isConnected() ) {
    ui->subLoopCurrent->setText("no link");
    return;
  }
  ui->subLoopCurrent->setValue(subLoopMotor->motor()->getUserPosition());

  setEnv("SUBLOOPMOTORDESC", subLoopMotor->motor()->getDescription());

  const QString units = subLoopMotor->motor()->getUnits();
  setEnv("SUBLOOPMOTORUNITS", units);
  ui->subLoopCurrent->setSuffix(units);
  ui->subLoopStep->setSuffix(units);

  const int prec = subLoopMotor->motor()->getPrecision();
  setEnv("SUBLOOPMOTORPREC", prec);
  ui->subLoopCurrent->setDecimals(prec);
  ui->subLoopStep->setDecimals(prec);

}

void MainWindow::onSubLoopStep() {
  check(ui->subLoopStep,
        ! ui->checkMulti->isChecked() ||
        ! ui->subLoop->isChecked() ||
        ! subLoopMotor->motor()->isConnected() ||
        ui->subLoopStep->value() != 0.0 );
}

void MainWindow::onLoopTest() {
  if (inMulti) {
    stopMulti();
  } else {
    ui->testMulti->setText("Stop");
    inMulti=true;
    check(ui->testMulti, false);
    acquireMulti(".test");
    check(ui->testMulti, true);
    inMulti=false;
    ui->testMulti->setText("Test");
  }
}


void MainWindow::onDynoMotor() {

  if ( inCT )
    return;

  check(dynoMotor->setupButton(),
        ! ui->checkDyno->isChecked() ||
        ( dynoMotor->motor()->isConnected() &&
          ! dynoMotor->motor()->isMoving() ) );
  check(ui->dynoCurrent,
        ! ui->checkDyno->isChecked() ||
        ! dynoMotor->motor()->getLimitStatus() );

  if ( inMulti || inDyno )
    return;

  setEnv("DYNOMOTORPV", dynoMotor->motor()->getPv());
  check(dynoMotor->setupButton(),
        ! ui->checkDyno->isChecked() || dynoMotor->motor()->isConnected() );
  check(ui->dynoNormalSpeed,
        ! dynoMotor->motor()->isConnected() ||
        dynoMotor->motor()->getNormalSpeed()>=0.0);

  if ( ! dynoMotor->motor()->isConnected() ) {
    ui->dynoCurrent->setText("no link");
    return;
  }
  ui->dynoCurrent->setValue(dynoMotor->motor()->getUserPosition());

  if (ui->dynoSpeed->value()==0.0)
    ui->dynoSpeed->setValue(dynoMotor->motor()->getNormalSpeed());
  ui->dynoSpeed->setMaximum(dynoMotor->motor()->getMaximumSpeed());
  ui->dynoNormalSpeed->setValue(dynoMotor->motor()->getNormalSpeed());

  setEnv("DYNOMOTORDESC", dynoMotor->motor()->getDescription());

  const QString units = dynoMotor->motor()->getUnits();
  setEnv("DYNOMOTORUNITS", units);
  ui->dynoCurrent->setSuffix(units);
  ui->dynoNormalSpeed->setSuffix(units+"/s");
  ui->dynoSpeed->setSuffix(units+"/s");

  const int prec = dynoMotor->motor()->getPrecision();
  setEnv("DYNOMOTORPREC", prec);
  ui->dynoCurrent->setDecimals(prec);
  ui->dynoNormalSpeed->setDecimals(prec);
  ui->dynoSpeed->setDecimals(prec);

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
  onDyno2Motor();
  onDynoSpeedLock();
  onDynoDirectionLock();
}

void MainWindow::onDyno2Motor() {

  if ( inCT )
    return;

  check(dyno2Motor->setupButton(),
        ! ui->checkDyno->isChecked() ||
        ! ui->dyno2->isChecked() ||
        ( dyno2Motor->motor()->isConnected() &&
          ! dyno2Motor->motor()->isMoving() ) );
  check(ui->dyno2Current,
        ! ui->checkDyno->isChecked() ||
        ! ui->dyno2->isChecked() ||
        ! dyno2Motor->motor()->getLimitStatus() );

  if ( inMulti || inDyno )
    return;

  setEnv("DYNO2MOTORPV", dyno2Motor->motor()->getPv());
  check(dyno2Motor->setupButton(),
        ! ui->checkDyno->isChecked() ||
        ! ui->dyno2->isChecked() ||
        dyno2Motor->motor()->isConnected() );
  check(ui->dyno2NormalSpeed,
        ! dyno2Motor->motor()->isConnected() ||
        dyno2Motor->motor()->getNormalSpeed()>=0.0);

  if ( ! dyno2Motor->motor()->isConnected() ) {
    ui->dyno2Current->setText("no link");
    return;
  }
  ui->dyno2Current->setValue(dyno2Motor->motor()->getUserPosition());

  if (ui->dyno2Speed->value()==0.0)
    ui->dyno2Speed->setValue(dyno2Motor->motor()->getNormalSpeed());
  ui->dyno2Speed->setMaximum(dyno2Motor->motor()->getMaximumSpeed());
  ui->dyno2NormalSpeed->setValue(dyno2Motor->motor()->getNormalSpeed());


  setEnv("DYNO2MOTORDESC", dyno2Motor->motor()->getDescription());

  const QString units = dyno2Motor->motor()->getUnits();
  setEnv("DYNO2MOTORUNITS", units);
  ui->dyno2Current->setSuffix(units);
  ui->dyno2NormalSpeed->setSuffix(units+"/s");
  ui->dyno2Speed->setSuffix(units+"/s");

  const int prec = dyno2Motor->motor()->getPrecision();
  setEnv("DYNO2MOTORPREC", prec);
  ui->dyno2Current->setDecimals(prec);
  ui->dyno2NormalSpeed->setDecimals(prec);
  ui->dyno2Speed->setDecimals(prec);

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

void MainWindow::onDynoTest() {
  if (inDyno) {
    stopDyno();
  } else {
    ui->testDyno->setText("Stop");
    inDyno=true;
    check(ui->testDyno, false);
    acquireDyno(".test.tif");
    check(ui->testDyno, true);
    inDyno=false;
    ui->testDyno->setText("Test");
  }
}



void MainWindow::onDetectorSelection() {
  const QString currentText = ui->detSelection->currentText();
  if (currentText.isEmpty()) {
    det->setCamera(Detector::NONE);
    return;
  } else {
    foreach (Detector::Camera cam , Detector::knownCameras)
      if (currentText==Detector::cameraName(cam)) {
        det->setCamera(cam);
        return;
      }
  }
  det->setCamera(currentText);
}


void MainWindow::onDetectorUpdate() {

  if ( ! det->isConnected() )
    ui->detStatus->setText("no link");
  else if ( det->isAcquiring() )
    ui->detStatus->setText("acquiring");
  else
    ui->detStatus->setText("idle");

  check (ui->detStatus, det->isConnected() &&
         ! det->isAcquiring() );

  if (  det->isConnected() ) {
    onRotSpeed(); // to update exp/step
    ui->exposureInfo->setValue(det->exposure());
    ui->detExposure->setValue(det->exposure());
    ui->detPeriod->setValue(det->period());
    ui->detTriggerMode->setText(det->triggerModeString());
    ui->detImageMode->setText(det->imageModeString());
    ui->detTotalImages->setValue(det->number());
    ui->detProgress->setMaximum(det->number());
    ui->detPath->setText(det->filePath());
    ui->detPath->setStyleSheet( det->pathExists() ? "" : "color: rgb(255, 0, 0);");
    ui->detFileTemplate->setText(det->fileTemplate());
    ui->detFileName->setText(det->fileName());
  } else {
    ui->exposureInfo->setText("");
    ui->detExposure->setText("");
    ui->detPeriod->setText("");
    ui->detTriggerMode->setText("");
    ui->detImageMode->setText("");
    ui->detTotalImages->setText("");
    ui->detPath->setText("");
    ui->detPath->setText("");
    ui->detFileTemplate->setText("");
    ui->detFileName->setText("");
  }

  updateDetectorProgress();

}

void MainWindow::updateDetectorProgress() {
  ui->detProgress->setVisible
      ( ( inAcquisition || det->isAcquiring() )  &&
        det->number() > 1 &&
        det->imageMode() == 1 );
}


void MainWindow::onDetectorTest() {
  if ( ! det->isConnected() )
    return;
  else if (inAcquisition) {
    stopDetector();
  } else {
    ui->testDetector->setText("Stop");
    inAcquisition=true;
    check(ui->testDetector, false);
    acquireDetector(".test.tif");
    check(ui->testDetector, true);
    inAcquisition=false;
    ui->testDetector->setText("Test");
  }
}


void MainWindow::setEnv(const char * var, const char * val) {

  setenv(var, val, 1);

  int found = -1, curRow=0, rowCount=hui->table->rowCount();
  while ( found < 0 && curRow < rowCount ) {
    if ( hui->table->item(curRow, 0)->text() == var )
      found = curRow;
    curRow++;
  }

  if (found < 0) {

    QString desc = envDesc.contains(var) ? envDesc[var] : "" ;

    hui->table->setSortingEnabled(false);
    hui->table->insertRow(rowCount);
    hui->table->setItem( rowCount, 0, new QTableWidgetItem( QString(var) ));
    hui->table->setItem( rowCount, 1, new QTableWidgetItem( QString(val) ));
    hui->table->setItem( rowCount, 2, new QTableWidgetItem(desc));
    hui->table->setSortingEnabled(true);

    hui->table->resizeColumnToContents(0);
    hui->table->resizeColumnToContents(1);
    hui->table->resizeColumnToContents(2);

  } else {
    hui->table->item(found, 1)->setText( QString(val) );
    hui->table->resizeColumnToContents(1);
  }

}






void MainWindow::check(QWidget * obj, bool status) {

  if ( ! obj )
    return;

  static const QString warnStyle = "background-color: rgba(255, 0, 0, 128);";
  obj->setStyleSheet( status  ?  ""  :  warnStyle );

  QWidget * tab = 0;
  if ( ! preReq.contains(obj) ) {

    int index=0;
    QWidget *t;
    while ( ! tab && (t = ui->control->widget(index++)) )
      if ( t->findChildren<const QObject*>().indexOf(obj) != -1 )
        tab = t;

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

    if (tab==ui->tabDyno)
      ui->testDyno->setEnabled( tabOK || inDyno );
    if (tab==ui->tabMulti)
      ui->testMulti->setEnabled( tabOK || inMulti);
    if (tab==ui->tabDetector)
      ui->testDetector->setEnabled( tabOK || inAcquisition );

    preReq[tab] = qMakePair(tabOK, (const QWidget*)0);
    ui->control->setTabIcon(ui->control->indexOf(tab),
                            tabOK ? goodIcon : badIcon);

  }

  readyToStartCT=status;
  if ( status )
    foreach ( ReqP tabel, preReq)
      if ( ! tabel.second )
        readyToStartCT &= tabel.first;
  ui->startStop->setEnabled(readyToStartCT);

}















bool MainWindow::prepareDetector(const QString & filename) {
  return
      ! filename.isEmpty() &&
      det->isConnected() &&
      det->setNumber(1) &&
      det->setName(filename) &&
      det->prepareForAcq() ;
}


void MainWindow::stopDetector() {
  stopMe=true;
  ui->preAqScript->stop();
  det->stop();
  ui->postAqScript->stop();
}

int MainWindow::acquireDetector(const QString & filename) {
  inAcquisition=true;
  stopMe=false;
  int execStatus = -1;
  if ( ! prepareDetector(filename) || stopMe )
    goto acquireDetectorExit;
  updateDetectorProgress();
  execStatus = ui->preAqScript->execute();
  if ( execStatus || stopMe )
    goto acquireDetectorExit;
  det->acquire();
  if ( ! stopMe )
    execStatus = ui->postAqScript->execute();

acquireDetectorExit:
  inAcquisition=false;
  updateDetectorProgress();
  return execStatus;

}


void MainWindow::stopDyno() {
  stopMe=true;
  dynoMotor->motor()->stop();
  dyno2Motor->motor()->stop();
  stopDetector();
}

int MainWindow::acquireDyno(const QString & filename) {

  if ( ! ui->checkDyno->isChecked() )
    return -1;

  int ret=-1;
  stopMe=false;
  inDyno=true;
  ui->dynoWidget->setEnabled(false);

  dynoMotor->motor()->wait_stop();
  if ( ui->dyno2->isChecked() )
    dyno2Motor->motor()->wait_stop();

  const double
      dStart = dynoMotor->motor()->getUserPosition(),
      d2Start = dyno2Motor->motor()->getUserPosition(),
      dSpeed = dynoMotor->motor()->getNormalSpeed(),
      d2Speed = dyno2Motor->motor()->getNormalSpeed();

  // two stopMes below are reqiuired for the case it changes while the detector is being prepared
  if ( stopMe || ! prepareDetector(filename) || stopMe )
    goto acquireDynoExit;

  restoreMotor( dynoMotor->motor(), dStart, ui->dynoSpeed->value() );
  if ( ui->dyno2->isChecked() )
    restoreMotor( dyno2Motor->motor(), d2Start, ui->dyno2Speed->value() );

  if (stopMe)
    goto acquireDynoExit;

  dynoMotor->motor()->goLimit( ui->dynoPos->isChecked() ? 1 : -1 );
  if ( ui->dyno2->isChecked() )
    dyno2Motor->motor()->goLimit( ui->dyno2Pos->isChecked() ? 1 : -1 );
  dynoMotor->motor()->wait_start();
  if ( ui->dyno2->isChecked() )
    dyno2Motor->motor()->wait_start();

  if (stopMe)
    goto acquireDynoExit;

  ret = acquireDetector();

  if ( dynoMotor->motor()->getLimitStatus() ||
       ( ui->dyno2->isChecked() && dyno2Motor->motor()->getLimitStatus() ) )
    ret = 1;

acquireDynoExit:

  restoreMotor( dynoMotor->motor(), dStart, dSpeed );
  if ( ui->dyno2->isChecked() )
    restoreMotor( dyno2Motor->motor(), d2Start, d2Speed ) ;

  ui->dynoWidget->setEnabled(true);
  inDyno=false;

  return ret;

}





void MainWindow::stopMulti() {
  stopMe=true;
  loopMotor->motor()->stop();
  subLoopMotor->motor()->stop();
  ui->checkDyno->isChecked() ? stopDyno() : stopDetector();
}

int MainWindow::acquireMulti(const QString &filetemplate) {

  const int
      loopNumber = ui->loopNumber->value(),
      subLoopNumber = ui->subLoop->isChecked() ? ui->subLoopNumber->value() : 1,
      loopDigs = QString::number(loopNumber).size(),
      subLoopDigs = QString::number(subLoopNumber).size();

  QStringList filelist;
  for ( int x = 0; x < loopNumber; x++)
    for ( int y = 0; y < subLoopNumber; y++)
      filelist << filetemplate +
                  QString(".%1").arg(x, loopDigs, 10, QChar('0')) +
                  ( ui->subLoop->isChecked()  ?
                      QString(".%1").arg(y, subLoopDigs, 10, QChar('0')) : "") +
                  ".tif";

  return acquireMulti(filelist);

}


int MainWindow::acquireMulti(const QStringList & filelist) {

  stopMe=false;

  if ( ! ui->checkMulti->isEnabled() ) {
    appendMessage(ERROR, "Images in the multi-shot mode cannot be acquired now.");
    return -1;
  }

  const int
      loopNumber = ui->loopNumber->value(),
      subLoopNumber = ui->subLoop->isChecked() ? ui->subLoopNumber->value() : 1;
  const bool
      moveLoop = loopMotor->motor()->isConnected(),
      moveSubLoop = ui->subLoop->isChecked() && subLoopMotor->motor()->isConnected();

  if ( filelist.size() < loopNumber * subLoopNumber ) {
    appendMessage(ERROR, "Names for the images in the loop are not provided.");
    return -1;
  }

  if (moveLoop)
    loopMotor->motor()->wait_stop();
  if (moveSubLoop)
    subLoopMotor->motor()->wait_stop();

  if (stopMe ||
      (moveLoop && loopMotor->motor()->getLimitStatus()) ||
      (moveSubLoop && subLoopMotor->motor()->getLimitStatus()) )
    return -1;

  int execStatus=-1;
  inMulti=true;
  ui->multiWidget->setEnabled(false);

  const QString progressFormat = QString("Multishot progress: %p% ; %v of %m") +
      ( ui->subLoop->isChecked() ? "(%1,%2 of %3,%4)" : "" );
  if ( ! ui->subLoop->isChecked() )
    ui->multiProgress->setFormat(progressFormat);
  ui->multiProgress->setMaximum(loopNumber*subLoopNumber);
  ui->multiProgress->setValue(0);
  ui->multiProgress->setVisible(true);

  const double
      lStart = loopMotor->motor()->getUserPosition(),
      slStart = subLoopMotor->motor()->getUserPosition();

  for ( int x = 0; x < loopNumber; x++) {

    for ( int y = 0; y < subLoopNumber; y++) {

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

      QString filename = filelist[y+x*subLoopNumber];
      execStatus = ui->checkDyno->isChecked() ?
            acquireDyno(filename) : acquireDetector(filename);

      if (ui->subLoop->isChecked())
        ui->multiProgress->setFormat( progressFormat
                                      .arg(x+1).arg(y+1)
                                      .arg(loopNumber).arg(subLoopNumber)  );
      ui->multiProgress->setValue( 1 + y + subLoopNumber * x );

      if (stopMe || execStatus )
        goto acquireMultiExit;

      if (moveSubLoop && y < subLoopNumber-1 )
        subLoopMotor->motor()->goUserPosition
            ( slStart + y * ui->subLoopStep->value(), QCaMotor::STARTED);

      if (stopMe) {
        execStatus = -1;
        goto acquireMultiExit;
      }

    }

    if (moveLoop && x < loopNumber-1)
      loopMotor->motor()->goUserPosition
          ( lStart + x * ui->loopStep->value(), QCaMotor::STARTED);
    if (moveSubLoop && x < loopNumber-1)
      subLoopMotor->motor()->goUserPosition
          ( slStart, QCaMotor::STARTED );
    if (stopMe) {
      execStatus = -1;
      goto acquireMultiExit;
    }

  }

acquireMultiExit:

  if (moveLoop)
    restoreMotor(loopMotor->motor(), lStart);
  if (moveSubLoop)
    restoreMotor(subLoopMotor->motor(), slStart);
  ui->multiWidget->setEnabled(true);
  ui->multiProgress->setVisible(false);
  inMulti=false;
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


void MainWindow::onStartStop() {
  if ( inCT ) {
    stopMe = true;
    serialMotor->motor()->stop();
    thetaMotor->motor()->stop();
    stopMulti();
    stopDyno();
    stopDetector();
    emit requestToStopAcquisition();
  } else {
    ui->startStop->setText("Stop CT");
    engine();
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

/*
QString MainWindow::setAqFileEnv(const QLineEdit * edt, const QString & var) {

  QString eres, expr;
  expr = edt->text();
  if ( expr.isEmpty() )
    expr = ui->sampleFile->text();
  evalExpr(expr, eres);

  int indexOfDot = eres.lastIndexOf('.');
  if ( indexOfDot == -1 )
    indexOfDot = eres.size();
  QString res = eres;
  int count = 0;

  while ( res.isEmpty() || QFile::exists(res) || listHasFile(res) ) {
    res = eres;
    res.insert(indexOfDot, "_(" + QString::number(++count) + ")");
  }

  setEnv(var, res);
  return res;

}
*/

/*
bool MainWindow::listHasFile(const QString & fn) {
  if (fn.isEmpty())
    return false;
  bool found=false;

  int count = 0;
  while ( ! found && count < scanList->rowCount() )
    found  =  scanList->item(count++, 3)->text() == fn;

  return found;
}
*/

/*
bool MainWindow::doIt(int count) {

  if ( count >= scanList->rowCount() )
    return false;
  if ( ! ui->selectiveScan->isChecked() )
    return true;
  return scanList->item(count,0)->checkState() == Qt::Checked;

  return true;
}
*/









void MainWindow::updateSeriesProgress(bool onTimer) {
  if ( ui->endTime->isChecked() ) {
    QString format = "Series progress: %p% "
        + (QTime(0, 0, 0, 0).addMSecs(inCTtime.elapsed())).toString() + " of " + ui->scanTime->time().toString()
        + " (scans complete: " + QString::number(currentScan) + ")";
    ui->serialProgress->setFormat(format);
    ui->serialProgress->setValue(inCTtime.elapsed());
    if (onTimer && inCT)
      QTimer::singleShot(100, this, SLOT(updateSeriesProgress()));
  } else {
    ui->serialProgress->setValue(currentScan);
  }
}

void MainWindow::engine () {

  if ( ! readyToStartCT || inCT )
    return;

  stopMe = false;
  inCT = true;
  for (int idx=0; idx<ui->control->count() ; idx++)
    ui->control->widget(idx)->setEnabled(false);

  const double
      serialStart =  serialMotor->motor()->getUserPosition(),
      bgStart =  bgMotor->motor()->getUserPosition(),
      thetaStart = thetaMotor->motor()->getUserPosition(),
      thetaRange = ui->scanRange->value();
  double thetaInSeriesStart = thetaStart;
  const bool
      doSerial = ui->checkSerial->isChecked(),
      doBG = ui->checkFF->isChecked() && ui->nofBGs->value(),
      doDF = ui->checkFF->isChecked() && ui->nofDFs->value(),
      doAdd = ui->scanAdd->isChecked(),
      ongoingSeries = doSerial && ui->ongoingSeries->isChecked();
  const int
      totalScans = doSerial ?
        ( ui->endNumber->isChecked() ? ui->nofScans->value() : 0 ) : 1 ,
      totalProjections = ui->scanProjections->value(),
      bgInterval = ui->bgInterval->value(),
      dfInterval = ui->dfInterval->value(),
      bgs = ui->nofBGs->value(),
      dfs = ui->nofDFs->value(),
      scanDelay = QTime(0, 0, 0, 0).msecsTo( ui->scanDelay->time() ),
      scanTime = QTime(0, 0, 0, 0).msecsTo( ui->scanTime->time() );

  ui->scanProgress->setMaximum(totalProjections + ( doAdd ? 1 : 0 ));
  if ( totalScans ) {
    ui->serialProgress->setMaximum(totalScans);
    ui->serialProgress->setFormat("Series progress. %p% (scans complete: %v of %m)");
  } else if ( ui->endTime->isChecked() ) {
    ui->serialProgress->setMaximum(scanTime);
  } else {
    ui->serialProgress->setMaximum(0);
    ui->serialProgress->setFormat("Series progress. Scans complete: %v.");
  }
  ui->serialProgress->setValue(0);
  ui->serialProgress->setVisible(true);

  inCTtime.restart();
  currentScan=0;
  bool timeToStop=false;
  updateSeriesProgress();

  ui->preRunScript->execute();
  if (stopMe) goto onEngineExit;

  do { // serial scanning

    qDebug() << "SERIES" << currentScan;

    if (doSerial  && serialMotor->motor()->isConnected())
      serialMotor->motor()->wait_stop();

    if (stopMe) goto onEngineExit;

    ui->preScanScript->execute();

    if (stopMe) goto onEngineExit;

    if (ui->stepAndShotMode->isChecked()) {

      int
          currentProjection = 0,
          beforeBG = 0,
          beforeDF = 0;

      ui->scanProgress->setValue(0);
      ui->scanProgress->setVisible(true);
      do {

        if (doDF && ! beforeDF) {
          qDebug() << "  DO DF";
          beforeDF = dfInterval-1;
          if (stopMe) goto onEngineExit;
        } else
          beforeDF--;

        if (doBG && ! beforeBG) {
          bgMotor->motor()->goUserPosition
              ( bgStart + ui->bgTravel->value(), QCaMotor::STOPPED);
          if (stopMe) goto onEngineExit;
          qDebug() << "  DO BG";
          if (stopMe) goto onEngineExit;
          bgMotor->motor()->goUserPosition(bgStart, QCaMotor::STOPPED);
          beforeBG = bgInterval-1;
          if (stopMe) goto onEngineExit;
        } else
          beforeBG--;

        thetaMotor->motor()->wait_stop();
        if ( ! currentProjection )
          thetaInSeriesStart = ongoingSeries ?
                thetaMotor->motor()->getUserPosition() :
                thetaStart;

        if (stopMe) goto onEngineExit;

        qDebug() << "  AQ PROJECTION" << currentProjection << thetaMotor->motor()->getUserPosition();
        if (stopMe) goto onEngineExit;

        ui->scanProgress->setValue(currentProjection+1);

        currentProjection++;
        if ( currentProjection < totalProjections ||  doAdd )
          thetaMotor->motor()->goUserPosition
              ( thetaInSeriesStart + thetaRange * currentProjection / totalProjections, QCaMotor::STARTED);
        else if ( ! ongoingSeries )
          thetaMotor->motor()->goUserPosition( thetaStart, QCaMotor::STARTED );
        if (stopMe) goto onEngineExit;

      } while ( currentProjection < totalProjections );

      if ( doBG && ! beforeBG ) {
        bgMotor->motor()->goUserPosition
            ( bgStart + ui->bgTravel->value(), QCaMotor::STOPPED);
        if (stopMe) goto onEngineExit;
        qDebug() << "  DO LAST BG";
        if (stopMe) goto onEngineExit;
        bgMotor->motor()->goUserPosition(bgStart, QCaMotor::STARTED);
        if (stopMe) goto onEngineExit;
      }
      if ( doDF && ! beforeDF ) {
        qDebug() << "  AQ LAST DF";
        if (stopMe) goto onEngineExit;
      }
      if ( doAdd ) {
        thetaMotor->motor()->wait_stop();
        if (stopMe) goto onEngineExit;
        qDebug() << "  AQ LAST PROJECTION" << thetaMotor->motor()->getUserPosition();
        ui->scanProgress->setValue(currentProjection+1);
        if (stopMe) goto onEngineExit;
        if ( ! ongoingSeries )
          thetaMotor->motor()->goUserPosition(thetaStart, QCaMotor::STARTED);
      }

    } else
      qDebug() << "CONT" ;

    if (stopMe) goto onEngineExit;

    ui->postScanScript->execute();

    ui->scanProgress->setVisible(false);

    if (stopMe) goto onEngineExit;

    currentScan++;
    if ( ui->endNumber->isChecked() )
      timeToStop = currentScan >= totalScans;
    else if (ui->endTime->isChecked())
      timeToStop = inCTtime.elapsed() + scanDelay >= scanTime;
    else if (ui->endCondition->isChecked())
      timeToStop = ui->conditionScript->execute();
    else
      timeToStop = true;

    updateSeriesProgress(false);

    if ( ! timeToStop ) {
      if (doSerial  && serialMotor->motor()->isConnected())
        serialMotor->motor()->goUserPosition
            ( serialStart + currentScan * ui->serialStep->value(), QCaMotor::STARTED);
      if (stopMe) goto onEngineExit;
      qtWait(this, SIGNAL(requestToStopAcquisition()), scanDelay);
    }
    if (stopMe) goto onEngineExit;

  } while ( ! timeToStop );

  ui->postRunScript->execute();
  //sh1A->close();
  qDebug() << "SH CLOSE";

onEngineExit:

  ui->serialProgress->setVisible(false);
  ui->scanProgress->setVisible(false);
  if (doSerial && serialMotor->motor()->isConnected())
    serialMotor->motor()->goUserPosition(serialStart, QCaMotor::STARTED);
  if ( ! ongoingSeries )
    thetaMotor->motor()->goUserPosition(thetaStart, QCaMotor::STARTED);
  if ( doBG )
    bgMotor->motor()->goUserPosition(bgStart, QCaMotor::STARTED);
  for (int idx=0; idx<ui->control->count() ; idx++)
    ui->control->widget(idx)->setEnabled(true);
  inCT = false;

  QTimer::singleShot(0, this, SLOT(onThetaMotor()));
  QTimer::singleShot(0, this, SLOT(onBGmotor()));
  QTimer::singleShot(0, this, SLOT(onSerialMotor()));
  QTimer::singleShot(0, this, SLOT(onLoopMotor()));
  QTimer::singleShot(0, this, SLOT(onSubLoopMotor()));
  QTimer::singleShot(0, this, SLOT(onDynoMotor()));
  QTimer::singleShot(0, this, SLOT(onDyno2Motor()));
  QTimer::singleShot(0, this, SLOT(onDetectorUpdate()));
  QTimer::singleShot(0, this, SLOT(onShutterStatus()));






  /*

  stopMe = false;

  if ( ! dryRun )
    setEngineStatus(Running);
  else
    setEngineStatus(Filling);

  motorsInitials.clear();
  if ( thetaMotor->motor()->isConnected() )
    motorsInitials[thetaMotor] = thetaMotor->motor()->getUserPosition();
  if ( bgMotor->motor()->isConnected() )
    motorsInitials[bgMotor] = bgMotor->motor()->getUserPosition();
  if ( loopMotor->motor()->isConnected() )
    motorsInitials[loopMotor] = loopMotor->motor()->getUserPosition();
  if ( subLoopMotor->motor()->isConnected() )
    motorsInitials[subLoopMotor] = subLoopMotor->motor()->getUserPosition();
  if ( dynoMotor->motor()->isConnected() )
    motorsInitials[dynoMotor] = dynoMotor->motor()->getUserPosition();
  if ( dyno2Motor->motor()->isConnected() )
    motorsInitials[dyno2Motor] = dyno2Motor->motor()->getUserPosition();

  const bool
      scanAdd = ui->scanAdd->isChecked(),
      doBg = transQty,
      doL = ui->checkMulti->isChecked(),
      doSL = doL && ui->subLoop->isChecked(),
      multiBg = doL && ! ui->multiBg->isChecked();
  const double
      start=ui->scanStart->value(),
      end=ui->scanEnd->value(),
      transIn = ui->transIn->value(),
      transOut = ui->bgOut->value(),
      lStart = ui->loopStart->value(),
      lEnd = ui->loopEnd->value(),
      slStart = ui->subLoopStart->value(),
      slEnd = ui->subLoopEnd->value();
  const int
      projs=ui->scanProjections->value(),
      tProjs = projs + ( scanAdd ? 1 : 0 ),
      bgInterval = ui->transInterval->value(),
      lN = doL ? ui->loopNumber->value() : 1,
      slN = doSL ? ui->subLoopNumber->value() : 1,
      expCount = ui->dfAfter->value() + ui->dfBefore->value() +
      tProjs * lN * slN  +
      ( doBg  ?  transQty * ( multiBg ? lN * slN : 1 )  :  0 );

  if (dryRun) {
    scanList->clear();
    QStringList listHeader;
    listHeader  << "Scheduled" << "Role" << "Position" << "File name";
    scanList->setHorizontalHeaderLabels ( listHeader );
  }

  ui->progressBar->setRange(0,expCount);
  ui->progressBar->reset();


  QString filename, aqErr, aqOut;;
  int
      count = 0,
      dfCount = 0,
      smCount = 0;

  if ( ! dryRun ) {
    onPreExec();
    if (ui->remoteCT->isChecked())
      createXLIfile("xli.config");
  }


  // Set starting environment to fake values - just for a case.

  setEnv("COUNT", 0);
  setEnv("DFCOUNT", 0);
  setEnv("DFBEFORECOUNT", 0);
  setEnv("GSCANPOS", start);
  setEnv("BGCOUNT", 0);
  setEnv("PCOUNT", 0);
  setEnv("GTRANSPOS", transIn);
  setEnv("LOOPCOUNT", 0);
  setEnv("GLOOPPOS", lStart);
  setEnv("SUBLOOPCOUNT", 0);
  setEnv("GSUBLOOPPOS", slStart);
  setEnv("DFAFTERCOUNT", 0);



  //
  // Dark current images before the scan
  //

  setEnv("AQTYPE", "DARKCURRENT");
  setEnv("DFTYPE", "BEFORE");


  if ( ! dryRun && ui->dfBefore->value() )
    sh1A->close(true);

  for ( int j = 0 ; j < ui->dfBefore->value() ; ++j )  {

    setEnv("DFBEFORECOUNT", j+1);
    setEnv("DFCOUNT", dfCount+1);

    if ( dryRun ) {
      filename = setAqFileEnv(ui->dfFile, "DFFILE");
      appendScanListRow(DF, 0, filename );
    } else if ( doIt(count) ) {
      filename = scanList->item(count,3)->text() ;
      setEnv("DFFILE", filename);
      if ( ! acquireDetector(filename) )
        scanList->item(count,0)->setCheckState(Qt::Unchecked);
    }

    QCoreApplication::processEvents();
    count++;
    dfCount++;
    ui->progressBar->setValue(count);
    if (stopMe) {
      setEngineStatus(Paused);
      return;
    }

  }



  //
  // Main scan
  //

  if (!dryRun)
    sh1A->open(true);

  int bgBeforeNext=0, bgCount=0;
  bool isBg, wasBg = false;
  double
      lastScan = thetaMotor->motor()->get(),
      lastTrans = bgMotor->motor()->get(),
      lastLoop = loopMotor->motor()->get(),
      lastSubLoop = subLoopMotor->motor()->get();

  // here ( doBg && scanAdd && ! wasBg ) needed to take last background
  // if ui->scanAdd->isChecked()
  while ( smCount < tProjs || ( doBg && scanAdd && ! wasBg ) ) {

    // here "cProj == tProj" needed to take last background if ui->scanAdd->isChecked()
    isBg  =  doBg && ( ! bgBeforeNext  ||  smCount == tProjs );

    double cPos = start + smCount * (end - start) / projs;
    setEnv("GSCANPOS", cPos);

    double transGoal;
    if ( isBg ) {
      transGoal = transOut;
      bgBeforeNext = bgInterval;
      wasBg = true;
      setEnv("AQTYPE", "BACKGROUND");
      setEnv("BGCOUNT", bgCount+1);
    } else {
      transGoal = transIn;
      bgBeforeNext--;
      wasBg = false;
      setEnv("AQTYPE", "SAMPLE");
      setEnv("PCOUNT", smCount+1);
    }
    setEnv("GTRANSPOS", transGoal);

    int loopN =  ( ! doL || (isBg && ! multiBg) )  ?  1  :  lN;
    int subLoopN = ( loopN <= 1 || ! doSL )  ?  1  :  slN;

    //
    // In loop
    //

    for ( int x = 0; x < loopN; x++) {

      setEnv("LOOPCOUNT", x+1);

      double loopPos = lStart   +  ( (loopN==1)  ?  0  :  x * (lEnd-lStart) / (loopN-1) );
      setEnv("GLOOPPOS", loopPos);

      //
      // In sub-loop
      //

      for ( int y = 0; y < subLoopN; y++) {

        setEnv("COUNT", count+1);
        setEnv("SUBLOOPCOUNT", y+1);

        double subLoopPos = slStart +  ( ( subLoopN == 1 )  ?
                                           0  :  y * (slEnd - slStart) / (subLoopN-1) );
        setEnv("GSUBLOOPPOS", subLoopPos);

        if ( dryRun ) {

          filename =  isBg  ?
                setAqFileEnv(ui->bgFile,     "BGFILE")  :
                setAqFileEnv(ui->sampleFile, "SAMPLEFILE");

          if (y)
            appendScanListRow(SLOOP, subLoopPos, filename);
          else if (x)
            appendScanListRow(LOOP, loopPos, filename);
          else if (isBg)
            appendScanListRow(BG, transOut, filename);
          else
            appendScanListRow(SAMPLE, cPos, filename);

        } else if ( doIt(count) ) {

          filename = scanList->item(count,3)->text();
          setEnv( isBg ? "BGFILE" : "SAMPLEFILE" , filename);

          if ( lastTrans != transGoal ) {
            appendMessage(CONTROL, "Moving translation motor to " + QString::number(transGoal) + ".");
            bgMotor->motor()->goUserPosition(transGoal, QCaMotor::STARTED);
            lastTrans = transGoal;
          }
          if ( ! isBg && lastScan != cPos) {
            appendMessage(CONTROL, "Moving scan motor to " + QString::number(cPos) + ".");
            thetaMotor->motor()->goUserPosition(cPos, QCaMotor::STARTED);
            lastScan=cPos;
          }
          if ( doL  &&  lastLoop != loopPos ) {
            appendMessage(CONTROL, "Moving loop motor to " + QString::number(loopPos) + ".");
            loopMotor->motor()->goUserPosition(loopPos, QCaMotor::STARTED);
            lastLoop = loopPos;
          }
          if ( doSL  &&  lastSubLoop != subLoopPos ) {
            appendMessage(CONTROL, "Moving sub-loop motor to " + QString::number(subLoopPos) + ".");
            subLoopMotor->motor()->goUserPosition(subLoopPos, QCaMotor::STARTED);
            lastSubLoop = subLoopPos;
          }

          //// qtWait(100);

          bgMotor->motor()->wait_stop();
          thetaMotor->motor()->wait_stop();
          loopMotor->motor()->wait_stop();
          subLoopMotor->motor()->wait_stop();

          if ( ! acquireDyno(filename) )
            scanList->item(count,0)->setCheckState(Qt::Unchecked);

        }

        QCoreApplication::processEvents();
        count++;
        ui->progressBar->setValue(count);
        if (stopMe) {
          setEngineStatus(Paused);
          return;
        }

      }

    }

    if ( ! isBg ) smCount++;
    else bgCount++;

  }


  //
  // Dark current images after the scan
  //

  setEnv("AQTYPE", "DARKCURRENT");
  setEnv("DFTYPE", "AFTER");

  if (!dryRun)
    sh1A->close(true);

  for ( int j = 0 ; j < ui->dfAfter->value() ; ++j ) {

    setEnv("DFAFTERCOUNT", j+1);
    setEnv("DFCOUNT", dfCount+1);

    if ( dryRun ) {
      filename = setAqFileEnv(ui->dfFile, "DFFILE");
      appendScanListRow(DF, 0, filename );
    } else if ( doIt(count) ) {
      filename = scanList->item(count,3)->text() ;
      setEnv("DFFILE", filename);
      if ( ! acquireDetector(filename) )
        scanList->item(count,0)->setCheckState(Qt::Unchecked);
    }

    QCoreApplication::processEvents();
    count++;
    dfCount++;
    ui->progressBar->setValue(count);
    if (stopMe) {
      setEngineStatus(Paused);
      return;
    }

  }

  if ( ! dryRun ) {
    onPostExec();
    appendMessage(CONTROL, "Scan is finished.");
  }
  setEngineStatus(Stopped);

  */

}


