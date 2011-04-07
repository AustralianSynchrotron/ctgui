#include "ctgui_mainwindow.h"
#include <QDebug>
#include <QFileDialog>
#include <QVector>
#include <QProcess>
#include <QtCore>
#include <QSettings>





static int evalExpr(const QString & templ, QString & result) {

  QProcess proc;
  proc.start("/bin/sh -c \"eval echo -n " + templ + "\"");
  proc.waitForFinished();

  QCoreApplication::processEvents();

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








const QString MainWindow::shutterPvBaseName = "SR08ID01PSS01:HU01A_BL_SHUTTER";
const QIcon MainWindow::badIcon = QIcon(":/icons/warn.svg");
const QIcon MainWindow::goodIcon = QIcon();

MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::MainWindow),
  hui(new Ui::HelpDialog),
  hDialog(new QDialog(this, Qt::Tool)),
  scanList(new QStandardItemModel(0,4,this)),
  proxyModel(new QSortFilterProxyModel(this)),
  transQty(0),
  stopMe(false),
  engineIsOn(false)
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

  thetaMotor = new QCaMotorGUI(ui->tabScan);
  ui->scanLayout->addWidget(thetaMotor->basicUI()->setup, 0, 0, 1, 1);
  bgMotor = new QCaMotorGUI(ui->tabFF);
  ui->ffLayout->addWidget(bgMotor->basicUI()->setup, 3, 0, 1, 1);
  loopMotor = new QCaMotorGUI(ui->tabMulti);
  ui->loopLayout->addWidget(loopMotor->basicUI()->setup, 0, 0, 1, 1);
  subLoopMotor = new QCaMotorGUI(ui->tabMulti);
  ui->subLoopLayout->addWidget(subLoopMotor->basicUI()->setup, 0, 0, 1, 1);
  dynoMotor = new QCaMotorGUI(ui->tabDyno);
  ui->dyno1Layout->addWidget(dynoMotor->basicUI()->setup, 1, 0, 1, 1);
  dyno2Motor = new QCaMotorGUI(ui->tabDyno);
  ui->dyno2Layout->addWidget(dyno2Motor->basicUI()->setup, 0, 0, 1, 1);

  ui->dynoStarStopWg->setVisible(false); // until we have Qt-areaDetector and can trig the detector.

  proxyModel->setSourceModel(scanList);
  proxyModel->setFilterKeyColumn(-1);
  ui->scanView->setModel(proxyModel);
  setScanTable();

  opnSts = new QEpicsPV(shutterPvBaseName + "_OPEN_STS", ui->tabFF) ;
  clsSts = new QEpicsPV(shutterPvBaseName + "_CLOSE_STS", ui->tabFF) ;
  opnCmd = new QEpicsPV(shutterPvBaseName + "_OPEN_CMD", ui->tabFF) ;
  clsCmd = new QEpicsPV(shutterPvBaseName + "_CLOSE_CMD", ui->tabFF) ;

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


  connect(thetaMotor, SIGNAL(changedUserPosition(double)),
          ui->currentPos, SLOT(setValue(double)));
  connect(thetaMotor, SIGNAL(changedUserPosition(double)),
          SLOT(onScanPosChanges()));
  connect(thetaMotor, SIGNAL(changedUnits(QString)),
          SLOT(setThetaUnits()));
  connect(thetaMotor, SIGNAL(changedPrecision(int)),
          SLOT  (setThetaPrec()));
  connect(thetaMotor, SIGNAL(changedConnected(bool)),
          SLOT(onThetaMotorChanges()));
  connect(thetaMotor, SIGNAL(changedPv()),
          SLOT(onThetaMotorChanges()));
  connect(thetaMotor, SIGNAL(changedDescription(QString)),
          SLOT(onThetaMotorChanges()));
  connect(thetaMotor, SIGNAL(changedUserLoLimit(double)),
          SLOT(onThetaMotorChanges()));
  connect(thetaMotor, SIGNAL(changedUserHiLimit(double)),
          SLOT(onThetaMotorChanges()));

  connect(ui->lockScanStart, SIGNAL(toggled(bool)),
          SLOT(lockThetaStart(bool)));
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
  connect(ui->startStop, SIGNAL(clicked()),
          SLOT(onStartStop()));

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

  connect(bgMotor, SIGNAL(changedUserPosition(double)),
          ui->transCurrent, SLOT(setValue(double)));
  connect(bgMotor, SIGNAL(changedUserPosition(double)),
          SLOT(onTransPosChanges()));
  connect(bgMotor, SIGNAL(changedUnits(QString)),
          SLOT(setBgUnits()));
  connect(bgMotor, SIGNAL(changedPrecision(int)),
          SLOT(setBgPrec()));
  connect(bgMotor, SIGNAL(changedConnected(bool)),
          SLOT(onTransMotorChanges()));
  connect(bgMotor, SIGNAL(changedPv()),
          SLOT(onTransMotorChanges()));
  connect(bgMotor, SIGNAL(changedDescription(QString)),
          SLOT(onTransMotorChanges()));
  connect(bgMotor, SIGNAL(changedUserLoLimit(double)),
          SLOT(onTransMotorChanges()));
  connect(bgMotor, SIGNAL(changedUserHiLimit(double)),
          SLOT(onTransMotorChanges()));

  connect(ui->lockTransIn, SIGNAL(toggled(bool)),
          SLOT(lockBgStart(bool)));
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

  connect(loopMotor, SIGNAL(changedUserPosition(double)),
          ui->loopCurrent, SLOT(setValue(double)));
  connect(loopMotor, SIGNAL(changedUserPosition(double)),
          SLOT(onLoopPosChanges()));
  connect(loopMotor, SIGNAL(changedUnits(QString)),
          SLOT(setLoopUnits()));
  connect(loopMotor, SIGNAL(changedPrecision(int)),
          SLOT(setLoopPrec()));
  connect(loopMotor, SIGNAL(changedConnected(bool)),
          SLOT(onLoopMotorChanges()));
  connect(loopMotor, SIGNAL(changedPv()),
          SLOT(onLoopMotorChanges()));
  connect(loopMotor, SIGNAL(changedDescription(QString)),
          SLOT(onLoopMotorChanges()));
  connect(loopMotor, SIGNAL(changedUserLoLimit(double)),
          SLOT(onLoopMotorChanges()));
  connect(loopMotor, SIGNAL(changedUserHiLimit(double)),
          SLOT(onLoopMotorChanges()));

  connect(ui->lockLoopStart, SIGNAL(toggled(bool)),
          SLOT(lockLoopStart(bool)));
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
  connect(subLoopMotor, SIGNAL(changedUserPosition(double)),
          ui->subLoopCurrent, SLOT(setValue(double)));
  connect(subLoopMotor, SIGNAL(changedUserPosition(double)),
          SLOT(onSubLoopPosChanges()));
  connect(subLoopMotor, SIGNAL(changedUnits(QString)),
          SLOT(setSubLoopUnits()));
  connect(subLoopMotor, SIGNAL(changedPrecision(int)),
          SLOT(setSubLoopPrec()));
  connect(subLoopMotor, SIGNAL(changedConnected(bool)),
          SLOT(onSubLoopMotorChanges()));
  connect(subLoopMotor, SIGNAL(changedPv()),
          SLOT(onSubLoopMotorChanges()));
  connect(subLoopMotor, SIGNAL(changedDescription(QString)),
          SLOT(onSubLoopMotorChanges()));
  connect(subLoopMotor, SIGNAL(changedUserLoLimit(double)),
          SLOT(onSubLoopMotorChanges()));
  connect(subLoopMotor, SIGNAL(changedUserHiLimit(double)),
          SLOT(onSubLoopMotorChanges()));

  connect(ui->lockSubLoopStart, SIGNAL(toggled(bool)),
          SLOT(lockSubLoopStart(bool)));
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

  connect(dynoMotor, SIGNAL(changedUserPosition(double)),
          ui->dynoCurrent, SLOT(setValue(double)));
  connect(dynoMotor, SIGNAL(changedUserPosition(double)),
          SLOT(onDynoPosChanges()));
  connect(dynoMotor, SIGNAL(changedUnits(QString)),
          SLOT(setDynoUnits()));
  connect(dynoMotor, SIGNAL(changedPrecision(int)),
          SLOT  (setDynoPrec()));
  connect(dynoMotor, SIGNAL(changedConnected(bool)),
          SLOT(onDynoMotorChanges()));
  connect(dynoMotor, SIGNAL(changedPv()),
          SLOT(onDynoMotorChanges()));
  connect(dynoMotor, SIGNAL(changedDescription(QString)),
          SLOT(onDynoMotorChanges()));
  connect(dynoMotor, SIGNAL(changedUserLoLimit(double)),
          SLOT(onDynoMotorChanges()));
  connect(dynoMotor, SIGNAL(changedUserHiLimit(double)),
          SLOT(onDynoMotorChanges()));

  connect(ui->dynoShot, SIGNAL(toggled(bool)),
          SLOT(onDynoChanges()));
  connect(ui->lockDynoStart, SIGNAL(toggled(bool)),
          SLOT(lockDynoStart(bool)));
  connect(ui->dynoRange, SIGNAL(valueChanged(double)),
          SLOT(onDynoRangeChanges()));
  connect(ui->dynoStart, SIGNAL(valueChanged(double)),
          SLOT(onDynoStartChanges()));
  connect(ui->dynoEnd, SIGNAL(valueChanged(double)),
          SLOT(onDynoEndChanges()));

  connect(ui->dyno2Shot, SIGNAL(toggled(bool)),
          SLOT(onDyno2Changes()));
  connect(dyno2Motor, SIGNAL(changedUserPosition(double)),
          ui->dyno2Current, SLOT(setValue(double)));
  connect(dyno2Motor, SIGNAL(changedUserPosition(double)),
          SLOT(onDyno2PosChanges()));
  connect(dyno2Motor, SIGNAL(changedUnits(QString)),
          SLOT(setDyno2Units()));
  connect(dyno2Motor, SIGNAL(changedPrecision(int)),
          SLOT  (setDyno2Prec()));
  connect(dyno2Motor, SIGNAL(changedConnected(bool)),
          SLOT(onDyno2MotorChanges()));
  connect(dyno2Motor, SIGNAL(changedPv()),
          SLOT(onDyno2MotorChanges()));
  connect(dyno2Motor, SIGNAL(changedDescription(QString)),
          SLOT(onDyno2MotorChanges()));
  connect(dyno2Motor, SIGNAL(changedUserLoLimit(double)),
          SLOT(onDyno2MotorChanges()));
  connect(dyno2Motor, SIGNAL(changedUserHiLimit(double)),
          SLOT(onDyno2MotorChanges()));

  connect(ui->lockDyno2Start, SIGNAL(toggled(bool)),
          SLOT(lockDyno2Start(bool)));
  connect(ui->dyno2Range, SIGNAL(valueChanged(double)),
          SLOT(onDyno2RangeChanges()));
  connect(ui->dyno2Start, SIGNAL(valueChanged(double)),
          SLOT(onDyno2StartChanges()));
  connect(ui->dyno2End, SIGNAL(valueChanged(double)),
          SLOT(onDyno2EndChanges()));


  connect(ui->sampleFile, SIGNAL(textChanged(QString)),
          SLOT(onSampleFileChanges()));
  connect(ui->bgFile, SIGNAL(textChanged(QString)),
          SLOT(onBgFileChanges()));
  connect(ui->dfFile, SIGNAL(textChanged(QString)),
          SLOT(onDfFileChanges()));
  connect(ui->detectorCommand, SIGNAL(textChanged()),
          SLOT(onDetectorCommandChanges()));
  connect(ui->acquire, SIGNAL(clicked()),
          SLOT(onAcquire()));


  ui->control->setCurrentIndex(0);


  setThetaUnits();
  setBgUnits();
  setLoopUnits();
  setSubLoopUnits();

  setThetaPrec();
  setBgPrec();
  setLoopPrec();
  setSubLoopPrec();

  lockThetaStart(ui->lockScanStart->isChecked());
  lockBgStart(ui->lockTransIn->isChecked());
  lockLoopStart(ui->lockLoopStart->isChecked());
  lockSubLoopStart(ui->lockSubLoopStart->isChecked());

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
  onLoopMotorChanges();
  onLoopPosChanges();
  onLoopRangeChanges();
  onLoopNumberChanges();
  onLoopStepChanges();
  onLoopStartChanges();
  onLoopEndChanges();

  onSubLoopChanges();
  onSubLoopMotorChanges();
  onSubLoopPosChanges();
  onSubLoopRangeChanges();
  onSubLoopNumberChanges();
  onSubLoopStepChanges();
  onSubLoopStartChanges();
  onSubLoopEndChanges();

  onDynoChanges();
  onDyno2Changes();

  onDetectorCommandChanges();
  onSampleFileChanges();
  onBgFileChanges();
  onDfFileChanges();

  hui->table->sortByColumn(0, Qt::AscendingOrder);


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
  config.setValue("motor", thetaMotor->getPv());
  if ( ! ui->lockScanStart->isChecked() )
    config.setValue("start", ui->scanStart->value());
  config.setValue("range", ui->scanRange->value());
  config.setValue("steps", ui->projections->value());
  config.setValue("add", ui->scanAdd->isChecked());
  config.setValue("return", ui->returnToOrigin->isChecked());
  config.endGroup();

  config.beginGroup("flatfield");
  config.setValue("interval", ui->transInterval->value());
  config.setValue("motor", bgMotor->getPv());
  if ( ! ui->lockTransIn->isChecked() )
    config.setValue("inbeam", ui->transIn->value());
  config.setValue("distance", ui->transDist->value());
  config.setValue("before", ui->dfBefore->value());
  config.setValue("after", ui->dfAfter->value());
  config.endGroup();

  if (ui->multiShot->isChecked()) {

    config.setValue("loop",true);

    config.beginGroup("loop");
    config.setValue("singleBackground", ! ui->multiBg->isChecked() );
    config.setValue("motor", loopMotor->getPv());
    if ( ! ui->lockLoopStart->isChecked() )
      config.setValue("start", ui->loopStart->value());
    config.setValue("range", ui->loopRange->value());
    config.setValue("steps", ui->loopNumber->value());

    if (ui->subLoop->isChecked()) {
      config.setValue("subloop",true);
      config.beginGroup("subloop");
      config.setValue("motor", subLoopMotor->getPv());
      if ( ! ui->lockSubLoopStart->isChecked() )
        config.setValue("start", ui->subLoopStart->value());
      config.setValue("range", ui->subLoopRange->value());
      config.setValue("steps", ui->subLoopNumber->value());
      config.endGroup();
    }

    config.endGroup();

  }

  if (ui->dynoShot->isChecked()) {

    config.setValue("dynoshot",true);

    config.beginGroup("dynoshot");
    config.setValue("motor",dynoMotor->getPv());
    if ( ! ui->lockDynoStart->isChecked() )
      config.setValue("start", ui->dynoStart->value());
    config.setValue("range", ui->dynoRange->value());

    if (ui->dyno2Shot->isChecked()) {

      config.setValue("dyno2shot",true);

      config.beginGroup("dyno2shot");
      config.setValue("motor",dyno2Motor->getPv());
      if ( ! ui->lockDyno2Start->isChecked() )
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


  if ( fileName.isEmpty() )
    fileName = QFileDialog::getOpenFileName(0, "Load configuration", QDir::currentPath());



  QSettings config(fileName, QSettings::IniFormat);

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
  if ( config.contains("motor")  )  thetaMotor->setPv( config.value("motor").toString() );
  ui->lockScanStart->setChecked( ! config.contains("start") );
  if ( config.contains("start")  )
    ui->scanStart->setValue( config.value("start").toDouble() );
  if ( config.contains("range")  )
    ui->scanRange->setValue( config.value("range").toDouble() );
  if ( config.contains("steps")  )
    ui->projections->setValue( config.value("steps").toInt() );
  if ( config.contains("add")  )
    ui->scanAdd->setChecked( config.value("add").toBool() );
  if ( config.contains("return")  )
    ui->returnToOrigin->setChecked( config.value("return").toBool() );
  config.endGroup();

  config.beginGroup("flatfield");
  if ( config.contains("interval")  )
    ui->transInterval->setValue( config.value("interval").toInt() );
  if ( config.contains("motor")  )
    bgMotor->setPv( config.value("motor").toString() );
  if ( config.contains("inbeam")  )
    ui->transIn->setValue( config.value("inbeam").toDouble() );
  else
    ui->lockTransIn->setChecked(true);
  if ( config.contains("distance")  )
    ui->transDist->setValue( config.value("distance").toDouble() );
  if ( config.contains("before")  )
    ui->dfBefore->setValue( config.value("before").toInt() );
  if ( config.contains("after")  )
    ui->dfAfter->setValue( config.value("after").toInt() );
  config.endGroup();


  if ( config.contains("loop") ) {

    ui->multiShot->setChecked(true);

    config.beginGroup("loop");

    if ( config.contains("singleBackground")  )
      ui->multiBg->setChecked( config.value("singleBackground").toBool() );
    if ( config.contains("motor")  )
      loopMotor->setPv( config.value("motor").toString() );
    ui->lockLoopStart->setChecked( ! config.contains("start") );
    if ( config.contains("start")  )
      ui->loopStart->setValue( config.value("start").toDouble() ) ;
    if ( config.contains("range")  )
      ui->loopRange->setValue( config.value("range").toDouble() ) ;
    if ( config.contains("steps")  )
      ui->loopNumber->setValue( config.value("steps").toInt() );

    if ( config.contains("subloop") ) {

      ui->subLoop->setChecked(true);

      config.beginGroup("subloop");
      if ( config.contains("motor")  )
        subLoopMotor->setPv( config.value("motor").toString() );
      ui->lockSubLoopStart->setChecked( ! config.contains("start") );
      if ( config.contains("start")  )
        ui->subLoopStart->setValue( config.value("start").toDouble() ) ;
      if ( config.contains("range")  )
        ui->subLoopRange->setValue( config.value("range").toDouble() );
      if ( config.contains("steps")  )
        ui->subLoopNumber->setValue( config.value("steps").toInt() );
      config.endGroup();

    }

    config.endGroup();

  }


  if ( config.contains("dynoshot") ) {

    ui->dynoShot->setChecked(true);

    config.beginGroup("dynoshot");

    if ( config.contains("motor")  )
      dynoMotor->setPv( config.value("motor").toString() );
    ui->lockDynoStart->setChecked( ! config.contains("start") );
    if ( config.contains("start")  )
      ui->dynoStart->setValue( config.value("start").toDouble() ) ;
    if ( config.contains("range")  )
      ui->dynoRange->setValue( config.value("range").toDouble() ) ;

    if ( config.contains("dyno2shot") ) {

      ui->dyno2Shot->setChecked(true);

      config.beginGroup("dyno2shot");
      if ( config.contains("motor")  )
        dyno2Motor->setPv( config.value("motor").toString() );
      ui->lockDyno2Start->setChecked( ! config.contains("start") );
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

    hui->table->insertRow(rowCount);
    hui->table->setItem( rowCount, 0, new QTableWidgetItem( QString(var) ));
    hui->table->setItem( rowCount, 1, new QTableWidgetItem( QString(val) ));

    QString desc;

    if ( ! strcmp( var, "GTRANSPOS" ) )
      desc = "Directed position of the translation stage.";
    else if ( ! strcmp( var, "FILE" ) )
      desc = "Name of the last aquired image.";
    else if ( ! strcmp( var, "AQTYPE" ) )
      desc = "Role of the image being acquired: DARKCURRENT, BACKGROUND, SAMPLE or SINGLESHOT.";
    else if ( ! strcmp( var, "DFTYPE" ) )
      desc = "Role of the dark field image being acquired: BEFORE or AFTER the scan.";
    else if ( ! strcmp( var, "DFBEFORECOUNT" ) )
      desc = "Count of the dark field image in the before-scan series.";
    else if ( ! strcmp( var, "DFAFTERCOUNT" ) )
      desc = "Count of the dark field image in the after-scan series.";
    else if ( ! strcmp( var, "DFCOUNT" ) )
      desc = "Count of the dark field image.";
    else if ( ! strcmp( var, "GSCANPOS" ) )
      desc = "Directed position of the sample rotation stage.";
    else if ( ! strcmp( var, "BGCOUNT" ) )
      desc = "Count of the background image.";
    else if ( ! strcmp( var, "PCOUNT" ) )
      desc = "Sample projection counter.";
    else if ( ! strcmp( var, "LOOPCOUNT" ) )
      desc = "Count of the image in the loop series.";
    else if ( ! strcmp( var, "GLOOPPOS" ) )
      desc = "Directed position of the loop stage.";
    else if ( ! strcmp( var, "SUBLOOPCOUNT" ) )
      desc = "Count of the image in the sub-loop series.";
    else if ( ! strcmp( var, "GSUBLOOPPOS" ) )
      desc = "Directed position of the sub-loop stage.";
    else if ( ! strcmp( var, "COUNT" ) )
      desc = "Total image counter (excluding dark field).";
    else if ( ! strcmp( var, "SCANMOTORUNITS" ) )
      desc = "Units of the rotation stage.";
    else if ( ! strcmp( var, "TRANSMOTORUNITS" ) )
      desc = "Units of the translation stage.";
    else if ( ! strcmp( var, "LOOPMOTORUNITS" ) )
      desc = "Units of the loop stage.";
    else if ( ! strcmp( var, "SUBLOOPMOTORUNITS" ) )
      desc = "Units of the sub-loop stage.";
    else if ( ! strcmp( var, "SCANMOTORPREC" ) )
      desc = "Precsicion of the rotation stage.";
    else if ( ! strcmp( var, "TRANSMOTORPREC" ) )
      desc = "Precsicion of the translation stage.";
    else if ( ! strcmp( var, "LOOPMOTORPREC" ) )
      desc = "Precsicion of the loop stage.";
    else if ( ! strcmp( var, "SUBLOOPMOTORPREC" ) )
      desc = "Precsicion of the sub-loop stage.";
    else if ( ! strcmp( var, "EXPPATH" ) )
      desc = "Working directory.";
    else if ( ! strcmp( var, "EXPNAME" ) )
      desc = "Experiment name.";
    else if ( ! strcmp( var, "EXPDESC" ) )
      desc = "Description of the experiment.";
    else if ( ! strcmp( var, "SAMPLEDESC" ) )
      desc = "Sample description.";
    else if ( ! strcmp( var, "PEOPLE" ) )
      desc = "Experimentalists list.";
    else if ( ! strcmp( var, "SCANMOTORPV" ) )
      desc = "EPICS PV of the rotation stage.";
    else if ( ! strcmp( var, "SCANMOTORDESC" ) )
      desc = "EPICS descriptiopn of the rotation stage.";
    else if ( ! strcmp( var, "SCANPOS" ) )
      desc = "Actual position of the rotation stage.";
    else if ( ! strcmp( var, "SCANRANGE" ) )
      desc = "Travel range of the rotation stage in the scan.";
    else if ( ! strcmp( var, "SCANSTEP" ) )
      desc = "Rotation step between two projections.";
    else if ( ! strcmp( var, "PROJECTIONS" ) )
      desc = "Total number of projections.";
    else if ( ! strcmp( var, "SCANSTART" ) )
      desc = "Start point of the rotation stage.";
    else if ( ! strcmp( var, "SCANEND" ) )
      desc = "End point of rotation stage.";
    else if ( ! strcmp( var, "TRANSMOTORPV" ) )
      desc = "EPICS PV of the translation stage.";
    else if ( ! strcmp( var, "TRANSMOTORDESC" ) )
      desc = "EPICS descriptiopn of the translation stage.";
    else if ( ! strcmp( var, "TRANSPOS" ) )
      desc = "Actual position of the translation stage.";
    else if ( ! strcmp( var, "TRANSINTERVAL" ) )
      desc = "Interval (number of projections) between two background acquisitions.";
    else if ( ! strcmp( var, "TRANSN" ) )
      desc = "Total number of backgrounds in the scan.";
    else if ( ! strcmp( var, "TRANSDIST" ) )
      desc = "Travel of the translation stage.";
    else if ( ! strcmp( var, "TRANSIN" ) )
      desc = "Position of the translation stage in-beam.";
    else if ( ! strcmp( var, "TRANSOUT" ) )
      desc = "Position of the translation stage out of the beam.";
    else if ( ! strcmp( var, "DFBEFORE" ) )
      desc = "Total number of dark current acquisitions before the scan.";
    else if ( ! strcmp( var, "DFAFTER" ) )
      desc = "Total number of dark current acquisitions after the scan.";
    else if ( ! strcmp( var, "DFTOTAL" ) )
      desc = "Total number of dark current acquisitions.";
    else if ( ! strcmp( var, "LOOPMOTORPV" ) )
      desc = "EPICS PV of the loop stage.";
    else if ( ! strcmp( var, "LOOPMOTORDESC" ) )
      desc = "EPICS descriptiopn of the loop stage.";
    else if ( ! strcmp( var, "LOOPRANGE" ) )
      desc = "Total travel range of the loop stage.";
    else if ( ! strcmp( var, "LOOPSTEP" ) )
      desc = "Single step between two acquisitions in the loop series.";
    else if ( ! strcmp( var, "LOOPSTART" ) )
      desc = "Start point of the loop stage.";
    else if ( ! strcmp( var, "LOOPEND" ) )
      desc = "End point of the loop stage.";
    else if ( ! strcmp( var, "SUBLOOPMOTORPV" ) )
      desc = "EPICS PV of the sub-loop stage.";
    else if ( ! strcmp( var, "SUBLOOPMOTORDESC" ) )
      desc = "EPICS descriptiopn of the sub-loop stage.";
    else if ( ! strcmp( var, "SUBLOOPRANGE" ) )
      desc = "Total travel range of the sub-loop stage.";
    else if ( ! strcmp( var, "SUBLOOPSTEP" ) )
      desc = "Single step between two acquisitions in the sub-loop series.";
    else if ( ! strcmp( var, "SUBLOOPSTART" ) )
      desc = "Start point of the sub-loop stage.";
    else if ( ! strcmp( var, "SUBLOOPEND" ) )
      desc = "End point of the sub-loop stage.";
    else if ( ! strcmp( var, "LOOPPOS" ) )
      desc = "Actual position of the loop stage.";
    else if ( ! strcmp( var, "LOOPN" ) )
      desc = "Total number of aqcuisitions in the loop series.";
    else if ( ! strcmp( var, "SUBLOOPPOS" ) )
      desc = "Actual position of the sub-loop stage.";
    else if ( ! strcmp( var, "SUBLOOPN" ) )
      desc = "Total number of aqcuisitions in the sub-loop series.";
    else if ( ! strcmp( var, "DYNOPOS" ) )
      desc = "Actual position of the stage controlling dynamic shot.";
    else if ( ! strcmp( var, "DYNOMOTORUNITS" ) )
      desc = "Units of the motor controlling dynamic shot.";
    else if ( ! strcmp( var, "DYNOMOTORPREC" ) )
      desc = "Precsicion of the motor controlling dynamic shot.";
    else if ( ! strcmp( var, "DYNOMOTORPV" ) )
      desc = "EPICS PV of the stage controlling dynamic shot";
    else if ( ! strcmp( var, "DYNOMOTORDESC" ) )
      desc = "EPICS descriptiopn of the stage controlling dynamic shot.";
    else if ( ! strcmp( var, "DYNORANGE" ) )
      desc = "Total travel range of the stage controlling dynamic shot.";
    else if ( ! strcmp( var, "DYNOSTART" ) )
      desc = "Start point of the stage controlling dynamic shot.";
    else if ( ! strcmp( var, "DYNOEND" ) )
      desc = "End point of the stage controlling dynamic shot.";
    else if ( ! strcmp( var, "DYNO2POS" ) )
      desc = "Actual position of the second stage controlling dynamic shot.";
    else if ( ! strcmp( var, "DYNO2MOTORUNITS" ) )
      desc = "Units of the motor controlling dynamic shot.";
    else if ( ! strcmp( var, "DYNO2MOTORPREC" ) )
      desc = "Precsicion of the motor controlling dynamic shot.";
    else if ( ! strcmp( var, "DYNO2MOTORPV" ) )
      desc = "EPICS PV of the second stage controlling dynamic shot";
    else if ( ! strcmp( var, "DYNO2MOTORDESC" ) )
      desc = "EPICS descriptiopn of the second stage controlling dynamic shot.";
    else if ( ! strcmp( var, "DYNO2RANGE" ) )
      desc = "Total travel range of the second stage controlling dynamic shot.";
    else if ( ! strcmp( var, "DYNO2START" ) )
      desc = "Start point of the second stage controlling dynamic shot.";
    else if ( ! strcmp( var, "DYNO2END" ) )
      desc = "End point of the second stage controlling dynamic shot.";


    hui->table->setItem( rowCount, 2, new QTableWidgetItem(desc));

    hui->table->resizeColumnToContents(0);
    hui->table->resizeColumnToContents(1);
    hui->table->resizeColumnToContents(2);

  } else {
    hui->table->item(found, 1)->setText( QString(val) );
    hui->table->resizeColumnToContents(1);
  }

}


void MainWindow::onFocusChange( QWidget * , QWidget * now ) {
  hDialog->setVisible( widgetsNeededHelp.contains(now) ||
                      QApplication::activeWindow() == hDialog ||
                      engineIsOn );
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

  const QString units = thetaMotor->getUnits();
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

  const QString units = bgMotor->getUnits();
  setEnv("TRANSMOTORUNITS", units);

  if ( sender() != bgMotor)
    return;

  ui->transCurrent->setSuffix(units);
  ui->transDist->setSuffix(units);
  ui->transIn->setSuffix(units);
  ui->transOut->setSuffix(units);

}

void MainWindow::setLoopUnits() {

  const QString units = loopMotor->getUnits();
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

  const QString units = subLoopMotor->getUnits();
  setEnv("SUBLOOPMOTORUNITS", units);

  if ( sender() != subLoopMotor)
    return;

  ui->subLoopCurrent->setSuffix(units);
  ui->subLoopRange->setSuffix(units);
  ui->subLoopStart->setSuffix(units);
  ui->subLoopEnd->setSuffix(units);
  ui->subLoopStep->setSuffix(units);

}

void MainWindow::setThetaPrec(){

  const int prec = thetaMotor->getPrecision();
  setEnv("SCANMOTORPREC", prec);

  if ( sender() != thetaMotor)
    return;

  ui->currentPos->setDecimals(prec);
  ui->scanRange->setDecimals(prec);
  ui->scanStart->setDecimals(prec);
  ui->scanEnd->setDecimals(prec);
  ui->scanStep->setDecimals(prec);

}

void MainWindow::setBgPrec() {

  const int prec = bgMotor->getPrecision();
  setEnv("TRANSMOTORPREC", prec);

  if ( sender() != bgMotor)
    return;

  ui->transCurrent->setDecimals(prec);
  ui->transDist->setDecimals(prec);
  ui->transIn->setDecimals(prec);
  ui->transOut->setDecimals(prec);

}

void MainWindow::setLoopPrec() {

  const int prec = loopMotor->getPrecision();
  setEnv("LOOPMOTORPREC", prec);

  if ( sender() != loopMotor)
    return;

  ui->loopCurrent->setDecimals(prec);
  ui->loopRange->setDecimals(prec);
  ui->loopStart->setDecimals(prec);
  ui->loopEnd->setDecimals(prec);
  ui->loopStep->setDecimals(prec);

}

void MainWindow::setSubLoopPrec() {

  const int prec = subLoopMotor->getPrecision();
  setEnv("SUBLOOPMOTORPREC", prec);

  if ( sender() != subLoopMotor)
    return;

  ui->subLoopCurrent->setDecimals(prec);
  ui->subLoopRange->setDecimals(prec);
  ui->subLoopStart->setDecimals(prec);
  ui->subLoopEnd->setDecimals(prec);
  ui->subLoopStep->setDecimals(prec);

}




void MainWindow::lockThetaStart(bool lock) {
  ui->scanStart->setValue(thetaMotor->get());
  ui->scanStart->setEnabled(!lock);
  if (lock)
    connect(thetaMotor, SIGNAL(changedUserPosition(double)),
            ui->scanStart, SLOT(setValue(double)));
  else
    disconnect(thetaMotor, SIGNAL(changedUserPosition(double)),
               ui->scanStart, SLOT(setValue(double)));
}

void MainWindow::lockBgStart(bool lock)  {
  ui->transIn->setValue(bgMotor->get());
  ui->transIn->setEnabled(!lock);;
  if (lock)
    connect(bgMotor, SIGNAL(changedUserPosition(double)),
            ui->transIn, SLOT(setValue(double)));
  else
    disconnect(bgMotor, SIGNAL(changedUserPosition(double)),
               ui->transIn, SLOT(setValue(double)));
}

void MainWindow::lockLoopStart(bool lock) {
  ui->loopStart->setValue(loopMotor->get());
  ui->loopStart->setEnabled(!lock);;
  if (lock)
    connect(loopMotor, SIGNAL(changedUserPosition(double)),
            ui->loopStart, SLOT(setValue(double)));
  else
    disconnect(loopMotor, SIGNAL(changedUserPosition(double)),
               ui->loopStart, SLOT(setValue(double)));
}

void MainWindow::lockSubLoopStart(bool lock) {
  ui->subLoopStart->setValue(subLoopMotor->get());
  ui->subLoopStart->setEnabled(!lock);;
  if (lock)
    connect(subLoopMotor, SIGNAL(changedUserPosition(double)),
            ui->subLoopStart, SLOT(setValue(double)));
  else
    disconnect(subLoopMotor, SIGNAL(changedUserPosition(double)),
               ui->subLoopStart, SLOT(setValue(double)));
}

void MainWindow::lockDynoStart(bool lock) {
  ui->dynoStart->setValue(dynoMotor->get());
  ui->dynoStart->setEnabled(!lock);;
  if (lock)
    connect(dynoMotor, SIGNAL(changedUserPosition(double)),
            ui->dynoStart, SLOT(setValue(double)));
  else
    disconnect(dynoMotor, SIGNAL(changedUserPosition(double)),
               ui->dynoStart, SLOT(setValue(double)));
}

void MainWindow::lockDyno2Start(bool lock) {
  ui->dyno2Start->setValue(dyno2Motor->get());
  ui->dyno2Start->setEnabled(!lock);;
  if (lock)
    connect(dyno2Motor, SIGNAL(changedUserPosition(double)),
            ui->dyno2Start, SLOT(setValue(double)));
  else
    disconnect(dyno2Motor, SIGNAL(changedUserPosition(double)),
               ui->dyno2Start, SLOT(setValue(double)));
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

  bool itemOK = thetaMotor->isConnected();
  check(thetaMotor->basicUI()->setup, itemOK);

  setEnv("SCANMOTORPV", thetaMotor->getPv());
  setEnv("SCANMOTORDESC", thetaMotor->getDescription());

  if ( sender() != thetaMotor )
    return;

  onScanStartChanges();
  onScanEndChanges();

}

void MainWindow::onScanPosChanges(){
  setEnv("SCANPOS", thetaMotor->get());
}

void MainWindow::onScanRangeChanges(){

  double range = ui->scanRange->value();

  check(ui->scanRange, range != 0.0);
  setEnv("SCANRANGE", range);

  if( sender() != ui->scanRange)
    return;

  ui->scanStep->blockSignals(true);
  ui->scanStep->setRange(-qAbs(range), qAbs(range));
  ui->scanStep->setValue(range/ui->projections->value());
  ui->scanStep->blockSignals(false);
  ui->scanEnd->setValue(ui->scanStart->value()+range);

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

  int proj = nearbyint( ui->scanRange->value() / ( step==0.0 ? 1.0 : step) );
  ui->projections->blockSignals(true);
  ui->projections->setValue(proj);
  ui->projections->blockSignals(false);
  ui->scanStep->setStyleSheet( step * proj == ui->scanRange->value()  ?
                                ""  :  "color: rgba(255, 0, 0, 128);");

}

void MainWindow::onScanStartChanges(){

  const double start = ui->scanStart->value();
  setEnv("SCANSTART", start);

  bool itemOK = ! thetaMotor->isConnected() || (
        start >= thetaMotor->getUserLoLimit()  &&
        start <= thetaMotor->getUserHiLimit() );
  check(ui->scanStart, itemOK);

  if( sender() != ui->scanStart)
    return;

  ui->scanEnd->setValue( start + ui->scanRange->value() );
  setScanTable();

}

void MainWindow::onScanEndChanges(){

  const double end = ui->scanEnd->value();
  setEnv("SCANEND", end);

  bool itemOK = ! thetaMotor->isConnected() || (
        end >= thetaMotor->getUserLoLimit()  &&
        end <= thetaMotor->getUserHiLimit() );
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
  if (engineIsOn) {
    stopMe=true;
    QTimer::singleShot(0, this, SLOT(setScanTable()));
  } else {
    ui->scanView->setUpdatesEnabled(false);
    engine(true);
    onSelectiveScanChanges();
    ui->scanView->setUpdatesEnabled(true);
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
      bgMotor->isConnected() == (bool) transQty ;
  check(bgMotor->basicUI()->setup, itemOK);

  setEnv("TRANSMOTORPV", bgMotor->getPv());
  setEnv("TRANSMOTORDESC", bgMotor->getDescription());

  if( sender() != bgMotor )
    return;

  onTransInChanges();
  onTransOutChanges();
  onTransIntervalChanges();

}

void MainWindow::onTransPosChanges() {
  setEnv("TRANSPOS", bgMotor->get());
}

void MainWindow::onTransIntervalChanges() {

  const int interval = ui->transInterval->value();
  setEnv("TRANSINTERVAL", interval);

  if (interval == ui->transInterval->minimum())
    transQty = 0;
  else
    transQty = ceil(ui->projections->value()/(float)interval) +
        ( ui->scanAdd->isChecked() ? 1 : 0 ) ;

  setEnv("TRANSN", transQty);

  bool itemOK =
      bgMotor->isConnected() == (bool) transQty;
  check(ui->transInterval , itemOK);

  ui->bgFile->setEnabled(transQty);

  if ( sender() != ui->transInterval )
    return;

  onTransMotorChanges();
  onBgFileChanges();
  onTransDistChanges();
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

  bool itemOK = ! bgMotor->isConnected() || (
        pos >= bgMotor->getUserLoLimit()  &&
        pos <= bgMotor->getUserHiLimit() );
  check(ui->transIn, itemOK);

  if( sender() != ui->transIn )
    return;

  ui->transOut->setValue(pos+ui->transDist->value());
  setScanTable();

}

void MainWindow::onTransOutChanges() {

  const double pos = ui->transOut->value();
  setEnv("TRANSOUT", pos);

  bool itemOK = ! bgMotor->isConnected() || (
        pos >= bgMotor->getUserLoLimit()  &&
        pos <= bgMotor->getUserHiLimit() );
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

  setScanTable();

}

void MainWindow::onShutterMan(){
  ui->shutterMan->setEnabled(false);
  QTimer::singleShot(2000, this, SLOT(onShutterChanges()));
  shutterMan( ! shutterStatus );
}

void MainWindow::onShutterChanges() {
  QEpicsPV * ss = (QEpicsPV *) sender();
  ui->shutterMan->setEnabled(true);
  if ( ss == clsSts )
    shutterStatus = ! clsSts->get().toBool();
  else if ( ss == opnSts )
    shutterStatus = opnSts->get().toBool();
  ui->shutterStatus->setText( QString("Shutter: ") +
                             (shutterStatus ? "opened" : "closed"));
  ui->shutterMan->setText( shutterStatus ? "Close" : "Open");
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
  if( sender() != ui->singleShot  &&  sender() != ui->multiShot )
    return;
  setScanTable();
}

void MainWindow::onLoopPosChanges() {
  setEnv("LOOPPOS", loopMotor->get());
}

void MainWindow::onLoopMotorChanges() {

  bool itemOK =
      ui->singleShot->isChecked() || loopMotor->isConnected();
  check(loopMotor->basicUI()->setup, itemOK);

  setEnv("LOOPMOTORPV", loopMotor->getPv());
  setEnv("LOOPMOTORDESC", loopMotor->getDescription());

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
      ui->singleShot->isChecked() || ! loopMotor->isConnected() ||
      ( pos >= loopMotor->getUserLoLimit()  &&
       pos <= loopMotor->getUserHiLimit() );
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
      ui->singleShot->isChecked() || ! loopMotor->isConnected() ||
      ( pos >= loopMotor->getUserLoLimit()  &&
       pos <= loopMotor->getUserHiLimit() );
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
  setEnv("SUBLOOPPOS", subLoopMotor->get() );
}

void MainWindow::onSubLoopMotorChanges() {

  bool itemOK = ! ui->subLoop->isChecked() ||
      ui->singleShot->isChecked() || subLoopMotor->isConnected();
  check(subLoopMotor->basicUI()->setup, itemOK);

  setEnv("SUBLOOPMOTORPV", subLoopMotor->getPv());
  setEnv("SUBLOOPMOTORDESC", subLoopMotor->getDescription());

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
      ui->singleShot->isChecked() || ! subLoopMotor->isConnected() ||
      ( pos >= subLoopMotor->getUserLoLimit()  &&
       pos <= subLoopMotor->getUserHiLimit() );
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
      ui->singleShot->isChecked() || ! subLoopMotor->isConnected() ||
      ( pos >= subLoopMotor->getUserLoLimit()  &&
       pos <= subLoopMotor->getUserHiLimit() );
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
  onDetectorCommandChanges(); // to enable/disable acquisition button
}

void MainWindow::onDynoPosChanges(){
  setEnv("DYNOPOS", dynoMotor->get());
}

void MainWindow::setDynoUnits(){

  const QString units = dynoMotor->getUnits();
  setEnv("DYNOMOTORUNITS", units);

  if ( sender() != dynoMotor)
    return;

  ui->dynoCurrent->setSuffix(units);
  ui->dynoRange->setSuffix(units);
  ui->dynoStart->setSuffix(units);
  ui->loopEnd->setSuffix(units);

}

void MainWindow::setDynoPrec() {

  const int prec = dynoMotor->getPrecision();
  setEnv("DYNOMOTORPREC", prec);

  if ( sender() != dynoMotor)
    return;

  ui->dynoCurrent->setDecimals(prec);
  ui->dynoRange->setDecimals(prec);
  ui->dynoStart->setDecimals(prec);
  ui->dynoEnd->setDecimals(prec);

}

void MainWindow::onDynoMotorChanges(){

  bool itemOK =
      ! ui->dynoShot->isChecked() || dynoMotor->isConnected();
  check(dynoMotor->basicUI()->setup, itemOK);

  setEnv("DYNOMOTORPV", dynoMotor->getPv());
  setEnv("DYNOMOTORDESC", dynoMotor->getDescription());

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
      ! ui->dynoShot->isChecked() || ! dynoMotor->isConnected() ||
      ( pos >= dynoMotor->getUserLoLimit()  &&
       pos <= dynoMotor->getUserHiLimit() );
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
      ! ui->dynoShot->isChecked() || ! dynoMotor->isConnected() ||
      ( pos >= dynoMotor->getUserLoLimit()  &&
       pos <= dynoMotor->getUserHiLimit() );
  check(ui->dynoEnd, itemOK);

  if( sender() != ui->dynoEnd)
    return;

  double newRange = pos - ui->dynoStart->value();

  ui->dynoRange->blockSignals(true);
  ui->dynoRange->setValue( newRange );
  ui->dynoRange->blockSignals(false);

}



void MainWindow::onDyno2Changes() {
  onDyno2MotorChanges();
  onDyno2StartChanges();
  onDyno2EndChanges();
  onDyno2RangeChanges();
}

void MainWindow::onDyno2PosChanges(){
  setEnv("DYNO2POS", dyno2Motor->get());
}

void MainWindow::setDyno2Units(){

  const QString units = dyno2Motor->getUnits();
  setEnv("DYNO2MOTORUNITS", units);

  if ( sender() != dyno2Motor)
    return;

  ui->dyno2Current->setSuffix(units);
  ui->dyno2Range->setSuffix(units);
  ui->dyno2Start->setSuffix(units);
  ui->loopEnd->setSuffix(units);

}

void MainWindow::setDyno2Prec() {

  const int prec = dyno2Motor->getPrecision();
  setEnv("DYNO2MOTORPREC", prec);

  if ( sender() != dyno2Motor)
    return;

  ui->dyno2Current->setDecimals(prec);
  ui->dyno2Range->setDecimals(prec);
  ui->dyno2Start->setDecimals(prec);
  ui->dyno2End->setDecimals(prec);

}

void MainWindow::onDyno2MotorChanges(){

  bool itemOK =
      ! ui->dyno2Shot->isChecked() ||
      ! ui->dynoShot->isChecked() || dyno2Motor->isConnected();
  check(dyno2Motor->basicUI()->setup, itemOK);

  setEnv("DYNO2MOTORPV", dyno2Motor->getPv());
  setEnv("DYNO2MOTORDESC", dyno2Motor->getDescription());

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
      ! ui->dynoShot->isChecked() || ! dyno2Motor->isConnected() ||
      ( pos >= dyno2Motor->getUserLoLimit()  &&
       pos <= dyno2Motor->getUserHiLimit() );
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
      ! ui->dynoShot->isChecked() || ! dyno2Motor->isConnected() ||
      ( pos >= dyno2Motor->getUserLoLimit()  &&
       pos <= dyno2Motor->getUserHiLimit() );
  check(ui->dyno2End, itemOK);

  if( sender() != ui->dyno2End)
    return;

  double newRange = pos - ui->dyno2Start->value();

  ui->dyno2Range->blockSignals(true);
  ui->dyno2Range->setValue( newRange );
  ui->dyno2Range->blockSignals(false);

}







void MainWindow::onSampleFileChanges() {

  QString res, text = ui->sampleFile->text();
  int evalOK = ! evalExpr(text, res);

  check(ui->sampleFile, ! text.isEmpty() && evalOK );

  ui->exampleImageName->setText( res );
  ui->exampleImageName->setStyleSheet( evalOK  ?  ""  :  "color: rgba(255, 0, 0, 128);");

}

void MainWindow::onBgFileChanges() {

  QString res, text = ui->bgFile->text();
  int evalOK = ! evalExpr(text, res);
  bool itemOK =  ! ui->bgFile->isEnabled() || text.isEmpty() || evalOK;

  check(ui->bgFile, itemOK);

  ui->exampleImageName->setText( res );
  ui->exampleImageName->setStyleSheet( evalOK  ?  ""  :  "color: rgba(255, 0, 0, 128);");

}

void MainWindow::onDfFileChanges() {

  QString res, text = ui->dfFile->text();
  bool evalOK = ! evalExpr(text, res);
  bool itemOK =  ! ui->dfFile->isEnabled() || text.isEmpty() || evalOK ;

  check(ui->dfFile, itemOK);

  ui->exampleImageName->setText( res );
  ui->exampleImageName->setStyleSheet( evalOK  ?  ""  :  "color: rgba(255, 0, 0, 128);");

}

 void MainWindow::onDetectorCommandChanges(){

  bool scriptOK = prepareExec(aqExec, ui->detectorCommand, ui->detectorError);
  bool itemOK = ! ui->detectorCommand->toPlainText().isEmpty() && scriptOK;
  check(ui->detectorCommand, itemOK);

  if (preReq.contains(ui->tabDyno))
    itemOK &= preReq[ui->tabDyno].first;
  ui->acquire->setEnabled(itemOK);

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
  }
  if ( status )
    foreach ( ReqP tabel, preReq)
      if ( ! tabel.second )
        allOK &= tabel.first;
  ui->startStop->setEnabled(allOK);

}




bool MainWindow::shutterMan(bool st, bool wait) {

  if (shutterStatus == st)
    return shutterStatus;

  if (st)
    appendMessage(CONTROL, "Openning shutter.");
  else
    appendMessage(CONTROL, "Closing shutter.");

  qDebug() << "Shutter" << ( shutterStatus = st );

  if (st) opnCmd->set(1);
  else clsCmd->set(1);

  if ( ! wait )
    return shutterStatus;

  QEventLoop q;
  QTimer tT;
  tT.setSingleShot(true);

  connect(&tT, SIGNAL(timeout()), &q, SLOT(quit()));
  connect(opnSts, SIGNAL(valueUpdated(QVariant)), &q, SLOT(quit()));
  connect(opnSts, SIGNAL(connectionChanged(bool)), &q, SLOT(quit()));
  connect(clsSts, SIGNAL(valueUpdated(QVariant)), &q, SLOT(quit()));
  connect(clsSts, SIGNAL(connectionChanged(bool)), &q, SLOT(quit()));


  const int delay = 5000;  // 5 sec total delay
  tT.start(delay);
  while ( shutterStatus != st  &&  tT.isActive() )
    q.exec();

  return shutterStatus;

}

void MainWindow::acquire(const QString & filename) {

  if ( ! ui->acquire->isEnabled() ) {
    appendMessage(ERROR, "Image cannot be acquired now.");
    return;
  }
  if ( filename.isEmpty() ) {
    appendMessage(ERROR, "Empty filename to store image.");
    return;
  }

  const double dStart = ui->dynoStart->value(), d2Start = ui->dyno2Start->value();

  if ( ui->dynoShot->isChecked() ) {
    dynoMotor->wait_stop();
    double goal = ui->dynoEnd->value();
    appendMessage(CONTROL, "Start moving dyno-shot motor to " + QString::number(goal) + ".");
    dynoMotor->goUserPosition(goal, false);
  }
  if ( ui->dyno2Shot->isChecked() ) {
    dyno2Motor->wait_stop();
    double goal = ui->dyno2End->value();
    appendMessage(CONTROL, "Start moving second dyno-shot motor to " + QString::number(goal) + ".");
    dyno2Motor->goUserPosition(goal, false);
  }

  setEnv("FILE", filename);
  bool execStatus = doExec(aqExec, "image acquisition into \"" + filename + "\"");

  if ( ui->dynoShot->isChecked() ) {
    appendMessage(CONTROL, "Stopping dyno-shot motor.");
    dynoMotor->stop(true);
    appendMessage(CONTROL, "Returning dyno-shot motor to " + QString::number(dStart) + ".");
    dynoMotor->goUserPosition(dStart, false);
  }
  if ( ui->dyno2Shot->isChecked() ) {
    appendMessage(CONTROL, "Stopping dyno-shot motor.");
    dyno2Motor->stop(true);
    appendMessage(CONTROL, "Returning second dyno-shot motor to " + QString::number(d2Start) + ".");
    dyno2Motor->goUserPosition(d2Start, false);
  }

  // preview
  if ( ! execStatus && ui->livePreview->isChecked() )
      ui->image->setStyleSheet("image: url(" + filename + ");");

  if ( ui->dynoShot->isChecked() )
    dynoMotor->wait_stop();
  if ( ui->dyno2Shot->isChecked() )
    dyno2Motor->wait_stop();

}


void MainWindow::onAcquire() {

  setEnv("AQTYPE", "SINGLESHOT");
  acquire(".temp.tif");

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
  if (engineIsOn) {
    stopMe = true;
    emit requestToStopAcquisition();
    thetaMotor->stop();
    bgMotor->stop();
    loopMotor->stop();
    subLoopMotor->stop();
  } else {
    for (int icur=0 ; icur < scanList->rowCount() ; icur++)
      if ( scanList->item(icur,0)->checkState() == Qt::Checked )
        scanList->item(icur,3)->setText("");
    engine(false);
  }
}




void MainWindow::appendScanListRow(Role rl, double pos, const QString & fn) {

  QList<QStandardItem *> items;
  QStandardItem * ti;

  ti = new QStandardItem(true);
  ti->setCheckable(true);
  ti->setCheckState(Qt::Checked);
  items << ti;

  ti = new QStandardItem(true);
  ti->setText(roleName(rl));
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
  while ( res.isEmpty() || QFile::exists(res) || listHasFile(res) ) {
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








void MainWindow::engine (const bool dryRun) {

  if ( ! dryRun )
    appendMessage(CONTROL, "Starting engine.");

  stopMe = false;
  engineIsOn = true;

  const bool
      scanAdd = ui->scanAdd->isChecked(),
      doBg = transQty,
      doL = ui->multiShot->isChecked(),
      doSL = doL && ui->subLoop->isChecked(),
      multiBg = doL && ui->multiBg->isChecked();
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
  } else {
    ui->progressBar->setRange(0,expCount);
    ui->progressBar->reset();
  }


  QString filename, aqErr, aqOut;;
  int
      count = 0,
      dfCount = 0,
      smCount = 0;



  if ( ! dryRun ) {
    onPreExec();
    setEnv("AQTYPE", "DARKCURRENT");
    setEnv("DFTYPE", "BEFORE");
  }

  for ( int j = 0 ; j < ui->dfBefore->value() ; ++j )  {

    if ( ! dryRun  &&  doIt(count) ) {

      setEnv("DFBEFORECOUNT", j+1);
      setEnv("DFCOUNT", dfCount+1);
      filename = setAqFileEnv(ui->dfFile, "DFFILE");

      shutterMan(false,true);
      scanList->item(count,3)->setText(filename);
      acquire(filename);

    }

    if ( dryRun )
      appendScanListRow(DF, 0, filename);

    QCoreApplication::processEvents();
    count++;
    dfCount++;
    if (!dryRun)
      ui->progressBar->setValue(count);
    if (stopMe) {
      if (!dryRun)
        appendMessage(CONTROL, "Stoppping engine.");
      engineIsOn = false;
      emit engineStoped();
      return;
    }

  }

  int bgBeforeNext=0, bgCount=0;
  bool isBg, wasBg = false;
  double
      lastScan = thetaMotor->get(),
      lastTrans = bgMotor->get(),
      lastLoop = loopMotor->get(),
      lastSubLoop = subLoopMotor->get();

  // here ( doBg && scanAdd && ! wasBg ) needed to take last background
  // if ui->scanAdd->isChecked()
  while ( smCount < tProjs || ( doBg && scanAdd && ! wasBg ) ) {

    // here "cProj == tProj" needed to take last background if ui->scanAdd->isChecked()
    isBg  =  doBg && ( ! bgBeforeNext  ||  smCount == tProjs );

    double cPos = start + smCount * (end - start) / projs;

    if (!dryRun)
      setEnv("GSCANPOS", cPos);

    double transGoal;
    if ( isBg ) {
      transGoal = transOut;
      bgBeforeNext = bgInterval;
      wasBg = true;
      if (!dryRun) {
        setEnv("AQTYPE", "BACKGROUND");
        setEnv("BGCOUNT", bgCount+1);
      }
    } else {
      transGoal = transIn;
      bgBeforeNext--;
      wasBg = false;
      if (!dryRun) {
        setEnv("AQTYPE", "SAMPLE");
        setEnv("PCOUNT", smCount+1);
      }
    }
    setEnv("GTRANSPOS", transGoal);

    int loopN =  ( ! doL || (isBg && ! multiBg) )  ?  1  :  lN;
    int subLoopN = ( loopN <= 1 || ! doSL )  ?  1  :  slN;

    for ( int x = 0; x < loopN; x++) {

      double loopPos = lStart + x * (lEnd-lStart) / (loopN-1);
      if (!dryRun) {
        setEnv("LOOPCOUNT", x+1);
        setEnv("GLOOPPOS", loopPos);
      }

      for ( int y = 0; y < subLoopN; y++) {

        double subLoopPos = slStart + y * (slEnd - slStart) / (subLoopN-1);

        if ( ! dryRun ) {

          setEnv("SUBLOOPCOUNT", y+1);
          setEnv("GSUBLOOPPOS", subLoopPos);
          setEnv("COUNT", count+1);

          if (doIt(count)) {

            shutterMan(true,true);

            if ( lastTrans != transGoal ) {
              appendMessage(CONTROL, "Moving translation motor to " + QString::number(transGoal) + ".");
              //bgMotor->goUserPosition(transGoal, false);
              lastTrans = transGoal;
            }
            if ( ! isBg && lastScan != cPos) {
              appendMessage(CONTROL, "Moving scan motor to " + QString::number(cPos) + ".");
              //thetaMotor->goUserPosition(cPos, false);
              lastScan=cPos;
            }
            if ( doL  &&  lastLoop != loopPos ) {
              appendMessage(CONTROL, "Moving loop motor to " + QString::number(loopPos) + ".");
              //loopMotor->goUserPosition(loopPos, false);
              lastLoop = loopPos;
            }
            if ( doSL  &&  lastSubLoop != subLoopPos ) {
              appendMessage(CONTROL, "Moving sub-loop motor to " + QString::number(subLoopPos) + ".");
              //subLoopMotor->goUserPosition(subLoopPos, false);
              lastSubLoop = subLoopPos;
            }

            bgMotor->wait_stop();
            thetaMotor->wait_stop();
            loopMotor->wait_stop();
            subLoopMotor->wait_stop();

            filename =  isBg  ?
                  setAqFileEnv(ui->bgFile,     "BGFILE")  :
                  setAqFileEnv(ui->sampleFile, "SAMPLEFILE");
            scanList->item(count,3)->setText(filename);

            acquire(filename);

          }

        } else {

          if (y)
            appendScanListRow(SLOOP, subLoopPos, filename);
          else if (x)
            appendScanListRow(LOOP, loopPos, filename);
          else if (isBg)
            appendScanListRow(BG, transOut, filename);
          else
            appendScanListRow(SAMPLE, cPos, filename);

        }

        QCoreApplication::processEvents();
        count++;
        if (!dryRun)
          ui->progressBar->setValue(count);
        if (stopMe) {
          if (!dryRun)
            appendMessage(CONTROL, "Stoppping engine.");
          engineIsOn = false;
          emit engineStoped();
          return;
        }

      }

    }

    if ( ! isBg ) smCount++;
    else bgCount++;

  }

  if ( ! dryRun ) {
    setEnv("AQTYPE", "DARKCURRENT");
    setEnv("DFTYPE", "AFTER");
  }

  for ( int j = 0 ; j < ui->dfAfter->value() ; ++j ) {

    if ( ! dryRun  &&  doIt(count) ) {

      setEnv("DFAFTERCOUNT", j+1);
      setEnv("DFCOUNT", dfCount+1);
      filename = setAqFileEnv(ui->dfFile, "DFFILE");

      shutterMan(false,true);

      scanList->item(count,3)->setText(filename);
      acquire(filename);

    }

    if ( dryRun )
      appendScanListRow(DF, 0, filename);

    QCoreApplication::processEvents();
    count++;
    dfCount++;
    if (!dryRun)
      ui->progressBar->setValue(count);
    if (stopMe) {
      if (!dryRun)
        appendMessage(CONTROL, "Stoppping engine.");
      engineIsOn = false;
      emit engineStoped();
      return;
    }

  }

  if ( ! dryRun ) {
    onPostExec();
    appendMessage(CONTROL, "Scan is finished. Stoppping engine.");
  }
  engineIsOn = false;
  emit engineStoped();

}


