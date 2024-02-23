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

  static const QString storedState;
  explicit MainWindow(QWidget *parent = 0);
  ~MainWindow();

  QHash<QObject*, QString> configNames;

public slots:
  void saveConfiguration(QString fileName = QString());
  void loadConfiguration(QString fileName = QString());

private:

  Detector::ImageFormat uiImageFormat() const;

  bool isLoadingState;

  Ui::MainWindow *ui;
  float bgOrigin;
  float bgAcquire;
  float bgEnter;

  Shutter * shutterPri;
  Shutter * shutterSec;
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
    FLYHARD3B = 3,
    AQMODEEND = 5
  };

  static inline QString AqModeString(AqMode aqmd) {
    switch (aqmd) {
    case STEPNSHOT: return "Step-and-shot";
    case FLYSOFT  : return "On-the-fly (software)";
    case FLYHARD2B: return "On-the-fly (harware in 2B)";
    case FLYHARD3B: return "On-the-fly (harware in 3B)";
    default       : return "ERROR";
    }
  }


  bool prepareDetector(const QString & filetemplate, int count);
  int acquireDetector();
  int acquireDetector(const QString & filetemplate, int count=1);
  int acquireDyno(const QString & filetemplate, int count=1);
  int acquireMulti(const QString & filetemplate, int count=1);
  int acquireBG(const QString &filetemplate);
  int moveToBG();
  int acquireDF(const QString &filetemplate, Shutter::State stateToGo);

//  QFile * logFile;
//  QStringList accumulatedLog;

  void stopAll();

  void addMessage(const QString & str);

private slots:


  void storeCurrentState();

  void updateUi_expPath();
  void updateUi_pathSync(); // important to have this and previous line in this order as the latter one relies on the jobs done in the first.
  void updateUi_aqMode();
  void updateUi_hdf();

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
  void onScanTest();
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

  bool onVideoGetReady();
  void onVideoRecord();

  void onStartStop();

  QString mkRun(QAbstractButton * wdg, bool inr, const QString & txt=QString()); // mark button as running
  bool inRun(const QAbstractButton * wdg);



signals:

  void requestToStopAcquisition();

};


#endif // CTGUI_MAINWINDOW_H
