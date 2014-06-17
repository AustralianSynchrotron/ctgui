#ifndef CTGUI_MAINWINDOW_H
#define CTGUI_MAINWINDOW_H

#include <QMainWindow>
#include <qcamotorgui.h>
#include <QIcon>
#include <QAbstractItemModel>
#include <QSortFilterProxyModel>
#include <QStandardItem>
#include <QFile>
#include <QLineEdit>
#include <QTime>
#include <QThread>

#include <imbl/shutter1A.h>
#include "detector.h"
#include "triggct.h"





namespace Ui {
  class MainWindow;
}


class MainWindow : public QMainWindow
{
  Q_OBJECT;

public:

  explicit MainWindow(QWidget *parent = 0);

  ~MainWindow();

private:

  static const QString storedState;
  bool isLoadingState;


  Ui::MainWindow *ui;

  Shutter1A * sh1A;
  QCaMotorGUI * serialMotor;
  QCaMotorGUI * thetaMotor;
  QCaMotorGUI * bgMotor;
  QCaMotorGUI * loopMotor;
  QCaMotorGUI * subLoopMotor;
  QCaMotorGUI * dynoMotor;
  QCaMotorGUI * dyno2Motor;

  Detector * det;

  TriggCT * tct;

  typedef QPair <bool,const QWidget*> ReqP;
  QHash <const QObject*,  ReqP > preReq;


  void check(QWidget * obj, bool status);

  void engineRun();


  bool inAcquisitionTest;
  bool inDynoTest;
  bool inMultiTest;
  bool inFFTest;
  bool inCT;
  QTime inCTtime;
  bool readyToStartCT;
  bool stopMe;

  int totalScans;
  int currentScan;
  int totalProjections;
  int currentProjection;
  int totalLoops;
  int currentLoop;
  int totalSubLoops;
  int currentSubLoop;
  int totalShots;
  int currentShot;


  enum MsgType {
    LOG,
    ERROR,
    CONTROL
  };
  void appendMessage(MsgType tp, const QString& msg);
  void logMessage(const QString& msg);

  bool prepareDetector(const QString & filetemplate, int count=1);
  int acquireDetector();
  int acquireDetector(const QString & filetemplate, int count=1);
  int acquireDyno(const QString & filetemplate, int count=1);
  int acquireMulti(const QString & filetemplate, int count=1);
  int acquireBG(const QString &filetemplate);
  int acquireDF(const QString &filetemplate, Shutter1A::State stateToGo);
  int acquireProjection(const QString &filetemplate);

  QFile * logFile;
  QStringList accumulatedLog;

  void stopAll();

  void addMessage(const QString & str);

private slots:

  void saveConfiguration(QString fileName = QString());
  void loadConfiguration(QString fileName = QString());

  void storeCurrentState();

  void updateUi_expPath();
  void updateUi_pathSync(); // important to have this and previous line in this order as the latter one relies on the jobs done in the first.
  void updateUi_triggCT();

  void updateUi_checkDyno();
  void updateUi_checkMulti();
  void onWorkingDirBrowse();
  void onAcquisitionMode();
  void onSerialCheck();
  void onFFcheck();
  void onDynoCheck();
  void onMultiCheck();

  void updateUi_nofScans();
  void updateUi_acquisitionTime();
  void updateUi_condtitionScript();
  void updateUi_serialStep();
  void updateUi_serialMotor();
  void updateUi_ffOnEachScan();
  void updateUi_serialList();

  void updateUi_scanRange();
  void updateUi_scanStep();
  void updateUi_aqsPP();
  void updateUi_rotSpeed();
  void updateUi_stepTime();
  void updateUi_expOverStep();
  void updateUi_thetaMotor();

  void updateUi_bgTravel();
  void updateUi_bgInterval();
  void updateUi_dfInterval();
  void updateUi_bgMotor();
  void updateUi_shutterStatus();
  void onFFtest();

  void updateUi_loopStep();
  void updateUi_loopMotor();
  void updateUi_subLoopStep();
  void updateUi_subLoopMotor();
  void onSubLoop();
  void onLoopTest();

  void updateUi_dynoSpeed();
  void updateUi_dynoMotor();
  void updateUi_dyno2Speed();
  void updateUi_dyno2Motor();
  void onDyno2();
  void onDynoSpeedLock();
  void onDynoDirectionLock();
  void onDynoTest();

  void updateUi_detector();
  void onDetectorSelection();
  void onDetectorTest();

  void updateProgress();
  void nameToLog(const QString & fileName);
  void accumulateLog();

  void onStartStop();


signals:

  void requestToStopAcquisition();



};


#endif // CTGUI_MAINWINDOW_H
