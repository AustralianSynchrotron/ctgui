#ifndef CTGUI_MAINWINDOW_H
#define CTGUI_MAINWINDOW_H

#include <QMainWindow>
#include <qcamotorgui.h>
#include <qtpvwidgets.h>
#include <QIcon>
#include <QAbstractItemModel>
#include <QSortFilterProxyModel>
#include <QStandardItem>
#include <QFile>
#include <QLineEdit>
#include <QTime>
#include <QThread>
#include <QAbstractSpinBox>

#include "shutter.h"
#include "detector.h"
#include "triggct.h"
#include "positionlist.h"

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

  Detector::ImageFormat uiImageFormat() const;

  static const QString storedState;
  bool isLoadingState;

  Ui::MainWindow *ui;
  QHash<QObject*, QString> configNames;

  Shutter * shutter;
  Detector * det;
  TriggCT * tct;

  QCaMotorGUI * thetaMotor;
  QCaMotorGUI * bgMotor;
  QCaMotorGUI * dynoMotor;
  QCaMotorGUI * dyno2Motor;

  typedef QPair <bool,const QWidget*> ReqP;
  QHash <const QObject*,  ReqP > preReq;

  QList< QMDoubleSpinBox* > prsSelection;
  QMDoubleSpinBox * selectedPRS() const ;

  void check(QWidget * obj, bool status);

  void engineRun();

  bool stopMe;

  enum AqMode {
    STEPNSHOT = 0,
    FLYSOFT   = 1,
    FLYHARD2B = 2,
    FLYHARD3BTABL = 3,
    FLYHARD3BLAPS = 4,
    AQMODEEND = 5
  };

  static inline QString AqModeString(AqMode aqmd) {
    switch (aqmd) {
    case STEPNSHOT: return "Step-and-shot";
    case FLYSOFT  : return "On-the-fly (software)";
    case FLYHARD2B: return "On-the-fly (harware in 2B)";
    case FLYHARD3BTABL: return "On-the-fly (harware in 3B on table)";
    case FLYHARD3BLAPS: return "On-the-fly (harware in 3B on LAPS)";
    default       : return "ERROR";
    }
  }


  bool prepareDetector(const QString & filetemplate, int count=1);
  int acquireDetector();
  int acquireDetector(const QString & filetemplate, int count=1);
  int acquireDyno(const QString & filetemplate, int count=1);
  int acquireMulti(const QString & filetemplate, int count=1);
  int acquireBG(const QString &filetemplate);
  int acquireDF(const QString &filetemplate, Shutter::State stateToGo);

//  QFile * logFile;
//  QStringList accumulatedLog;

  void stopAll();

  void addMessage(const QString & str);

private slots:

  void saveConfiguration(QString fileName = QString());
  void loadConfiguration(QString fileName = QString());

  void storeCurrentState();

  void updateUi_expPath();
  void updateUi_pathSync(); // important to have this and previous line in this order as the latter one relies on the jobs done in the first.
  void updateUi_aqMode();

  void onWorkingDirBrowse();
  void onSerialCheck();
  void onFFcheck();
  void onDynoCheck();
  void onMultiCheck();

  void updateUi_serials();
  void updateUi_ffOnEachScan();
  void onSwapSerial();
  void onSerialTest();

  void updateUi_scanRange();
  void updateUi_scanStep();
  void updateUi_aqsPP();
  void updateUi_expOverStep();
  void updateUi_thetaMotor();
  QMDoubleSpinBox * selectPRS(QObject* prso = 0);

  void updateUi_bgTravel();
  void updateUi_bgInterval();
  void updateUi_dfInterval();
  void updateUi_bgMotor();
  // void updateUi_shutterStatus();
  void onFFtest();

  void updateUi_loops();
  void onSwapLoops();
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

  void onStartStop();


signals:

  void requestToStopAcquisition();

};


#endif // CTGUI_MAINWINDOW_H
