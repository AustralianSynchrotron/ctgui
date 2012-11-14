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

#include <imbl/shutter1A.h>
#include "detector.h"





namespace Ui {
  class MainWindow;
  class HelpDialog;
}









class MainWindow : public QMainWindow
{
  Q_OBJECT;

public:

  explicit MainWindow(QWidget *parent = 0);

  ~MainWindow();

private:

  static QHash<QString,QString> envDesc;

  static const QString storedState;
  bool isLoadingState;

  static const bool inited;
  static bool init();

  Ui::MainWindow *ui;
  Ui::HelpDialog *hui;

  QDialog *hDialog;


  Shutter1A * sh1A;
  QCaMotorGUI * serialMotor;
  QCaMotorGUI * thetaMotor;
  QCaMotorGUI * bgMotor;
  QCaMotorGUI * loopMotor;
  QCaMotorGUI * subLoopMotor;
  QCaMotorGUI * dynoMotor;
  QCaMotorGUI * dyno2Motor;

  Detector * det;

  typedef QPair <bool,const QWidget*> ReqP;
  QHash <const QObject*,  ReqP > preReq;

  QString setAqFileEnv(const QLineEdit * edt, const QString & var);

  void check(QWidget * obj, bool status);

  void errorOnScan(const QString & msg) {
    qDebug() << msg;
  }

  void engine();


  bool inAcquisitionTest;
  bool inDynoTest;
  bool inMultiTest;
  bool inFFTest;
  bool inCT;
  QTime inCTtime;
  int currentScan;
  bool readyToStartCT;
  bool stopMe;



  enum MsgType {
    LOG,
    ERROR,
    CONTROL
  };
  void appendMessage(MsgType tp, const QString& msg);
  void logMessage(const QString& msg);

  void setEnv(const char * var, const char * val);

  inline void setEnv(const char * var, const QString & val) {
    setEnv(var, (const char*) val.toAscii());
  }

  inline void setEnv(const QString & var, const QString & val) {
    setEnv((const char*) var.toAscii(), (const char*) val.toAscii());
  }

  template<class NUM> inline void setEnv(const char * var, NUM val) {
    setEnv(var, QString::number(val));
  }

  QWidget * insertVariableIntoMe;

  bool prepareDetector(const QString & filetemplate, int count=1);
  int acquireDetector();
  int acquireDetector(const QString & filetemplate, int count=1);
  int acquireDyno(const QString & filetemplate, int count=1);
  int acquireMulti(const QString & filetemplate, int count=1);
  int acquireBG(const QString &filetemplate);
  int acquireDF(const QString &filetemplate);
  int acquireProjection(const QString &filetemplate);

  QFile * logFile;

  void stopAll();

private slots:

  void saveConfiguration(QString fileName = QString());
  void loadConfiguration(QString fileName = QString());

  void storeCurrentState();

  //void onHelpClecked(int row);

  void onWorkingDirChange();
  void onWorkingDirBrowse();
  void onAcquisitionMode();
  void onSerialCheck();
  void onFFcheck();
  void onDynoCheck();
  void onMultiCheck();

  void onEndCondition();
  void onSerialMotor();
  void onSerialStep();

  void onScanRange();
  void onThetaMotor();
  void onProjections();
  void onRotSpeed();

  void onBGmotor();
  void onNofBG();
  void onBGtravel();
  void onNofDF();
  void onShutterStatus();
  void onFFtest();

  void onLoopMotor();
  void onLoopStep();
  void onSubLoop();
  void onSubLoopMotor();
  void onSubLoopStep();
  void onLoopTest();

  void onDynoMotor();
  void onDyno2();
  void onDyno2Motor();
  void onDynoSpeedLock();
  void onDynoDirectionLock();
  void onDynoTest();

  void onDetectorSelection();
  void onDetectorUpdate();
  void onDetectorTest();
  void updateDetectorProgress();

  void onStartStop();
  void updateSeriesProgress(bool onTimer=true);

  void recordLog(const QString & message=QString());

signals:

  void requestToStopAcquisition();



};

#endif // CTGUI_MAINWINDOW_H
