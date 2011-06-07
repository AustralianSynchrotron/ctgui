#ifndef CTGUI_MAINWINDOW_H
#define CTGUI_MAINWINDOW_H

#include <QMainWindow>
#include <qcamotorgui.h>
#include <QIcon>
#include <QAbstractItemModel>
#include <QSortFilterProxyModel>
#include <QStandardItem>
#include <QFile>

#include "ui_ctgui_mainwindow.h"
#include "ui_ctgui_variables.h"





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

  Ui::MainWindow *ui;
  Ui::HelpDialog *hui;

  QDialog *hDialog;

  QCaMotorGUI * thetaMotor;
  QCaMotorGUI * bgMotor;
  QCaMotorGUI * loopMotor;
  QCaMotorGUI * subLoopMotor;
  QCaMotorGUI * dynoMotor;
  QCaMotorGUI * dyno2Motor;

  QEpicsPv * opnSts;
  QEpicsPv * clsSts;
  QEpicsPv * opnCmd;
  QEpicsPv * clsCmd;
  bool shutterStatus;
  bool shutterMan(bool st, bool wait=false);


  static const QString shutterPvBaseName;

  typedef QPair <bool,const QWidget*> ReqP;
  QHash <const QObject*,  ReqP > preReq;

  static const QIcon goodIcon;
  static const QIcon badIcon;

  QStandardItemModel * scanList;
  QSortFilterProxyModel * proxyModel;

  enum Role {
    SAMPLE,
    DF,
    BG,
    LOOP,
    SLOOP
  };

  QString roleName(Role rl) const {
    switch (rl) {
    case SAMPLE: return "sample";
    case DF: return "dark field";
    case BG: return "background";
    case LOOP: return "loop";
    case SLOOP: return "sub-loop";
    }
    return QString();
  }

  bool listHasFile(const QString & fn);
  QString setAqFileEnv(const QLineEdit * edt, const QString & var);

  void appendScanListRow( Role rl, double pos, const QString & fn=QString() );

  int transQty;

  void check(QWidget * obj, bool status);

  QFile * aqExec;
  QFile * preExec;
  QFile * postExec;

  void errorOnScan(const QString & msg) {
    qDebug() << msg;
  }

  void acquire(const QString & filename);

  void engine(const bool dryRun);

  bool doIt(int count);

  bool stopMe;
  bool engineIsOn;

  int doExec(QFile * fileExec, const QString & action);
  bool prepareExec(QFile* fileExec, QPTEext * textW, QLabel * errorLabel);

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

  QList<QWidget*> widgetsNeededHelp;
  QWidget * insertVariableIntoMe;

private slots:

  void saveConfiguration(QString fileName = QString());
  void loadConfiguration(QString fileName = QString());

  void onFocusChange( QWidget * old, QWidget * now );
  void onHelpClecked(int row);

  void setThetaUnits();
  void setBgUnits();
  void setLoopUnits();
  void setSubLoopUnits();

  void setThetaPrec();
  void setBgPrec();
  void setLoopPrec();
  void setSubLoopPrec();

  void lockThetaStart(bool lock);
  void lockBgStart(bool lock);
  void lockLoopStart(bool lock);
  void lockSubLoopStart(bool lock);
  void lockDynoStart(bool lock);
  void lockDyno2Start(bool lock);

  void onBrowseExpPath();
  void onExpPathChanges();
  void onExpNameChanges();
  void onExpDescChanges();
  void onSampleDescChanges();
  void onPeopleListChanges();

  void onPreCommandChanges();
  void onPostCommandChanges();
  void onPreExec();
  void onPostExec();

  void onThetaMotorChanges();
  void onScanPosChanges();
  void onScanRangeChanges();
  void onScanStepChanges();
  void onProjectionsChanges();
  void onScanStartChanges();
  void onScanEndChanges();
  void onScanAddChanges();
  void onSelectiveScanChanges();
  void setScanTable();

  void onFilterChanges(const QString & text);
  void onSelectAll();
  void onSelectNone();
  void onSelectInvert();
  void onCheckAll();
  void onCheckNone();
  void onCheckInvert();

  void onTransMotorChanges();
  void onTransPosChanges();
  void onTransIntervalChanges();
  void onTransDistChanges();
  void onTransInChanges();
  void onTransOutChanges();
  void onDfChanges();
  void onShutterChanges();
  void onShutterMan();

  void onShotModeChanges();
  void onLoopMotorChanges();
  void onLoopPosChanges();
  void onLoopRangeChanges();
  void onLoopNumberChanges();
  void onLoopStepChanges();
  void onLoopStartChanges();
  void onLoopEndChanges();

  void onSubLoopChanges();
  void onSubLoopMotorChanges();
  void onSubLoopPosChanges();
  void onSubLoopRangeChanges();
  void onSubLoopNumberChanges();
  void onSubLoopStepChanges();
  void onSubLoopStartChanges();
  void onSubLoopEndChanges();

  void onDynoChanges();
  void onDynoPosChanges();
  void setDynoUnits();
  void setDynoPrec();
  void onDynoMotorChanges();
  void onDynoRangeChanges();
  void onDynoStartChanges();
  void onDynoEndChanges();

  void onDyno2Changes();
  void onDyno2PosChanges();
  void setDyno2Units();
  void setDyno2Prec();
  void onDyno2MotorChanges();
  void onDyno2RangeChanges();
  void onDyno2StartChanges();
  void onDyno2EndChanges();

  void onDetectorCommandChanges();
  void onSampleFileChanges();
  void onBgFileChanges();
  void onDfFileChanges();

  void onAcquire();
  void onStartStop();

signals:

  void requestToStopAcquisition();
  void engineStoped();

};

#endif // CTGUI_MAINWINDOW_H
