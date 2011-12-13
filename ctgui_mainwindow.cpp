
#include "ctgui_mainwindow.h"
#include <QDebug>
#include <QFileDialog>
#include <QVector>
#include <QProcess>
#include <QtCore>
#include <QSettings>
#include <QFile>





static int evalExpr(const QString & templ, QString & result) {

  QProcess proc;
  proc.start("/bin/sh -c \"eval echo -n " + templ + "\"");
  proc.waitForFinished();

  if ( proc.exitCode()  )  {
    result = proc.readAllStandardError();
    return proc.exitCode();
  } else {
    result = proc.readAllStandardOutput();
    return 0;
  }



}

static int checkScript(const QString & script, QString & result) {

  QProcess proc;
  proc.start("/bin/sh -n " + script);
  proc.waitForFinished();

  if ( proc.exitCode() )  {
    result = proc.readAllStandardError();
    return proc.exitCode();
  } else {
    result = proc.readAllStandardOutput();
    return 0;
  }

}







QHash<QString,QString> MainWindow::envDesc;
QHash<MainWindow::Role,QString> MainWindow::roleName;
const bool MainWindow::inited = MainWindow::init();
const QString MainWindow::shutterPvBaseName = "SR08ID01PSS01:HU01A_BL_SHUTTER";
const QIcon MainWindow::badIcon = QIcon(":/warn.svg");
const QIcon MainWindow::goodIcon = QIcon();

MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::MainWindow),
  hui(new Ui::HelpDialog),
  hDialog(new QDialog(this, Qt::Tool)),
  shutterStatus(0),
  scanList(new QStandardItemModel(0,4,this)),
  proxyModel(new QSortFilterProxyModel(this)),
  transQty(0),
  stopMe(true),
  engineStatus(Stopped)
{


  aqExec = new QFile( QString(tmpnam(0)) + ".ctgui_aq", this) ;
  aqExec->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
  aqExec->setPermissions( QFile::ExeUser | QFile::ReadUser | QFile::WriteUser);

  preExec = new QFile( QString(tmpnam(0)) + ".ctgui_pre", this) ;
  preExec->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
  preExec->setPermissions( QFile::ExeUser | QFile::ReadUser | QFile::WriteUser);

  postExec = new QFile( QString(tmpnam(0)) + ".ctgui_post", this) ;
  postExec->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
  postExec->setPermissions( QFile::ExeUser | QFile::ReadUser | QFile::WriteUser);

  ui->setupUi(this);
  hui->setupUi(hDialog);

  widgetsNeededHelp
      << ui->preCommand
      << ui->postCommand
      << ui->detectorCommand
      << ui->sampleFile
      << ui->bgFile
      << ui->dfFile;
  insertVariableIntoMe=0;

  QCheckBox * showVariables = new QCheckBox("Show variables");
  connect(showVariables, SIGNAL(toggled(bool)), hDialog, SLOT(setVisible(bool)));
  showVariables->setCheckState(Qt::Unchecked);
  ui->statusBar->addPermanentWidget(showVariables);



  thetaMotor = new QCaMotorGUI;
  ui->scanLayout->addWidget(thetaMotor->setupButton(), 0, 0, 1, 1);
  bgMotor = new QCaMotorGUI;
  ui->ffLayout->addWidget(bgMotor->setupButton(), 3, 0, 1, 1);
  loopMotor = new QCaMotorGUI;
  ui->loopLayout->addWidget(loopMotor->setupButton(), 0, 0, 1, 1);
  subLoopMotor = new QCaMotorGUI;
  ui->subLoopLayout->addWidget(subLoopMotor->setupButton(), 0, 0, 1, 1);
  dynoMotor = new QCaMotorGUI;
  ui->dyno1Layout->addWidget(dynoMotor->setupButton(), 1, 0, 1, 1);
  dyno2Motor = new QCaMotorGUI;
  ui->dyno2Layout->addWidget(dyno2Motor->setupButton(), 0, 0, 1, 1);

  ui->dynoStarStopWg->setVisible(false); // until we have Qt-areaDetector and can trig the detector.

  proxyModel->setSourceModel(scanList);
  proxyModel->setFilterKeyColumn(-1);
  ui->scanView->setModel(proxyModel);
  setScanTable();

  motorsInitials[thetaMotor] = 0 ;
  motorsInitials[bgMotor] = 0 ;
  motorsInitials[loopMotor] = 0 ;
  motorsInitials[subLoopMotor] = 0 ;
  motorsInitials[dynoMotor] = 0 ;
  motorsInitials[dyno2Motor] = 0 ;

  opnSts = new QEpicsPv(shutterPvBaseName + "_OPEN_STS", ui->tabFF) ;
  opnSts->setObjectName("Shutter open status");
  clsSts = new QEpicsPv(shutterPvBaseName + "_CLOSE_STS", ui->tabFF) ;
  clsSts->setObjectName("Shutter close status");
  opnCmd = new QEpicsPv(shutterPvBaseName + "_OPEN_CMD", ui->tabFF) ;
  opnCmd->setObjectName("Shutter open command");
  clsCmd = new QEpicsPv(shutterPvBaseName + "_CLOSE_CMD", ui->tabFF) ;
  clsCmd->setObjectName("Shutter close command");

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

  connect(ui->startStop, SIGNAL(clicked()),
          SLOT(onStartStop()));
  connect(ui->assistant, SIGNAL(clicked()),
          SLOT(onAssistant()));

  connect(ui->browseExpPath, SIGNAL(clicked()),
          SLOT(onBrowseExpPath()));
  connect(ui->expPath, SIGNAL(textChanged(QString)),
          SLOT(onExpPathChanges()));
  connect(ui->saveConfig, SIGNAL(clicked()),
          SLOT(saveConfiguration()));

  connect(ui->loadConfig, SIGNAL(clicked()),
          SLOT(loadConfiguration()));
  connect(ui->expName, SIGNAL(textChanged(QString)),
          SLOT(onExpNameChanges()));
  connect(ui->expDesc, SIGNAL(textChanged()),
          SLOT(onExpDescChanges()));
  connect(ui->sampleDesc, SIGNAL(textChanged()),
          SLOT(onSampleDescChanges()));
  connect(ui->peopleList, SIGNAL(textChanged()),
          SLOT(onPeopleListChanges()));

  connect(ui->preCommand, SIGNAL(textChanged()),
          SLOT(onPreCommandChanges()));
  connect(ui->postCommand, SIGNAL(textChanged()),
          SLOT(onPostCommandChanges()));
  connect(ui->preExec, SIGNAL(clicked()),
          SLOT(onPreExec()));
  connect(ui->postExec, SIGNAL(clicked()),
          SLOT(onPostExec()));


  connect(thetaMotor->motor(), SIGNAL(changedUserPosition(double)),
          ui->currentPos, SLOT(setValue(double)));
      connect(thetaMotor->motor(), SIGNAL(changedUserPosition(double)),
          SLOT(onScanPosChanges()));
  connect(thetaMotor->motor(), SIGNAL(changedUnits(QString)),
          SLOT(setThetaUnits()));
  connect(thetaMotor->motor(), SIGNAL(changedPrecision(int)),
          SLOT  (setThetaPrec()));
  connect(thetaMotor->motor(), SIGNAL(changedConnected(bool)),
          SLOT(onThetaMotorChanges()));
  connect(thetaMotor->motor(), SIGNAL(changedPv()),
          SLOT(onThetaMotorChanges()));
  connect(thetaMotor->motor(), SIGNAL(changedDescription(QString)),
          SLOT(onThetaMotorChanges()));
  connect(thetaMotor->motor(), SIGNAL(changedUserLoLimit(double)),
          SLOT(onThetaMotorChanges()));
  connect(thetaMotor->motor(), SIGNAL(changedUserHiLimit(double)),
          SLOT(onThetaMotorChanges()));

  connect(ui->getScanStart, SIGNAL(clicked()),
          SLOT(getThetaStart()));
  connect(ui->scanRange, SIGNAL(valueChanged(double)),
          SLOT(onScanRangeChanges()));
  connect(ui->scanStep, SIGNAL(valueChanged(double)),
          SLOT(onScanStepChanges()));
  connect(ui->projections, SIGNAL(valueChanged(int)),
          SLOT(onProjectionsChanges()));
  connect(ui->scanStart, SIGNAL(valueChanged(double)),
          SLOT(onScanStartChanges()));
  connect(ui->scanEnd, SIGNAL(valueChanged(double)),
          SLOT(onScanEndChanges()));
  connect(ui->scanAdd, SIGNAL(toggled(bool)),
          SLOT(onScanAddChanges()));
  connect(ui->selectiveScan, SIGNAL(toggled(bool)),
          SLOT(onSelectiveScanChanges()));

  connect(ui->listFilter, SIGNAL(textChanged(QString)),
          SLOT(onFilterChanges(QString)));
  connect(ui->selectAll, SIGNAL(clicked()),
          SLOT(onSelectAll()));
  connect(ui->selectNone, SIGNAL(clicked()),
          SLOT(onSelectNone()));
  connect(ui->selectInvert, SIGNAL(clicked()),
          SLOT(onSelectInvert()));
  connect(ui->checkAll, SIGNAL(clicked()),
          SLOT(onCheckAll()));
  connect(ui->checkNone, SIGNAL(clicked()),
          SLOT(onCheckNone()));
  connect(ui->checkInvert, SIGNAL(clicked()),
          SLOT(onCheckInvert()));

  connect(bgMotor->motor(), SIGNAL(changedUserPosition(double)),
          ui->transCurrent, SLOT(setValue(double)));
  connect(bgMotor->motor(), SIGNAL(changedUserPosition(double)),
          SLOT(onTransPosChanges()));
  connect(bgMotor->motor(), SIGNAL(changedUnits(QString)),
          SLOT(setBgUnits()));
  connect(bgMotor->motor(), SIGNAL(changedPrecision(int)),
          SLOT(setBgPrec()));
  connect(bgMotor->motor(), SIGNAL(changedConnected(bool)),
          SLOT(onTransMotorChanges()));
  connect(bgMotor->motor(), SIGNAL(changedPv()),
          SLOT(onTransMotorChanges()));
  connect(bgMotor->motor(), SIGNAL(changedDescription(QString)),
          SLOT(onTransMotorChanges()));
  connect(bgMotor->motor(), SIGNAL(changedUserLoLimit(double)),
          SLOT(onTransMotorChanges()));
  connect(bgMotor->motor(), SIGNAL(changedUserHiLimit(double)),
          SLOT(onTransMotorChanges()));

  connect(ui->getTransIn, SIGNAL(clicked()),
          SLOT(getBgStart()));
  connect(ui->transInterval, SIGNAL(valueChanged(int)),
          SLOT(onTransIntervalChanges()));
  connect(ui->transDist, SIGNAL(valueChanged(double)),
          SLOT(onTransDistChanges()));
  connect(ui->transIn, SIGNAL(valueChanged(double)),
          SLOT(onTransInChanges()));
  connect(ui->transOut, SIGNAL(valueChanged(double)),
          SLOT(onTransOutChanges()));

  connect(ui->dfBefore, SIGNAL(valueChanged(int)),
          SLOT(onDfChanges()));
  connect(ui->dfAfter, SIGNAL(valueChanged(int)),
          SLOT(onDfChanges()));
  connect(opnSts, SIGNAL(connectionChanged(bool)),
          SLOT(onDfChanges()));
  connect(clsSts, SIGNAL(connectionChanged(bool)),
          SLOT(onDfChanges()));
  connect(opnCmd, SIGNAL(connectionChanged(bool)),
          SLOT(onDfChanges()));
  connect(clsSts, SIGNAL(connectionChanged(bool)),
          SLOT(onDfChanges()));
  connect(opnSts, SIGNAL(valueChanged(QVariant)),
          SLOT(onShutterChanges()));
  connect(clsSts, SIGNAL(valueChanged(QVariant)),
          SLOT(onShutterChanges()));
  connect(ui->shutterMan, SIGNAL(clicked()),
          SLOT(onShutterMan()));

  connect(ui->singleShot, SIGNAL(toggled(bool)),
          SLOT(onShotModeChanges()));
  connect(ui->multiShot, SIGNAL(toggled(bool)),
          SLOT(onShotModeChanges()));
  connect(ui->multiBg, SIGNAL(toggled(bool)),
          SLOT(setScanTable()));

  connect(loopMotor->motor(), SIGNAL(changedUserPosition(double)),
          ui->loopCurrent, SLOT(setValue(double)));
  connect(loopMotor->motor(), SIGNAL(changedUserPosition(double)),
          SLOT(onLoopPosChanges()));
  connect(loopMotor->motor(), SIGNAL(changedUnits(QString)),
          SLOT(setLoopUnits()));
  connect(loopMotor->motor(), SIGNAL(changedPrecision(int)),
          SLOT(setLoopPrec()));
  connect(loopMotor->motor(), SIGNAL(changedConnected(bool)),
          SLOT(onLoopMotorChanges()));
  connect(loopMotor->motor(), SIGNAL(changedPv()),
          SLOT(onLoopMotorChanges()));
  connect(loopMotor->motor(), SIGNAL(changedDescription(QString)),
          SLOT(onLoopMotorChanges()));
  connect(loopMotor->motor(), SIGNAL(changedUserLoLimit(double)),
          SLOT(onLoopMotorChanges()));
  connect(loopMotor->motor(), SIGNAL(changedUserHiLimit(double)),
          SLOT(onLoopMotorChanges()));

  connect(ui->getLoopStart, SIGNAL(clicked()),
          SLOT(getLoopStart()));
  connect(ui->loopRange, SIGNAL(valueChanged(double)),
          SLOT(onLoopRangeChanges()));
  connect(ui->loopNumber, SIGNAL(valueChanged(int)),
          SLOT(onLoopNumberChanges()));
  connect(ui->loopStep, SIGNAL(valueChanged(double)),
          SLOT(onLoopStepChanges()));
  connect(ui->loopStart, SIGNAL(valueChanged(double)),
          SLOT(onLoopStartChanges()));
  connect(ui->loopEnd, SIGNAL(valueChanged(double)),
          SLOT(onLoopEndChanges()));

  connect(ui->subLoop, SIGNAL(toggled(bool)),
          SLOT(onSubLoopChanges()));
  connect(subLoopMotor->motor(), SIGNAL(changedUserPosition(double)),
          ui->subLoopCurrent, SLOT(setValue(double)));
  connect(subLoopMotor->motor(), SIGNAL(changedUserPosition(double)),
          SLOT(onSubLoopPosChanges()));
  connect(subLoopMotor->motor(), SIGNAL(changedUnits(QString)),
          SLOT(setSubLoopUnits()));
  connect(subLoopMotor->motor(), SIGNAL(changedPrecision(int)),
          SLOT(setSubLoopPrec()));
  connect(subLoopMotor->motor(), SIGNAL(changedConnected(bool)),
          SLOT(onSubLoopMotorChanges()));
  connect(subLoopMotor->motor(), SIGNAL(changedPv()),
          SLOT(onSubLoopMotorChanges()));
  connect(subLoopMotor->motor(), SIGNAL(changedDescription(QString)),
          SLOT(onSubLoopMotorChanges()));
  connect(subLoopMotor->motor(), SIGNAL(changedUserLoLimit(double)),
          SLOT(onSubLoopMotorChanges()));
  connect(subLoopMotor->motor(), SIGNAL(changedUserHiLimit(double)),
          SLOT(onSubLoopMotorChanges()));

  connect(ui->getSubLoopStart, SIGNAL(clicked()),
          SLOT(getSubLoopStart()));
  connect(ui->subLoopRange, SIGNAL(valueChanged(double)),
          SLOT(onSubLoopRangeChanges()));
  connect(ui->subLoopNumber, SIGNAL(valueChanged(int)),
          SLOT(onSubLoopNumberChanges()));
  connect(ui->subLoopStep, SIGNAL(valueChanged(double)),
          SLOT(onSubLoopStepChanges()));
  connect(ui->subLoopStart, SIGNAL(valueChanged(double)),
          SLOT(onSubLoopStartChanges()));
  connect(ui->subLoopEnd, SIGNAL(valueChanged(double)),
          SLOT(onSubLoopEndChanges()));

  connect(ui->testMulti, SIGNAL(clicked()),
          SLOT(onAcquireMulti()));

  connect(dynoMotor->motor(), SIGNAL(changedUserPosition(double)),
          ui->dynoCurrent, SLOT(setValue(double)));
  connect(dynoMotor->motor(), SIGNAL(changedUserPosition(double)),
          SLOT(onDynoPosChanges()));
  connect(dynoMotor->motor(), SIGNAL(changedUnits(QString)),
          SLOT(setDynoUnits()));
  connect(dynoMotor->motor(), SIGNAL(changedPrecision(int)),
          SLOT  (setDynoPrec()));
  connect(dynoMotor->motor(), SIGNAL(changedConnected(bool)),
          SLOT(onDynoMotorChanges()));
  connect(dynoMotor->motor(), SIGNAL(changedPv()),
          SLOT(onDynoMotorChanges()));
  connect(dynoMotor->motor(), SIGNAL(changedDescription(QString)),
          SLOT(onDynoMotorChanges()));
  connect(dynoMotor->motor(), SIGNAL(changedUserLoLimit(double)),
          SLOT(onDynoMotorChanges()));
  connect(dynoMotor->motor(), SIGNAL(changedUserHiLimit(double)),
          SLOT(onDynoMotorChanges()));

  connect(ui->dynoShot, SIGNAL(toggled(bool)),
          SLOT(onDynoChanges()));
  connect(ui->getDynoStart, SIGNAL(clicked()),
          SLOT(getDynoStart()));
  connect(ui->dynoRange, SIGNAL(valueChanged(double)),
          SLOT(onDynoRangeChanges()));
  connect(ui->dynoStart, SIGNAL(valueChanged(double)),
          SLOT(onDynoStartChanges()));
  connect(ui->dynoEnd, SIGNAL(valueChanged(double)),
          SLOT(onDynoEndChanges()));

  connect(ui->dyno2Shot, SIGNAL(toggled(bool)),
          SLOT(onDyno2Changes()));
  connect(dyno2Motor->motor(), SIGNAL(changedUserPosition(double)),
          ui->dyno2Current, SLOT(setValue(double)));
  connect(dyno2Motor->motor(), SIGNAL(changedUserPosition(double)),
          SLOT(onDyno2PosChanges()));
  connect(dyno2Motor->motor(), SIGNAL(changedUnits(QString)),
          SLOT(setDyno2Units()));
  connect(dyno2Motor->motor(), SIGNAL(changedPrecision(int)),
          SLOT  (setDyno2Prec()));
  connect(dyno2Motor->motor(), SIGNAL(changedConnected(bool)),
          SLOT(onDyno2MotorChanges()));
  connect(dyno2Motor->motor(), SIGNAL(changedPv()),
          SLOT(onDyno2MotorChanges()));
  connect(dyno2Motor->motor(), SIGNAL(changedDescription(QString)),
          SLOT(onDyno2MotorChanges()));
  connect(dyno2Motor->motor(), SIGNAL(changedUserLoLimit(double)),
          SLOT(onDyno2MotorChanges()));
  connect(dyno2Motor->motor(), SIGNAL(changedUserHiLimit(double)),
          SLOT(onDyno2MotorChanges()));

  connect(ui->getDyno2Start, SIGNAL(clicked()),
          SLOT(getDyno2Start()));
  connect(ui->dyno2Range, SIGNAL(valueChanged(double)),
          SLOT(onDyno2RangeChanges()));
  connect(ui->dyno2Start, SIGNAL(valueChanged(double)),
          SLOT(onDyno2StartChanges()));
  connect(ui->dyno2End, SIGNAL(valueChanged(double)),
          SLOT(onDyno2EndChanges()));
  connect(ui->testDyno, SIGNAL(clicked()),
          SLOT(onAcquireDyno()));


  connect(ui->sampleFile, SIGNAL(textChanged(QString)),
          SLOT(onSampleFileChanges()));
  connect(ui->bgFile, SIGNAL(textChanged(QString)),
          SLOT(onBgFileChanges()));
  connect(ui->dfFile, SIGNAL(textChanged(QString)),
          SLOT(onDfFileChanges()));
  connect(ui->detectorCommand, SIGNAL(textChanged()),
          SLOT(onDetectorCommandChanges()));
  connect(ui->testDetector, SIGNAL(clicked()),
          SLOT(onAcquireDetector()));

  connect(ui->remoteCT, SIGNAL(toggled(bool)),
          SLOT(onDoCT()));
  connect(ui->imageHeight, SIGNAL(valueChanged(int)),
          SLOT(onImageHeightChanges()));
  connect(ui->imageWidth, SIGNAL(valueChanged(int)),
          SLOT(onImageWidthChanges()));
  connect(ui->lockPixelSize, SIGNAL(toggled(bool)),
          SLOT(onLockPixelSize()));

  connect(ui->topSlice, SIGNAL(valueChanged(int)),
          SLOT(onTopSliceChanges()));
  connect(ui->bottomSlice, SIGNAL(valueChanged(int)),
          SLOT(onBottomSliceChanges()));
  connect(ui->rorLeft, SIGNAL(valueChanged(int)),
          SLOT(onRorLeftChanges()));
  connect(ui->rorRight, SIGNAL(valueChanged(int)),
          SLOT(onRorRightChanges()));
  connect(ui->rorFront, SIGNAL(valueChanged(int)),
          SLOT(onRorFrontChanges()));
  connect(ui->rorRear, SIGNAL(valueChanged(int)),
          SLOT(onRorRearChanges()));

  connect(ui->resetRor, SIGNAL(clicked()),
          SLOT(onResetRor()));
  connect(ui->resetTopSlice, SIGNAL(clicked()),
          SLOT(onResetTop()));
  connect(ui->resetBottomSlice, SIGNAL(clicked()),
          SLOT(onResetBottom()));
  connect(ui->resetLeftRor, SIGNAL(clicked()),
          SLOT(onResetLeft()));
  connect(ui->resetRightRor, SIGNAL(clicked()),
          SLOT(onResetRight()));
  connect(ui->resetFrontRor, SIGNAL(clicked()),
          SLOT(onResetFront()));
  connect(ui->resetRearRor, SIGNAL(clicked()),
          SLOT(onResetRear()));



  ui->control->setCurrentIndex(0);

  setEngineStatus(Stopped);

  setThetaUnits();
  setBgUnits();
  setLoopUnits();
  setSubLoopUnits();

  setThetaPrec();
  setBgPrec();
  setLoopPrec();
  setSubLoopPrec();

  onExpPathChanges();
  onExpNameChanges();
  onExpDescChanges();
  onSampleDescChanges();
  onPeopleListChanges();

  onPreCommandChanges();
  onPostCommandChanges();

  onThetaMotorChanges();
  onScanPosChanges();
  onScanRangeChanges();
  onScanStepChanges();
  onProjectionsChanges();
  onScanStartChanges();
  onScanEndChanges();
  setScanTable();

  onTransMotorChanges();
  onTransPosChanges();
  onTransIntervalChanges();
  onTransDistChanges();
  onTransInChanges();
  onTransOutChanges();
  onDfChanges();
  onShutterChanges();

  onShotModeChanges();
  onLoopPosChanges();
  onLoopNumberChanges();
  onSubLoopPosChanges();
  onSubLoopNumberChanges();

  onDynoChanges();

  onSampleFileChanges();
  onBgFileChanges();
  onDfFileChanges();

  onDoCT();

  updateTestButtons();

  hui->table->sortByColumn(0, Qt::AscendingOrder);

  ui->expPath->setText(QDir::homePath());
  // WARNING: PORTING ISSUE
  loadConfiguration("/etc/ctgui.defaults");


}


MainWindow::~MainWindow()
{
  aqExec->close();
  aqExec->remove();
  preExec->close();
  preExec->remove();
  postExec->close();
  postExec->remove();
  delete thetaMotor;
  delete bgMotor;
  delete loopMotor;
  delete subLoopMotor;
  delete ui;
}


bool MainWindow::init() {

  roleName[SAMPLE] = "sample";
  roleName[DF] = "dark-current";
  roleName[BG] = "background";
  roleName[LOOP] = "loop";
  roleName[SLOOP] = "sub-loop";

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



void MainWindow::saveConfiguration(QString fileName) {

  if ( fileName.isEmpty() )
    fileName = QFileDialog::getSaveFileName(0, "Save configuration", QDir::currentPath());

  QSettings config(fileName, QSettings::IniFormat);

  config.beginGroup("general");
  config.setValue("title", ui->expName->text());
  config.setValue("description", ui->expDesc->toPlainText());
  config.setValue("sample", ui->sampleDesc->toPlainText());
  config.setValue("people", ui->peopleList->toPlainText());
  config.endGroup();

  config.beginGroup("prepost");
  config.setValue("pre", ui->preCommand->toPlainText());
  config.setValue("post", ui->postCommand->toPlainText());
  config.endGroup();

  config.beginGroup("scan");
  config.setValue("motor", thetaMotor->motor()->getPv());
  config.setValue("prec", thetaMotor->motor()->getPrecision());
  config.setValue("start", ui->scanStart->value());
  config.setValue("range", ui->scanRange->value());
  config.setValue("steps", ui->projections->value());
  config.setValue("add", ui->scanAdd->isChecked());
  config.endGroup();

  config.beginGroup("flatfield");
  config.setValue("interval", ui->transInterval->value());
  config.setValue("motor", bgMotor->motor()->getPv());
  config.setValue("prec", bgMotor->motor()->getPrecision());
  config.setValue("inbeam", ui->transIn->value());
  config.setValue("distance", ui->transDist->value());
  config.setValue("before", ui->dfBefore->value());
  config.setValue("after", ui->dfAfter->value());
  config.endGroup();

  if (ui->multiShot->isChecked()) {

    config.beginGroup("loop");
    config.setValue("singleBackground", ! ui->multiBg->isChecked() );
    config.setValue("motor", loopMotor->motor()->getPv());
    config.setValue("prec", loopMotor->motor()->getPrecision());
    config.setValue("start", ui->loopStart->value());
    config.setValue("range", ui->loopRange->value());
    config.setValue("steps", ui->loopNumber->value());

    if (ui->subLoop->isChecked()) {
      config.beginGroup("subloop");
      config.setValue("motor", subLoopMotor->motor()->getPv());
      config.setValue("prec", subLoopMotor->motor()->getPrecision());
      config.setValue("start", ui->subLoopStart->value());
      config.setValue("range", ui->subLoopRange->value());
      config.setValue("steps", ui->subLoopNumber->value());
      config.endGroup();
    }

    config.endGroup();

  }

  if (ui->dynoShot->isChecked()) {

    config.beginGroup("dynoshot");
    config.setValue("motor", dynoMotor->motor()->getPv());
    config.setValue("prec", dynoMotor->motor()->getPrecision());
    config.setValue("start", ui->dynoStart->value());
    config.setValue("range", ui->dynoRange->value());

    if (ui->dyno2Shot->isChecked()) {

      config.beginGroup("dyno2shot");
      config.setValue("motor", dyno2Motor->motor()->getPv());
      config.setValue("prec", dyno2Motor->motor()->getPrecision());
      config.setValue("start", ui->dyno2Start->value());
      config.setValue("range", ui->dyno2Range->value());
      config.endGroup();

    }

    config.endGroup();

  }

  config.beginGroup("detector");
  config.setValue("sampleFile", ui->sampleFile->text());
  config.setValue("backgroundFile", ui->bgFile->text());
  config.setValue("darkfieldFile", ui->dfFile->text());
  config.setValue("command", ui->detectorCommand->toPlainText());
  config.setValue("live", ui->livePreview->isChecked());
  config.endGroup();

}


void MainWindow::loadConfiguration(QString fileName) {

  engineStatus=Paused; // to prevent scanView table updates;

  if ( fileName.isEmpty() )
    fileName = QFileDialog::getOpenFileName(0, "Load configuration", QDir::currentPath());

  if ( ! QFile::exists(fileName) ) {
    appendMessage(ERROR, "Configuration file \"" + fileName + "\" does not exist.");
    return;
  }
  QSettings config(fileName, QSettings::IniFormat);

  int prec=0;

  QStringList groups = config.childGroups();


  config.beginGroup("general");
  if ( config.contains("title")  )
    ui->expName->setText( config.value("title").toString() );
  if ( config.contains("description")  )
    ui->expDesc->setPlainText( config.value("description").toString() );
  if ( config.contains("sample")  )
    ui->sampleDesc->setPlainText( config.value("sample").toString() );
  if ( config.contains("people")  )
    ui->peopleList->setPlainText(config.value("people").toString());
  config.endGroup();

  config.beginGroup("prepost");
  if ( config.contains("pre")  )
    ui->preCommand->setPlainText(config.value("pre").toString());
  if ( config.contains("post")  )
    ui->postCommand->setPlainText(config.value("post").toString());
  config.endGroup();

  config.beginGroup("scan");
  if ( config.contains("prec") &&
       ( prec = config.value("prec").toInt() ) )
    setThetaPrec(prec);
  if ( config.contains("motor")  )
    thetaMotor->motor()->setPv( config.value("motor").toString() );
  if ( config.contains("start")  )
    ui->scanStart->setValue( config.value("start").toDouble() );
  if ( config.contains("range")  )
    ui->scanRange->setValue( config.value("range").toDouble() );
  if ( config.contains("steps")  )
    ui->projections->setValue( config.value("steps").toInt() );
  if ( config.contains("add")  )
    ui->scanAdd->setChecked( config.value("add").toBool() );
  config.endGroup();

  config.beginGroup("flatfield");
  if ( config.contains("interval")  )
    ui->transInterval->setValue( config.value("interval").toInt() );
  if ( config.contains("prec") &&
       ( prec = config.value("prec").toInt() ) )
    setBgPrec(prec);
  if ( config.contains("motor")  )
    bgMotor->motor()->setPv( config.value("motor").toString() );
  if ( config.contains("inbeam")  )
    ui->transIn->setValue( config.value("inbeam").toDouble() );
  if ( config.contains("distance")  )
    ui->transDist->setValue( config.value("distance").toDouble() );
  if ( config.contains("before")  )
    ui->dfBefore->setValue( config.value("before").toInt() );
  if ( config.contains("after")  )
    ui->dfAfter->setValue( config.value("after").toInt() );
  config.endGroup();

  ui->multiShot->setChecked( groups.contains("loop"));
  if ( groups.contains("loop") ) {

    config.beginGroup("loop");

    if ( config.contains("singleBackground")  )
      ui->multiBg->setChecked( config.value("singleBackground").toBool() );
    if ( config.contains("prec") &&
         ( prec = config.value("prec").toInt() ) )
      setLoopPrec(prec);
    if ( config.contains("motor")  )
      loopMotor->motor()->setPv( config.value("motor").toString() );
    if ( config.contains("start")  )
      ui->loopStart->setValue( config.value("start").toDouble() ) ;
    if ( config.contains("steps")  )
      ui->loopNumber->setValue( config.value("steps").toInt() );
    if ( config.contains("range")  )
      ui->loopRange->setValue( config.value("range").toDouble() ) ;

    if ( ! config.childGroups().contains("subloop") ) {
      ui->subLoop->setChecked(false);
    } else {
      ui->subLoop->setChecked(true);

      config.beginGroup("subloop");
      if ( config.contains("prec") &&
           ( prec = config.value("prec").toInt() ) )
        setSubLoopPrec(prec);
      if ( config.contains("motor")  )
        subLoopMotor->motor()->setPv( config.value("motor").toString() );
      if ( config.contains("start")  )
        ui->subLoopStart->setValue( config.value("start").toDouble() ) ;
      if ( config.contains("steps")  )
        ui->subLoopNumber->setValue( config.value("steps").toInt() );
      if ( config.contains("range")  )
        ui->subLoopRange->setValue( config.value("range").toDouble() );

      config.endGroup();

    }

    config.endGroup();

  }

  ui->dynoShot->setChecked(groups.contains("dynoshot"));
  if ( groups.contains("dynoshot") ) {

    config.beginGroup("dynoshot");

    if ( config.contains("prec") &&
         ( prec = config.value("prec").toInt() ) )
      setDynoPrec(prec);
    if ( config.contains("motor")  )
      dynoMotor->motor()->setPv( config.value("motor").toString() );
    if ( config.contains("start")  )
      ui->dynoStart->setValue( config.value("start").toDouble() ) ;
    if ( config.contains("range")  )
      ui->dynoRange->setValue( config.value("range").toDouble() ) ;


    if ( ! config.childGroups().contains("dyno2shot") ) {
      ui->dyno2Shot->setChecked(false);
    } else {
      ui->dyno2Shot->setChecked(true);

      config.beginGroup("dyno2shot");
      if ( config.contains("prec") &&
           ( prec = config.value("prec").toInt() ) )
        setDyno2Prec(prec);
      if ( config.contains("motor")  )
        dyno2Motor->motor()->setPv( config.value("motor").toString() );
      if ( config.contains("start")  )
        ui->dyno2Start->setValue( config.value("start").toDouble() ) ;
      if ( config.contains("range")  )
        ui->dyno2Range->setValue( config.value("range").toDouble() ) ;
      config.endGroup();

    }

    config.endGroup();

  }


  config.beginGroup("detector");
  if ( config.contains("sampleFile")  )
    ui->sampleFile->setText( config.value("sampleFile").toString() );
  if ( config.contains("backgroundFile")  )
    ui->bgFile->setText( config.value("backgroundFile").toString() );
  if ( config.contains("darkfieldFile")  )
    ui->dfFile->setText( config.value("darkfieldFile").toString() );
  if ( config.contains("command")  )
    ui->detectorCommand->setPlainText(config.value("command").toString());
  if ( config.contains("live")  )
    ui->livePreview->setChecked( config.value("live").toBool() );
  config.endGroup();


  if ( ui->expPath->text().isEmpty()  || fileName.isEmpty() )
    ui->expPath->setText( QFileInfo(fileName).absolutePath());

  engineStatus=Stopped; // to reenable scanView table updates;
  QTimer::singleShot(0, this, SLOT(setScanTable()));


}





void MainWindow::setEnv(const char * var, const char * val){

  setenv(var, val, 1);

  int found = -1, curRow=0, rowCount=hui->table->rowCount();
  while ( found < 0 && curRow < rowCount ) {
    if ( hui->table->item(curRow, 0)->text() == var )
      found = curRow;
    curRow++;
  }

  if (found < 0) {

    QString desc = envDesc.contains(var) ? envDesc[var] : "" ;

    hui->table->setSortingEnabled(false); // needed to prevent sorting after settingItem
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


void MainWindow::onFocusChange( QWidget * , QWidget * now ) {
  /*
  hDialog->setVisible( widgetsNeededHelp.contains(now) ||
                      QApplication::activeWindow() == hDialog ||
                      engineStatus == Running || engineStatus == Paused );
  */
  if ( widgetsNeededHelp.contains(now) )
    insertVariableIntoMe = now;
  else if ( QApplication::activeWindow() != hDialog )
    insertVariableIntoMe = 0;
}


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


int MainWindow::doExec(QFile * fileExec, const QString & action) {

  if ( ! fileExec ) {
    errorOnScan("Internal error: no execution file. Please report to developer.");
    return 1;
  }

  if ( fileExec->isOpen() ) {
    fileExec->close();
    fileExec->setPermissions( QFile::ExeUser | QFile::ReadUser | QFile::WriteUser);
  }

  QEventLoop q;
  QProcess proc;
  connect(&proc, SIGNAL(finished(int,QProcess::ExitStatus)), &q, SLOT(quit()));
  connect(this, SIGNAL(requestToStopAcquisition()), &q, SLOT(quit()));

  appendMessage(CONTROL, "Doing " + action + ".");

  proc.start( fileExec->fileName() );
  q.exec();

  if (proc.pid())
    proc.kill();

  int exitCode = proc.exitCode();

  QString
      out = proc.readAllStandardOutput(),
      err = proc.readAllStandardError();

  if ( ! exitCode )
    logMessage(out);
  else
    appendMessage(ERROR, "Error exit code on " + action + ".");

  if ( ! err.isEmpty() )
    appendMessage(ERROR, err);

  return exitCode;

}

bool MainWindow::prepareExec(QFile* fileExec, QPTEext * textW, QLabel * errorLabel) {

  QString res, text = textW->toPlainText();
  bool itemOK = true;

  if ( ! text.startsWith("#!") ||
      text.startsWith("#!/bin/bash") || text.startsWith("#!/bin/sh") ) {

    if ( fileExec->isOpen() )
      fileExec->resize(0);
    else
      fileExec->open( QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text );
    fileExec->write( text.toAscii() );
    fileExec->flush();

    itemOK = ! checkScript(fileExec->fileName(), res);

  }

  check(textW, itemOK);

  errorLabel->setText(res);
  errorLabel->setVisible(!itemOK);

  return itemOK;

}



void MainWindow::setThetaUnits() {

  const QString units = thetaMotor->motor()->getUnits();
  setEnv("SCANMOTORUNITS", units);

  if ( sender() != thetaMotor)
    return;

  ui->currentPos->setSuffix(units);
  ui->scanRange->setSuffix(units);
  ui->scanStart->setSuffix(units);
  ui->scanEnd->setSuffix(units);
  ui->scanStep->setSuffix(units);

}

void MainWindow::setBgUnits() {

  const QString units = bgMotor->motor()->getUnits();
  setEnv("TRANSMOTORUNITS", units);

  if ( sender() != bgMotor)
    return;

  ui->transCurrent->setSuffix(units);
  ui->transDist->setSuffix(units);
  ui->transIn->setSuffix(units);
  ui->transOut->setSuffix(units);

}

void MainWindow::setLoopUnits() {

  const QString units = loopMotor->motor()->getUnits();
  setEnv("LOOPMOTORUNITS", units);

  if ( sender() != loopMotor)
    return;

  ui->loopCurrent->setSuffix(units);
  ui->loopRange->setSuffix(units);
  ui->loopStart->setSuffix(units);
  ui->loopEnd->setSuffix(units);
  ui->loopStep->setSuffix(units);

}

void MainWindow::setSubLoopUnits() {

  const QString units = subLoopMotor->motor()->getUnits();
  setEnv("SUBLOOPMOTORUNITS", units);

  if ( sender() != subLoopMotor)
    return;

  ui->subLoopCurrent->setSuffix(units);
  ui->subLoopRange->setSuffix(units);
  ui->subLoopStart->setSuffix(units);
  ui->subLoopEnd->setSuffix(units);
  ui->subLoopStep->setSuffix(units);

}

void MainWindow::setThetaPrec(int prec){

  if ( ! prec )
    prec = thetaMotor->motor()->getPrecision();
  setEnv("SCANMOTORPREC", prec);

  ui->currentPos->setDecimals(prec);
  ui->scanRange->setDecimals(prec);
  ui->scanStart->setDecimals(prec);
  ui->scanEnd->setDecimals(prec);
  ui->scanStep->setDecimals(prec);

}

void MainWindow::setBgPrec(int prec){

  if ( ! prec )
    prec = bgMotor->motor()->getPrecision();
  setEnv("TRANSMOTORPREC", prec);

  ui->transCurrent->setDecimals(prec);
  ui->transDist->setDecimals(prec);
  ui->transIn->setDecimals(prec);
  ui->transOut->setDecimals(prec);

}

void MainWindow::setLoopPrec(int prec){

  if ( ! prec )
    prec = loopMotor->motor()->getPrecision();
  setEnv("LOOPMOTORPREC", prec);

  ui->loopCurrent->setDecimals(prec);
  ui->loopRange->setDecimals(prec);
  ui->loopStart->setDecimals(prec);
  ui->loopEnd->setDecimals(prec);
  ui->loopStep->setDecimals(prec);

}

void MainWindow::setSubLoopPrec(int prec){

  if ( ! prec )
    prec = subLoopMotor->motor()->getPrecision();
  setEnv("SUBLOOPMOTORPREC", prec);

  ui->subLoopCurrent->setDecimals(prec);
  ui->subLoopRange->setDecimals(prec);
  ui->subLoopStart->setDecimals(prec);
  ui->subLoopEnd->setDecimals(prec);
  ui->subLoopStep->setDecimals(prec);

}




void MainWindow::getThetaStart() {
  ui->scanStart->setValue(ui->currentPos->value());
}

void MainWindow::getBgStart()  {
  ui->transIn->setValue(ui->transCurrent->value());
}

void MainWindow::getLoopStart() {
    ui->loopStart->setValue(ui->loopCurrent->value());
}

void MainWindow::getSubLoopStart() {
    ui->subLoopStart->setValue(ui->subLoopCurrent->value());
}

void MainWindow::getDynoStart() {
  ui->dynoStart->setValue(ui->dynoCurrent->value());
}

void MainWindow::getDyno2Start() {
  ui->dyno2Start->setValue(ui->dyno2Current->value());
}



void MainWindow::onBrowseExpPath() {
  QString new_dir = QFileDialog::getExistingDirectory(this, "Open directory", ui->expPath->text());
  ui->expPath->setText(new_dir);
}

void MainWindow::onExpPathChanges(){

  const QString path = ui->expPath->text();
  setEnv("EXPPATH", path);

  bool itemOK = QFile::exists(path);
  check(ui->expPath, itemOK);

  if (itemOK)
    QDir::setCurrent(path);

}

void MainWindow::onExpNameChanges(){
  const QString name = ui->expName->text();
  check( ui->expName, ! name.isEmpty() );
  setEnv("EXPNAME", name);
}

void MainWindow::onExpDescChanges(){
  setEnv("EXPDESC", ui->expDesc->toPlainText());
}

void MainWindow::onSampleDescChanges(){
  setEnv("SAMPLEDESC", ui->sampleDesc->toPlainText());
}

void MainWindow::onPeopleListChanges(){
  setEnv("PEOPLE", ui->peopleList->toPlainText());
}



void MainWindow::onPreCommandChanges(){
  bool scriptOK = prepareExec(preExec, ui->preCommand, ui->preError);
  ui->preExec->setEnabled( scriptOK && ! ui->preCommand->toPlainText().isEmpty() );
}

void MainWindow::onPostCommandChanges(){
  bool scriptOK = prepareExec(postExec, ui->postCommand, ui->postError);
  ui->postExec->setEnabled( scriptOK && ! ui->postCommand->toPlainText().isEmpty() );
}


void MainWindow::onPostExec(){
  doExec(postExec, "post-scan procedures");
}

void MainWindow::onPreExec(){
  doExec(preExec, "pre-scan procedures");
}




void MainWindow::onThetaMotorChanges() {

  bool itemOK = thetaMotor->motor()->isConnected();
  check(thetaMotor->setupButton(), itemOK);

  setEnv("SCANMOTORPV", thetaMotor->motor()->getPv());
  setEnv("SCANMOTORDESC", thetaMotor->motor()->getDescription());

  if ( sender() != thetaMotor )
    return;

  onScanStartChanges();
  onScanEndChanges();

}

void MainWindow::onScanPosChanges(){
  setEnv("SCANPOS", thetaMotor->motor()->get());
}

void MainWindow::onScanRangeChanges(){

  double range = ui->scanRange->value();

  check(ui->scanRange, range != 0.0);
  setEnv("SCANRANGE", range);

  if( sender() != ui->scanRange)
    return;

  ui->scanStep->blockSignals(true);
  ui->scanStep->setValue(range/ui->projections->value());
  ui->scanStep->blockSignals(false);
  ui->scanEnd->setValue(ui->scanStart->value()+range);

  onDoCT();

}

void MainWindow::onProjectionsChanges() {

  const int proj = ui->projections->value();
  setEnv("PROJECTIONS", proj);

  if( sender() != ui->projections )
    return;

  ui->scanStep->setValue(ui->scanRange->value()/proj);
  ui->transInterval->setMaximum(proj);

  setScanTable();

}

void MainWindow::onScanStepChanges(){

  const double step = ui->scanStep->value();
  setEnv("SCANSTEP", step);

  if( sender() != ui->scanStep )
    return;

  ui->scanRange->blockSignals(true);
  ui->scanRange->setValue( copysign( ui->scanRange->value(), step));
  ui->scanRange->blockSignals(false);

  int proj = (int) nearbyint( ui->scanRange->value() / ( step==0.0 ? 1.0 : step) );
  if ( ui->scanRange->value() != 0.0 ) {
    ui->projections->blockSignals(true);
    ui->projections->setValue(proj);
    ui->projections->blockSignals(false);
  }
  ui->scanStep->setStyleSheet( step * proj == ui->scanRange->value()  ?
                               ""  :  "color: rgba(255, 0, 0, 128);");

}

void MainWindow::onScanStartChanges(){

  const double start = ui->scanStart->value();
  setEnv("SCANSTART", start);

  bool itemOK = ! thetaMotor->motor()->isConnected() || (
        start >= thetaMotor->motor()->getUserLoLimit()  &&
        start <= thetaMotor->motor()->getUserHiLimit() );
  check(ui->scanStart, itemOK);

  if( sender() != ui->scanStart)
    return;

  ui->scanEnd->setValue( start + ui->scanRange->value() );
  setScanTable();

}

void MainWindow::onScanEndChanges(){

  const double end = ui->scanEnd->value();
  setEnv("SCANEND", end);

  bool itemOK = ! thetaMotor->motor()->isConnected() || (
        end >= thetaMotor->motor()->getUserLoLimit()  &&
        end <= thetaMotor->motor()->getUserHiLimit() );
  check(ui->scanEnd, itemOK);

  if( sender() != ui->scanEnd )
    return;

  ui->scanRange->setValue( end - ui->scanStart->value() );
  setScanTable();


}

void MainWindow::onScanAddChanges(){
  onTransIntervalChanges();
  if ( sender() != ui->scanAdd )
    return;
  setScanTable();
}

void MainWindow::onSelectiveScanChanges(){
  ui->scanView->setColumnHidden( 0, ! ui->selectiveScan->isChecked() );
}

void MainWindow::setScanTable() {
  if ( engineStatus == Running || engineStatus == Paused )
    return;
  if ( engineStatus == Filling ) {
    stopMe=true;
    QTimer::singleShot(0, this, SLOT(setScanTable()));
  } else {
    ui->scanView->setUpdatesEnabled(false);
    engine(true);
    onSelectiveScanChanges();
    ui->scanView->setUpdatesEnabled(true);
    check(ui->scanView, engineStatus == Stopped); // Stoppped if has finished populating
    engineStatus = Stopped;
  }
}


void MainWindow::onFilterChanges(const QString & text) {
  proxyModel->setFilterRegExp( QRegExp(
      QString(".*%1.*").arg(text), Qt::CaseInsensitive) );
}

void MainWindow::onSelectAll() {
  ui->scanView->selectAll();
}

void MainWindow::onSelectNone() {
  ui->scanView->selectionModel()->clear();
}

void MainWindow::onSelectInvert() {
  QItemSelection slct = ui->scanView->selectionModel()->selection();
  ui->scanView->selectAll();
  ui->scanView->selectionModel()->select
      ( slct, QItemSelectionModel::Deselect );
}

void MainWindow::onCheckAll() {
  foreach ( QModelIndex idx , ui->scanView->selectionModel()->selectedRows(0) )
    scanList->itemFromIndex(proxyModel->mapToSource(idx))->setCheckState(Qt::Checked);
}

void MainWindow::onCheckNone() {
  foreach ( QModelIndex idx , ui->scanView->selectionModel()->selectedRows(0) )
    scanList->itemFromIndex(proxyModel->mapToSource(idx))->setCheckState(Qt::Unchecked);
}

void MainWindow::onCheckInvert() {
  foreach ( QModelIndex idx , ui->scanView->selectionModel()->selectedRows(0) ) {
    QStandardItem * itm = scanList->itemFromIndex(proxyModel->mapToSource(idx));
    Qt::CheckState st = itm->checkState();
    st = (st == Qt::Checked) ? Qt::Unchecked : Qt::Checked;
    itm->setCheckState(st);
  }
}


void MainWindow::onTransMotorChanges() {

  bool itemOK =
      bgMotor->motor()->isConnected() == (bool) transQty ;
  check(bgMotor->setupButton(), itemOK);

  setEnv("TRANSMOTORPV", bgMotor->motor()->getPv());
  setEnv("TRANSMOTORDESC", bgMotor->motor()->getDescription());

  if( sender() != bgMotor )
    return;

  onTransInChanges();
  onTransOutChanges();
  onTransIntervalChanges();

}

void MainWindow::onTransPosChanges() {
  setEnv("TRANSPOS", bgMotor->motor()->get());
}

void MainWindow::onTransIntervalChanges() {

  const int interval = ui->transInterval->value();
  setEnv("TRANSINTERVAL", interval);

  if (interval == ui->transInterval->minimum())
    transQty = 0;
  else
    transQty = (int) ceil(ui->projections->value()/(float)interval) +
        ( ui->scanAdd->isChecked() ? 1 : 0 ) ;

  setEnv("TRANSN", transQty);

  bool itemOK =
      bgMotor->motor()->isConnected() == (bool) transQty;
  check(ui->transInterval , itemOK);

  ui->bgFile->setEnabled(transQty);

  if ( sender() != ui->transInterval )
    return;

  onTransMotorChanges();
  onBgFileChanges();
  onTransDistChanges();
  onDoCT();
  setScanTable();

}

void MainWindow::onTransDistChanges() {

  const double dist = ui->transDist->value();
  setEnv("TRANSDIST", dist);

  bool itemOK =
      ! transQty ||  dist != 0.0;
  check(ui->transDist, itemOK);

  if( sender() == ui->transDist )
    ui->transOut->setValue(ui->transIn->value()+dist);

}

void MainWindow::onTransInChanges() {

  const double pos = ui->transIn->value();
  setEnv("TRANSIN", pos);

  bool itemOK = ! bgMotor->motor()->isConnected() || (
        pos >= bgMotor->motor()->getUserLoLimit()  &&
        pos <= bgMotor->motor()->getUserHiLimit() );
  check(ui->transIn, itemOK);

  if( sender() != ui->transIn )
    return;

  ui->transOut->setValue(pos+ui->transDist->value());
  setScanTable();

}

void MainWindow::onTransOutChanges() {

  const double pos = ui->transOut->value();
  setEnv("TRANSOUT", pos);

  bool itemOK = ! bgMotor->motor()->isConnected() || (
        pos >= bgMotor->motor()->getUserLoLimit()  &&
        pos <= bgMotor->motor()->getUserHiLimit() );
  check(ui->transOut, itemOK);

  if( sender() != ui->transOut )
    return;

  ui->transDist->setValue(pos - ui->transIn->value());
  setScanTable();

}

void MainWindow::onDfChanges() {

  int bf=ui->dfBefore->value(), af=ui->dfAfter->value(),
      total = af+bf;

  ui->dfFile->setEnabled(total);
  onDfFileChanges();

  bool itemOK =
      ! total ||
      ( opnSts->isConnected() && clsSts->isConnected() &&
       opnCmd->isConnected() && clsCmd->isConnected() );
  check(ui->dfBefore, itemOK);
  check(ui->dfAfter, itemOK);
  check(ui->shutterMan, itemOK);

  setEnv("DFBEFORE", bf);
  setEnv("DFAFTER", af);
  setEnv("DFTOTAL", total);

  onDoCT();
  setScanTable();

}


void MainWindow::onShotModeChanges() {
  ui->multiWg->setEnabled(ui->multiShot->isChecked());
  ui->multiBg->setEnabled(ui->multiShot->isChecked());
  onLoopMotorChanges();
  onLoopRangeChanges();
  onLoopStepChanges();
  onLoopStartChanges();
  onLoopEndChanges();
  onSubLoopChanges();
  onDoCT();
  updateTestButtons();
  if( sender() != ui->singleShot  &&  sender() != ui->multiShot )
    return;
  setScanTable();
}

void MainWindow::onLoopPosChanges() {
  setEnv("LOOPPOS", loopMotor->motor()->get());
}

void MainWindow::onLoopMotorChanges() {

  bool itemOK =
      ui->singleShot->isChecked() || loopMotor->motor()->isConnected();
  check(loopMotor->setupButton(), itemOK);

  setEnv("LOOPMOTORPV", loopMotor->motor()->getPv());
  setEnv("LOOPMOTORDESC", loopMotor->motor()->getDescription());

  if( sender() != loopMotor)
    return;

  onLoopStartChanges();
  onLoopEndChanges();

}

void MainWindow::onLoopRangeChanges() {

  const double dist = ui->loopRange->value();
  setEnv("LOOPRANGE", dist);

  bool itemOK =
      ui->singleShot->isChecked() || dist != 0.0;
  check(ui->loopRange, itemOK);

  if( sender() != ui->loopRange )
    return;

  ui->loopEnd->setValue(ui->loopStart->value()+dist);

  ui->loopStep->blockSignals(true);
  ui->loopStep->setValue(dist/(ui->loopNumber->value()-1));
  ui->loopStep->blockSignals(false);
  onLoopStepChanges();

}

void MainWindow::onLoopStepChanges() {

  const double step = ui->loopStep->value();
  setEnv("LOOPSTEP", step);

  bool itemOK =
      ui->singleShot->isChecked() || step != 0.0;
  check(ui->loopStep, itemOK);

  if( sender() != ui->loopStep )
    return;

  double newRange = step * ( ui->loopNumber->value() - 1 );

  ui->loopRange->blockSignals(true);
  ui->loopRange->setValue( newRange );
  ui->loopRange->blockSignals(false);

  ui->loopEnd->blockSignals(true);
  ui->loopEnd->setValue( ui->loopStart->value() + newRange );
  ui->loopEnd->blockSignals(false);

}

void MainWindow::onLoopNumberChanges(){

  const int shots = ui->loopNumber->value();
  setEnv("LOOPN", shots);

  if( sender() != ui->loopNumber )
    return;

  double newRange = ui->loopStep->value() * (shots-1);

  ui->loopRange->blockSignals(true);
  ui->loopRange->setValue( newRange );
  ui->loopRange->blockSignals(false);

  ui->loopEnd->blockSignals(true);
  ui->loopEnd->setValue( ui->loopStart->value() + newRange );
  ui->loopEnd->blockSignals(false);

  setScanTable();

}

void MainWindow::onLoopStartChanges() {

  const double pos = ui->loopStart->value();
  setEnv("LOOPSTART", pos);

  bool itemOK =
      ui->singleShot->isChecked() || ! loopMotor->motor()->isConnected() ||
      ( pos >= loopMotor->motor()->getUserLoLimit()  &&
       pos <= loopMotor->motor()->getUserHiLimit() );
  check(ui->loopStart, itemOK);

  if( sender() != ui->loopStart )
    return;

  ui->loopEnd->blockSignals(true);
  ui->loopEnd->setValue( pos + ui->loopStep->value() * ( ui->loopNumber->value() - 1 ) );
  ui->loopEnd->blockSignals(false);

  setScanTable();

}

void MainWindow::onLoopEndChanges() {

  const double pos = ui->loopEnd->value();
  setEnv("LOOPEND", pos);

  bool itemOK =
      ui->singleShot->isChecked() || ! loopMotor->motor()->isConnected() ||
      ( pos >= loopMotor->motor()->getUserLoLimit()  &&
       pos <= loopMotor->motor()->getUserHiLimit() );
  check(ui->loopEnd, itemOK);

  if( sender() != ui->loopEnd)
    return;

  double newRange = pos - ui->loopStart->value();

  ui->loopRange->blockSignals(true);
  ui->loopRange->setValue( newRange );
  ui->loopRange->blockSignals(false);

  ui->loopStep->blockSignals(true);
  ui->loopStep->setValue(newRange/(ui->loopNumber->value()-1));
  ui->loopStep->blockSignals(false);

  setScanTable();

}


void MainWindow::onSubLoopChanges() {
  ui->subLoopWg->setEnabled(ui->subLoop->isChecked());
  onSubLoopMotorChanges();
  onSubLoopRangeChanges();
  onSubLoopStepChanges();
  onSubLoopStartChanges();
  onSubLoopEndChanges();
  if( sender() != ui->subLoop )
    return;
  setScanTable();
}

void MainWindow::onSubLoopPosChanges(){
  setEnv("SUBLOOPPOS", subLoopMotor->motor()->get() );
}

void MainWindow::onSubLoopMotorChanges() {

  bool itemOK = ! ui->subLoop->isChecked() ||
      ui->singleShot->isChecked() || subLoopMotor->motor()->isConnected();
  check(subLoopMotor->setupButton(), itemOK);

  setEnv("SUBLOOPMOTORPV", subLoopMotor->motor()->getPv());
  setEnv("SUBLOOPMOTORDESC", subLoopMotor->motor()->getDescription());

  if( sender() != subLoopMotor)
    return;

  onSubLoopStartChanges();
  onSubLoopEndChanges();

}

void MainWindow::onSubLoopRangeChanges() {

  const double dist = ui->subLoopRange->value();
  setEnv("SUBLOOPRANGE", dist);

  bool itemOK = ! ui->subLoop->isChecked() ||
      ui->singleShot->isChecked() || dist != 0.0;
  check(ui->subLoopRange, itemOK);

  if( sender() != ui->subLoopRange )
    return;

  ui->subLoopEnd->setValue(ui->subLoopStart->value()+dist);

  ui->subLoopStep->blockSignals(true);
  ui->subLoopStep->setValue(dist/(ui->subLoopNumber->value()-1));
  ui->subLoopStep->blockSignals(false);
  onSubLoopStepChanges();

}

void MainWindow::onSubLoopStepChanges() {

  const double step = ui->subLoopStep->value();
  setEnv("SUBLOOPSTEP", step);

  bool itemOK = ! ui->subLoop->isChecked() ||
      ui->singleShot->isChecked() || step != 0.0;
  check(ui->subLoopStep, itemOK);

  if( sender() != ui->subLoopStep )
    return;

  double newRange = step * ( ui->subLoopNumber->value() - 1 );

  ui->subLoopRange->blockSignals(true);
  ui->subLoopRange->setValue( newRange );
  ui->subLoopRange->blockSignals(false);

  ui->subLoopEnd->blockSignals(true);
  ui->subLoopEnd->setValue( ui->subLoopStart->value() + newRange );
  ui->subLoopEnd->blockSignals(false);

}

void MainWindow::onSubLoopNumberChanges(){

  const int shots = ui->subLoopNumber->value();
  setEnv("SUBLOOPN", shots);

  if( sender() != ui->subLoopNumber )
    return;

  double newRange = ui->subLoopStep->value() * (shots-1);

  ui->subLoopRange->blockSignals(true);
  ui->subLoopRange->setValue( newRange );
  ui->subLoopRange->blockSignals(false);

  ui->subLoopEnd->blockSignals(true);
  ui->subLoopEnd->setValue( ui->subLoopStart->value() + newRange );
  ui->subLoopEnd->blockSignals(false);

  setScanTable();

}

void MainWindow::onSubLoopStartChanges() {

  const double pos = ui->subLoopStart->value();
  setEnv("SUBLOOPSTART", pos);

  bool itemOK = ! ui->subLoop->isChecked() ||
      ui->singleShot->isChecked() || ! subLoopMotor->motor()->isConnected() ||
      ( pos >= subLoopMotor->motor()->getUserLoLimit()  &&
       pos <= subLoopMotor->motor()->getUserHiLimit() );
  check(ui->subLoopStart, itemOK);

  if( sender() != ui->subLoopStart )
    return;

  ui->subLoopEnd->blockSignals(true);
  ui->subLoopEnd->setValue( pos + ui->subLoopStep->value() * ( ui->subLoopNumber->value() - 1 ) );
  ui->subLoopEnd->blockSignals(false);

  setScanTable();

}

void MainWindow::onSubLoopEndChanges() {

  const double pos = ui->subLoopEnd->value();
  setEnv("SUBLOOPEND", pos);

  bool itemOK = ! ui->subLoop->isChecked() ||
      ui->singleShot->isChecked() || ! subLoopMotor->motor()->isConnected() ||
      ( pos >= subLoopMotor->motor()->getUserLoLimit()  &&
       pos <= subLoopMotor->motor()->getUserHiLimit() );
  check(ui->subLoopEnd, itemOK);

  if( sender() != ui->subLoopEnd)
    return;

  double newRange = pos - ui->subLoopStart->value();

  ui->subLoopRange->blockSignals(true);
  ui->subLoopRange->setValue( newRange );
  ui->subLoopRange->blockSignals(false);

  ui->subLoopStep->blockSignals(true);
  ui->subLoopStep->setValue(newRange/(ui->subLoopNumber->value()-1));
  ui->subLoopStep->blockSignals(false);

  setScanTable();

}



void MainWindow::onDynoChanges() {
  onDynoMotorChanges();
  onDynoStartChanges();
  onDynoEndChanges();
  onDynoRangeChanges();
  onDyno2Changes();
  ui->dynoWg->setEnabled(ui->dynoShot->isChecked());
  updateTestButtons();
}

void MainWindow::onDynoPosChanges(){
  setEnv("DYNOPOS", dynoMotor->motor()->get());
}

void MainWindow::setDynoUnits(){

  const QString units = dynoMotor->motor()->getUnits();
  setEnv("DYNOMOTORUNITS", units);

  if ( sender() != dynoMotor)
    return;

  ui->dynoCurrent->setSuffix(units);
  ui->dynoRange->setSuffix(units);
  ui->dynoStart->setSuffix(units);
  ui->loopEnd->setSuffix(units);

}

void MainWindow::setDynoPrec(int prec){

  if ( ! prec )
    prec = dynoMotor->motor()->getPrecision();
  setEnv("DYNOMOTORPREC", prec);

  ui->dynoCurrent->setDecimals(prec);
  ui->dynoRange->setDecimals(prec);
  ui->dynoStart->setDecimals(prec);
  ui->dynoEnd->setDecimals(prec);

}

void MainWindow::onDynoMotorChanges(){

  bool itemOK =
      ! ui->dynoShot->isChecked() || dynoMotor->motor()->isConnected();
  check(dynoMotor->setupButton(), itemOK);

  setEnv("DYNOMOTORPV", dynoMotor->motor()->getPv());
  setEnv("DYNOMOTORDESC", dynoMotor->motor()->getDescription());

  if( sender() != dynoMotor)
    return;

  onDynoStartChanges();
  onDynoEndChanges();

}

void MainWindow::onDynoRangeChanges() {

  const double dist = ui->dynoRange->value();
  setEnv("DYNORANGE", dist);

  bool itemOK =
      ! ui->dynoShot->isChecked() || dist != 0.0;
  check(ui->dynoRange, itemOK);

  if( sender() != ui->dynoRange )
    return;

  ui->dynoEnd->setValue(ui->dynoStart->value()+dist);

}

void MainWindow::onDynoStartChanges() {

  const double pos = ui->dynoStart->value();
  setEnv("DYNOSTART", pos);

  bool itemOK =
      ! ui->dynoShot->isChecked() || ! dynoMotor->motor()->isConnected() ||
      ( pos >= dynoMotor->motor()->getUserLoLimit()  &&
       pos <= dynoMotor->motor()->getUserHiLimit() );
  check(ui->dynoStart, itemOK);

  if( sender() != ui->dynoStart )
    return;

  ui->dynoEnd->blockSignals(true);
  ui->dynoEnd->setValue( pos + ui->dynoRange->value() );
  ui->dynoEnd->blockSignals(false);

}

void MainWindow::onDynoEndChanges() {

  const double pos = ui->dynoEnd->value();
  setEnv("DYNOEND", pos);

  bool itemOK =
      ! ui->dynoShot->isChecked() || ! dynoMotor->motor()->isConnected() ||
      ( pos >= dynoMotor->motor()->getUserLoLimit()  &&
       pos <= dynoMotor->motor()->getUserHiLimit() );
  check(ui->dynoEnd, itemOK);

  if( sender() != ui->dynoEnd)
    return;

  double newRange = pos - ui->dynoStart->value();

  ui->dynoRange->blockSignals(true);
  ui->dynoRange->setValue( newRange );
  ui->dynoRange->blockSignals(false);
  onDynoRangeChanges();

}





void MainWindow::onDyno2Changes() {
  onDyno2MotorChanges();
  onDyno2StartChanges();
  onDyno2EndChanges();
  onDyno2RangeChanges();
  ui->dyno2Wg->setEnabled(ui->dyno2Shot->isChecked());
}

void MainWindow::onDyno2PosChanges(){
  setEnv("DYNO2POS", dyno2Motor->motor()->get());
}

void MainWindow::setDyno2Units(){

  const QString units = dyno2Motor->motor()->getUnits();
  setEnv("DYNO2MOTORUNITS", units);

  if ( sender() != dyno2Motor)
    return;

  ui->dyno2Current->setSuffix(units);
  ui->dyno2Range->setSuffix(units);
  ui->dyno2Start->setSuffix(units);
  ui->loopEnd->setSuffix(units);

}

void MainWindow::setDyno2Prec(int prec){

  if ( ! prec )
    prec = dyno2Motor->motor()->getPrecision();
  setEnv("DYNO2MOTORPREC", prec);

  ui->dyno2Current->setDecimals(prec);
  ui->dyno2Range->setDecimals(prec);
  ui->dyno2Start->setDecimals(prec);
  ui->dyno2End->setDecimals(prec);

}

void MainWindow::onDyno2MotorChanges(){

  bool itemOK =
      ! ui->dyno2Shot->isChecked() ||
      ! ui->dynoShot->isChecked() || dyno2Motor->motor()->isConnected();
  check(dyno2Motor->setupButton(), itemOK);

  setEnv("DYNO2MOTORPV", dyno2Motor->motor()->getPv());
  setEnv("DYNO2MOTORDESC", dyno2Motor->motor()->getDescription());

  if( sender() != dyno2Motor)
    return;

  onDyno2StartChanges();
  onDyno2EndChanges();

}

void MainWindow::onDyno2RangeChanges() {

  const double dist = ui->dyno2Range->value();
  setEnv("DYNO2RANGE", dist);

  bool itemOK =
      ! ui->dyno2Shot->isChecked() ||
      ! ui->dynoShot->isChecked() || dist != 0.0;
  check(ui->dyno2Range, itemOK);

  if( sender() != ui->dyno2Range )
    return;

  ui->dyno2End->setValue(ui->dyno2Start->value()+dist);

}

void MainWindow::onDyno2StartChanges() {

  const double pos = ui->dyno2Start->value();
  setEnv("DYNO2START", pos);

  bool itemOK =
      ! ui->dyno2Shot->isChecked() ||
      ! ui->dynoShot->isChecked() || ! dyno2Motor->motor()->isConnected() ||
      ( pos >= dyno2Motor->motor()->getUserLoLimit()  &&
       pos <= dyno2Motor->motor()->getUserHiLimit() );
  check(ui->dyno2Start, itemOK);

  if( sender() != ui->dyno2Start )
    return;

  ui->dyno2End->blockSignals(true);
  ui->dyno2End->setValue( pos + ui->dyno2Range->value() );
  ui->dyno2End->blockSignals(false);

}

void MainWindow::onDyno2EndChanges() {

  const double pos = ui->dyno2End->value();
  setEnv("DYNO2END", pos);

  bool itemOK =
      ! ui->dyno2Shot->isChecked() ||
      ! ui->dynoShot->isChecked() || ! dyno2Motor->motor()->isConnected() ||
      ( pos >= dyno2Motor->motor()->getUserLoLimit()  &&
       pos <= dyno2Motor->motor()->getUserHiLimit() );
  check(ui->dyno2End, itemOK);

  if( sender() != ui->dyno2End)
    return;

  double newRange = pos - ui->dyno2Start->value();

  ui->dyno2Range->blockSignals(true);
  ui->dyno2Range->setValue( newRange );
  ui->dyno2Range->blockSignals(false);
  onDyno2RangeChanges();

}







void MainWindow::onSampleFileChanges() {

  QString res, text = ui->sampleFile->text();
  int evalOK = ! evalExpr(text, res);

  check(ui->sampleFile, ! text.isEmpty() && evalOK );

  ui->exampleImageName->setText( res );
  ui->exampleImageName->setStyleSheet( evalOK  ?  ""  :  "color: rgba(255, 0, 0, 128);");

  setScanTable();

}

void MainWindow::onBgFileChanges() {

  QString res, text = ui->bgFile->text();
  int evalOK = ! evalExpr(text, res);
  bool itemOK =  ! ui->bgFile->isEnabled() || text.isEmpty() || evalOK;

  check(ui->bgFile, itemOK);

  ui->exampleImageName->setText( res );
  ui->exampleImageName->setStyleSheet( evalOK  ?  ""  :  "color: rgba(255, 0, 0, 128);");

  setScanTable();

}

void MainWindow::onDfFileChanges() {

  QString res, text = ui->dfFile->text();
  bool evalOK = ! evalExpr(text, res);
  bool itemOK =  ! ui->dfFile->isEnabled() || text.isEmpty() || evalOK ;

  check(ui->dfFile, itemOK);

  ui->exampleImageName->setText( res );
  ui->exampleImageName->setStyleSheet( evalOK  ?  ""  :  "color: rgba(255, 0, 0, 128);");

  setScanTable();

}

void MainWindow::onDetectorCommandChanges(){

  bool scriptOK = prepareExec(aqExec, ui->detectorCommand, ui->detectorError);
  bool itemOK = ! ui->detectorCommand->toPlainText().isEmpty() && scriptOK;
  check(ui->detectorCommand, itemOK);
  ui->testDetector->setEnabled(itemOK);
  updateTestButtons();

}





void MainWindow::onDoCT() {

  onImageHeightChanges();
  onImageWidthChanges();
  onTopSliceChanges();
  onBottomSliceChanges();
  onRorLeftChanges();
  onRorRightChanges();
  onRorFrontChanges();
  onRorRearChanges();
  onLockPixelSize();

  bool doCT = ui->remoteCT->isChecked();

  bool rangeOK = ! doCT  ||  ui->scanRange->value() == 180;
  ui->rctReq_scanRange->setShown(!rangeOK);

  bool flatOK = ! doCT ||  ( ui->transInterval->value()  &&
                             ui->dfBefore->value() + ui->dfAfter->value() );
  ui->rctReq_flatField->setShown(!flatOK);

  bool multiOK = ! doCT || ui->singleShot->isChecked();
  ui->rctReq_multiShot->setShown(!multiOK);

  check(ui->remoteCT, ! doCT || (rangeOK && flatOK && multiOK) );

}


void MainWindow::onImageHeightChanges() {
  int height = ui->imageHeight->value();
  ui->topSlice->setMaximum(height);
  ui->bottomSlice->setMaximum(height);
  check(ui->imageHeight, ! ui->remoteCT->isChecked()  ||  height>3);
}


void MainWindow::onImageWidthChanges() {
  int width = ui->imageWidth->value();
  ui->rorFront->setMaximum(width);
  ui->rorRear->setMaximum(width);
  ui->rorLeft->setMaximum(width);
  ui->rorRight->setMaximum(width);
  ui->rotationCenter->setRange(-width/3, width/3); // theoretically should be width/2
  check(ui->imageWidth, ! ui->remoteCT->isChecked()  ||  width>3);
}


void MainWindow::onLockPixelSize() {
  bool lock = ui->lockPixelSize->isChecked();
  if (lock) {
    ui->pixelWidth->setValue(ui->pixelHeight->value());
    connect(ui->pixelHeight, SIGNAL(valueChanged(double)),
            ui->pixelWidth, SLOT(setValue(double)));
    connect(ui->pixelWidth, SIGNAL(valueChanged(double)),
            ui->pixelHeight, SLOT(setValue(double)));
  } else {
    disconnect(ui->pixelHeight, SIGNAL(valueChanged(double)),
               ui->pixelWidth, SLOT(setValue(double)));
    disconnect(ui->pixelWidth, SIGNAL(valueChanged(double)),
               ui->pixelHeight, SLOT(setValue(double)));
  }
}


void MainWindow::onTopSliceChanges() {
  int top = ui->topSlice->value();
  int bot = ui->bottomSlice->value();
  if ( bot && top > bot )
    ui->bottomSlice->setValue(top);
}


void MainWindow::onBottomSliceChanges() {
  int top = ui->topSlice->value();
  int bot = ui->bottomSlice->value();
  if ( bot && top > bot )
    ui->topSlice->setValue(bot);
}


void MainWindow::onRorLeftChanges() {
  int left = ui->rorLeft->value();
  int right = ui->rorRight->value();
  if ( right && left > right )
    ui->rorRight->setValue(left);
}


void MainWindow::onRorRightChanges() {
  int left = ui->rorLeft->value();
  int right = ui->rorRight->value();
  if ( right && left > right )
    ui->rorLeft->setValue(right);
}


void MainWindow::onRorFrontChanges() {
  int front = ui->rorFront->value();
  int rear = ui->rorRear->value();
  if ( rear && front > rear )
    ui->rorFront->setValue(rear);
}


void MainWindow::onRorRearChanges() {
  int front = ui->rorFront->value();
  int rear = ui->rorRear->value();
  if ( rear && front > rear )
    ui->rorRear->setValue(front);
}

void MainWindow::onResetTop() {
  ui->topSlice->setValue(ui->topSlice->minimum());
}


void MainWindow::onResetBottom() {
  ui->bottomSlice->setValue(ui->bottomSlice->minimum());
}


void MainWindow::onResetLeft() {
  ui->rorLeft->setValue(ui->rorLeft->minimum());
}


void MainWindow::onResetRight() {
  ui->rorRight->setValue(ui->rorRight->minimum());
}


void MainWindow::onResetFront() {
  ui->rorFront->setValue(ui->rorFront->minimum());
}


void MainWindow::onResetRear() {
  ui->rorRear->setValue(ui->rorRear->minimum());
}


void MainWindow::onResetRor() {
  onResetTop();
  onResetBottom();
  onResetLeft();
  onResetRight();
  onResetFront();
  onResetRear();
}




void MainWindow::updateTestButtons() {
  bool st = preReq[ui->detectorCommand].first
      && ui->dynoShot->isChecked() && preReq[ui->tabDyno].first;
  ui->testDyno->setEnabled(st);
  st = preReq[ui->detectorCommand].first && preReq[ui->tabDyno].first
      && preReq[ui->tabMulti].first && ui->multiShot->isChecked();
  ui->testMulti->setEnabled(st);
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

    preReq[obj] = qMakePair(status, (const QWidget*) tab);

  } else {

    tab = (QWidget*) preReq[obj].second;
    preReq[obj].first = status;

  }

  bool tabOK=status, allOK=status;
  if ( status )
    foreach ( ReqP tabel, preReq)
      if ( tabel.second == tab )
        tabOK &= tabel.first;
  if (tab) {
    preReq[tab] = qMakePair(tabOK, (const QWidget*)0);
    ui->control->setTabIcon(ui->control->indexOf(tab),
                            tabOK ? goodIcon : badIcon);
    updateTestButtons();
  }
  if ( status )
    foreach ( ReqP tabel, preReq)
      if ( ! tabel.second )
        allOK &= tabel.first;
  ui->startStop->setEnabled(allOK);

}



void MainWindow::onShutterMan(){
  if ( ! shutterStatus ) // in prog
    return;
  QTimer::singleShot(2000, this, SLOT(onShutterChanges()));
  shutterMan( shutterStatus < 0, true ); // invert current status
}

void MainWindow::onShutterChanges() {

  if ( ! opnSts->isConnected() || ! clsSts->isConnected() ) {
    shutterStatus = 0;
    ui->shutterMan->setEnabled(false);
    return;
  }

  if ( clsSts->get().toBool() == opnSts->get().toBool()  )  { // in between
    shutterStatus = 0;
    ui->shutterMan->setText("Wait");
    ui->shutterStatus->setText("in progress");
  } else if ( opnSts->get().toBool() ) {
    shutterStatus = 1;
    ui->shutterStatus->setText("opened");
    ui->shutterMan->setText("Close");
  } else {
    shutterStatus = -1;
    ui->shutterStatus->setText("closed");
    ui->shutterMan->setText("Open");
  }
  ui->shutterMan->setEnabled(shutterStatus);


}



int MainWindow::shutterMan(bool st, bool wait) {

  int rst = st ? 1 : -1 ;

  if ( ! opnSts->isConnected() || ! clsSts->isConnected() ||
       ! opnCmd->isConnected() || ! clsCmd->isConnected() ) {
    appendMessage(ERROR, "Shutter is not connected.");
    return shutterStatus;
  }
  if ( rst == shutterStatus )
    return shutterStatus;

  appendMessage(CONTROL, QString(st ? "Openning" : "Closing") + " shutter.");

  const int delay = 5000;  // 5 sec total delay
  QEventLoop q;
  QTimer tT;
  tT.setSingleShot(true);

  connect(&tT, SIGNAL(timeout()), &q, SLOT(quit()));
  connect(opnSts, SIGNAL(valueUpdated(QVariant)), &q, SLOT(quit()));
  connect(opnSts, SIGNAL(connectionChanged(bool)), &q, SLOT(quit()));
  connect(clsSts, SIGNAL(valueUpdated(QVariant)), &q, SLOT(quit()));
  connect(clsSts, SIGNAL(connectionChanged(bool)), &q, SLOT(quit()));



  if ( ! shutterStatus ) { // in prog

    tT.start(delay);
    while ( tT.isActive() && ! shutterStatus ) {
      q.exec();
      onShutterChanges();
    }

    if ( ! shutterStatus ) {
      appendMessage(ERROR, "Shutter is in progress for too long. Check it.");
      return shutterStatus;
    }
    if ( rst == shutterStatus )
      return shutterStatus;
    tT.stop();

  }

  ui->shutterMan->setEnabled(false);

  // do manipulation
  if (st) {
    clsCmd->set(0);
    opnCmd->set(1);
  } else {
    opnCmd->set(0);
    clsCmd->set(1);
  }

  if ( ! wait )
    return shutterStatus;

  tT.start(delay);
  while ( tT.isActive() && rst != shutterStatus ) {
    q.exec();
    onShutterChanges();
  }


  return shutterStatus;

}


int MainWindow::acquireDetector(const QString & filename) {

  if ( ! preReq[ui->detectorCommand].first ) {
    appendMessage(ERROR, "Image cannot be acquired now.");
    return 1;
  }
  if ( filename.isEmpty() ) {
    appendMessage(ERROR, "Empty filename to store image.");
    return 1;
  }

  ui->detectorWidget->setEnabled(false);


  setEnv("FILE", filename);
  int execStatus = doExec(aqExec, "image acquisition into \"" + filename + "\"");
  if (execStatus)
    appendMessage(ERROR, "Error acquiring \"" + filename + "\".");


  ui->detectorWidget->setEnabled(true);

  // preview.
  if ( ! execStatus && ui->livePreview->isChecked() )
    // TODO. Should be executed in a parelel thread
    ui->image->setStyleSheet("image: url(" + filename + ");");

  return execStatus;

}


void MainWindow::waitStopDynos() {
  if ( ui->dynoShot->isChecked() )
    dynoMotor->motor()->wait_stop();
  if ( ui->dyno2Shot->isChecked() )
    dyno2Motor->motor()->wait_stop();
}

void MainWindow::stopDynos() {
  if ( ui->dynoShot->isChecked() )
    dynoMotor->motor()->stop();
  if ( ui->dyno2Shot->isChecked() )
    dyno2Motor->motor()->stop();
  waitStopDynos();
}


void MainWindow::startDynos(double dynoPos, double dyno2Pos) {

  if ( ( ui->dynoShot->isChecked()  && dynoMotor->motor()->getUserGoal()  != dynoPos ) ||
       ( ui->dyno2Shot->isChecked() && dyno2Motor->motor()->getUserGoal() != dyno2Pos ) )
    stopDynos();
  else
    waitStopDynos();

  if ( ui->dynoShot->isChecked() )
    dynoMotor->motor()->goUserPosition(dynoPos, QCaMotor::STARTED);
  if ( ui->dyno2Shot->isChecked() )
    dyno2Motor->motor()->goUserPosition(dyno2Pos, QCaMotor::STARTED);

  //// qtWait(200);


}

int MainWindow::acquireDyno(const QString & filename) {

  if ( ui->dynoShot->isChecked() && ! preReq[ui->tabDyno].first ) {
    appendMessage(ERROR, "Image in the dynamic shot mode cannot be acquired now.");
    return 1;
  }
  if ( filename.isEmpty() ) {
    appendMessage(ERROR, "Empty filename to store image.");
    return 1;
  }

  ui->dynoWidget->setEnabled(false);

  int execStatus = 0;

  const double
      dStart = ui->dynoStart->value(),
      d2Start = ui->dyno2Start->value();

  startDynos(dStart, d2Start);
  waitStopDynos();
  if (stopMe) {
    ui->dynoWidget->setEnabled(true);
    return execStatus;
  }

  startDynos(ui->dynoEnd->value(), ui->dyno2End->value());
  execStatus = acquireDetector(filename);

  startDynos(dStart, d2Start);

  ui->dynoWidget->setEnabled(true);

  return execStatus;

}


int MainWindow::acquireMulti() {

  if ( ! ui->testMulti->isEnabled() ) {
    appendMessage(ERROR, "Images in the multi-shot mode cannot be acquired now.");
    return 1;
  }

  ui->multiWidget->setEnabled(false);

  const bool
      doL = ui->multiShot->isChecked(),
      doSL = doL && ui->subLoop->isChecked();

  const int
      loopN =  doL ? ui->loopNumber->value() : 1,
      subLoopN = doSL ? ui->subLoopNumber->value() : 1;
  const double
      lStart = ui->loopStart->value(),
      lEnd = ui->loopEnd->value(),
      slStart = ui->subLoopStart->value(),
      slEnd = ui->subLoopEnd->value();

  double
      lastLoop = loopMotor->motor()->get(),
      lastSubLoop = subLoopMotor->motor()->get();

  int execStatus=0;

  for ( int x = 0; x < loopN; x++) {

    double loopPos = lStart   +
        ( (loopN==1)  ?  0  :  x * (lEnd-lStart) / (loopN-1) );
    if ( doL  &&  lastLoop != loopPos ) {
      appendMessage(CONTROL, "Moving loop motor to " + QString::number(loopPos) + ".");
      loopMotor->motor()->goUserPosition(loopPos, QCaMotor::STARTED);
      lastLoop = loopPos;
    }

    for ( int y = 0; y < subLoopN; y++) {

      double subLoopPos = slStart +
          ( ( subLoopN == 1 )  ?  0  :  y * (slEnd - slStart) / (subLoopN-1) );
      if ( doSL  &&  lastSubLoop != subLoopPos ) {
        appendMessage(CONTROL, "Moving sub-loop motor to " + QString::number(subLoopPos) + ".");
        subLoopMotor->motor()->goUserPosition(subLoopPos, QCaMotor::STARTED);
        lastSubLoop = subLoopPos;
      }

      loopMotor->motor()->wait_stop();
      subLoopMotor->motor()->wait_stop();

      if (stopMe) {
        loopMotor->motor()->goUserPosition(lStart);
        subLoopMotor->motor()->goUserPosition(slStart);
        ui->multiWidget->setEnabled(true);
        return execStatus;
      }


      QString filename = ".temp." + QString::number(x) + "." + QString::number(y) + ".tif";
      execStatus = acquireDyno(filename);

      if (execStatus)
        appendMessage(ERROR, "Failed to acquire image in the multi-shot run.");
      if (execStatus || stopMe) {
        loopMotor->motor()->goUserPosition(lStart);
        subLoopMotor->motor()->goUserPosition(slStart);
        ui->multiWidget->setEnabled(true);
        return execStatus;
      }

    }

  }

  loopMotor->motor()->goUserPosition(lStart);
  subLoopMotor->motor()->goUserPosition(slStart);
  ui->multiWidget->setEnabled(true);
  return execStatus;

}




void MainWindow::onAcquireDetector() {
  if ( ! ui->detectorWidget->isEnabled() ) {
    emit requestToStopAcquisition();
  } else {
    ui->testDetector->setText("Stop acquisition");
    setEnv("AQTYPE", "SINGLESHOT");
    acquireDetector(".temp.tif");
    ui->testDetector->setText("Test");
  }
}

void MainWindow::onAcquireDyno() {
  if ( ! ui->dynoWidget->isEnabled() ) {
    stopMe = true;
    emit requestToStopAcquisition();
    dynoMotor->motor()->stop();
    dyno2Motor->motor()->stop();
  } else {
    stopMe = false;
    ui->testDyno->setText("Stop acquisition");
    setEnv("AQTYPE", "DYNOSHOT");
    acquireDyno(".temp.tif");
    ui->testDyno->setText("Test");
  }
}

void MainWindow::onAcquireMulti() {
  if ( ! ui->multiWidget->isEnabled() ) {
    stopMe = true;
    emit requestToStopAcquisition();
    loopMotor->motor()->stop();
    subLoopMotor->motor()->stop();
    dynoMotor->motor()->stop();
    dyno2Motor->motor()->stop();
  } else {
    stopMe = false;
    ui->testMulti->setText("Stop acquisition");
    setEnv("AQTYPE", "MULTISHOT");
    acquireMulti();
    ui->testMulti->setText("Test");
  }
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


void MainWindow::onAssistant() {

  if ( engineStatus == Stopped || engineStatus == Filling ) { // update initials

    getThetaStart();
    getBgStart();
    getDyno2Start();
    getDynoStart();
    getLoopStart();
    getSubLoopStart();

  } else if ( engineStatus == Paused ) { // Break

    setEngineStatus(Stopped);

  }


}


void MainWindow::onStartStop() {

  if ( engineStatus == Running ) {

    stopMe = true;
    thetaMotor->motor()->stop();
    bgMotor->motor()->stop();
    loopMotor->motor()->stop();
    subLoopMotor->motor()->stop();
    dynoMotor->motor()->stop();
    dyno2Motor->motor()->stop();
    emit requestToStopAcquisition();

  } else if ( engineStatus == Filling ) {
    stopMe = true;
  } else {
    engine(false);
  }

}


void MainWindow::setEngineStatus(EngineStatus status) {

 // if ( status == engineStatus )
 //   return;

  if (status == Paused  &&  engineStatus != Filling ) {

    ui->progressBar->setFormat("Paused at %p%.");
    ui->verticalLayoutt->addWidget(ui->progressBar);
    ui->progressBar->show();
    ui->scanView->show();

    appendMessage(CONTROL, "Pausing engine.");

    if ( ui->dynoShot->isChecked() )
      dynoMotor->motor()->goUserPosition(motorsInitials[dynoMotor], QCaMotor::STARTED);
    if ( ui->dyno2Shot->isChecked() )
      dyno2Motor->motor()->goUserPosition(motorsInitials[dyno2Motor], QCaMotor::STARTED);

    ui->startStop->setText("Resume scan");
    ui->assistant->show();
    ui->assistant->setText("Break scan");
    ui->assistant->setToolTip("Breaks the execution and return motors to initial positions.");

  } else if (status == Running) {

    ui->progressBar->setFormat("Running. %p% done.");
    ui->verticalLayoutt->addWidget(ui->progressBar);
    ui->progressBar->show();
    ui->scanView->show();

    if (engineStatus == Stopped)
      appendMessage(CONTROL,  "Starting engine.");
    else if (engineStatus == Paused)
      appendMessage(CONTROL,  "Resuming engine.");

    ui->startStop->setText("Pause scan");
    ui->assistant->hide();

    ui->tabGeneral->setEnabled(false);
    ui->tabPrePost->setEnabled(false);
    ui->tabDetector->setEnabled(false);
    ui->tabDyno->setEnabled(false);
    ui->tabFF->setEnabled(false);
    ui->tabMulti->setEnabled(false);
    ui->tabRemoteCT->setEnabled(false);
    ui->tabScan->setEnabled(false);

  } else if ( status == Filling ) {

    ui->startStop->setText("Stop table update");
    ui->startStop->setEnabled(true);
    ui->progressBar->setFormat("Updating table.\n %p%");
    ui->horizontalLayout->insertWidget(0,ui->progressBar);
    ui->progressBar->show();
    ui->scanView->hide();

  } else { // Paused while filling or Stopped

    ui->progressBar->hide();
    ui->scanView->show();

    if (engineStatus == Paused)
      appendMessage(CONTROL,  "Resetting engine.");
    else if (engineStatus == Running)
      appendMessage(CONTROL,  "Stopping engine.");

    if ( engineStatus != Filling ) {
      foreach (QCaMotorGUI * mot, motorsInitials.keys() )
        if (mot->motor()->isConnected())
          mot->motor()->goUserPosition(motorsInitials[mot]);
    }

    ui->startStop->setText("Start");
    ui->assistant->show();
    ui->assistant->setText("Set initials");
    ui->assistant->setToolTip("Get starting positions of all stages from their current positions.");
    for (int tabIdx=0 ; tabIdx < ui->control->count() ; tabIdx++ )
      ui->control->widget(tabIdx)->setEnabled(true);

  }

  engineStatus=status;

}





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

  scanList->appendRow(items);

}


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
  while ( res.isEmpty() || /* QFile::exists(res) || */ listHasFile(res) ) {
    res = eres;
    res.insert(indexOfDot, "_(" + QString::number(++count) + ")");
  }

  setEnv(var, res);
  return res;

}

bool MainWindow::listHasFile(const QString & fn) {
  if (fn.isEmpty())
    return false;
  bool found=false;
  int count = 0;
  while ( ! found && count < scanList->rowCount() )
    found  =  scanList->item(count++, 3)->text() == fn;
  return found;
}

bool MainWindow::doIt(int count) {
  if ( count >= scanList->rowCount() )
    return false;
  if ( ! ui->selectiveScan->isChecked() )
    return true;
  return scanList->item(count,0)->checkState() == Qt::Checked;
}

void MainWindow::createXLIfile(const QString & filename) {

  QFile templ(":/xli.template");
  templ.open(QFile::ReadOnly);
  QString text = templ.readAll();
  templ.close();

  text.replace("${USERNAME}", ui->vblUser->text() );
  text.replace("${PASSWORD}", ui->vblPassword->text() );
  text.replace("${PROJECTIONS}", QString::number(ui->projections->value()));

  QString prList, dcList, bgList;
  for( int idx=0 ; idx < scanList->rowCount() ; idx++ ) {
    QString filename = scanList->item(idx,3)->text();
    QString roleText = scanList->item(idx,1)->text();
    if ( roleText == roleName[SAMPLE] )
      prList += "," + filename;
    else if ( roleText == roleName[BG] )
      bgList += "," + filename;
    else if ( roleText == roleName[DF] )
      dcList += "," + filename;
  }
  text.replace("${PROJECTION_LIST}", prList);
  text.replace("${DARK_LIST}", dcList);
  text.replace("${FLAT_LIST}", bgList);

  text.replace("${TRANSINTERVAL}", QString::number(ui->transInterval->value()));
  text.replace("${CENTER}", QString::number(ui->rotationCenter->value()) );
  text.replace("${PIX_HEIGHT}", QString::number(ui->pixelHeight->value()) );
  text.replace("${PIX_WIDTH}", QString::number(ui->pixelWidth->value()) );
  text.replace("${SCAN_STEP}", QString::number(ui->scanStep->value()));

  text.replace("${TOP_SLICE}", QString::number(ui->topSlice->value()-1));
  text.replace("${BOTTOM_SLICE}",
               QString::number (
                 ( ui->bottomSlice->value() == ui->bottomSlice->minimum() ?
                     ui->imageHeight->value() : ui->bottomSlice->value() )
                 - 1 ) );

  text.replace("${DO_ROR}",
               ui->rorLeft->value() != ui->rorLeft->minimum() ||
               ui->rorRight->value() != ui->rorRight->minimum() ||
               ui->rorFront->value() != ui->rorFront->minimum() ||
               ui->rorRear->value() != ui->rorRear->minimum()
               ? "1" : "0");
  text.replace("${ROR_LEFT}", QString::number(ui->rorLeft->value()-1) );
  text.replace("${ROR_RIGHT}", QString::number(ui->rorRight->value()-1) );
  text.replace("${ROR_REAR}", QString::number(ui->rorRear->value()-1) );
  text.replace("${ROR_FRONT}", QString::number(ui->rorFront->value()-1) );
  text.replace("${SLICE_BASE}", "slice.tif");

  QFile out(filename);
  out.open(QFile::WriteOnly|QFile::Truncate);
  out.write( text.toAscii() );
  out.close();


}












void MainWindow::engine (const bool dryRun) {

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
      doL = ui->multiShot->isChecked(),
      doSL = doL && ui->subLoop->isChecked(),
      multiBg = doL && ! ui->multiBg->isChecked();
  const double
      start=ui->scanStart->value(),
      end=ui->scanEnd->value(),
      transIn = ui->transIn->value(),
      transOut = ui->transOut->value(),
      lStart = ui->loopStart->value(),
      lEnd = ui->loopEnd->value(),
      slStart = ui->subLoopStart->value(),
      slEnd = ui->subLoopEnd->value();
  const int
      projs=ui->projections->value(),
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
    shutterMan(false,true);

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
    shutterMan(true,true);

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
    shutterMan(false,true);

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

}


