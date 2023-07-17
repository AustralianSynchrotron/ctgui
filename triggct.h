#ifndef TRIGGCT_H
#define TRIGGCT_H

#include <QObject>
#include <QPair>
#include <qtpv.h>

class TriggCT : public QObject
{
  Q_OBJECT;

private:

  QEpicsPv * zebraConnectedPv;
  QEpicsPv * gateStartPv;
  QEpicsPv * gateStartPvRBV;
  QEpicsPv * gateWidthPv;
  QEpicsPv * gateWidthPvRBV;
  QEpicsPv * gateStepPv;
  QEpicsPv * gateNofPv;
  QEpicsPv * pulseStartPv;
  QEpicsPv * pulseStartPvRBV;
  QEpicsPv * pulseWidthPv;
  QEpicsPv * pulseStepPv;
  QEpicsPv * pulseStepPvRBV;
  QEpicsPv * pulseNofPv;
  QEpicsPv * pulseNofPvRBV;
  QEpicsPv * armPv;
  QEpicsPv * armPvRBV;
  QEpicsPv * disarmPv;

  QString _prefix;
  bool iAmConnected;

public:
  explicit TriggCT(QObject *parent = 0);

  bool isConnected() const {return iAmConnected;}
  bool isRunning() const {return isConnected() && armPvRBV->get().toInt();}

  const QString & prefix() const {return _prefix;}
  double startPosition() const {return gateStartPvRBV->get().toDouble();}
  double step() const {return pulseStepPvRBV->get().toDouble();}
  double range() const {return gateWidthPvRBV->get().toDouble();}
  double trigs() const {return pulseNofPvRBV->get().toDouble();}

signals:
  void connectionChanged(bool);
  void runningChanged(bool);

public slots:

  bool setPrefix(const QString & newprefix, bool wait=false);
  bool setStartPosition(double pos, bool wait=false);
  bool setStep(double stp, bool wait=false);
  bool setRange(double rng, bool wait=false);
  bool setNofTrigs(int trgs, bool wait=false);
  bool start(bool wait=false);
  bool stop(bool wait=false);

private slots:
  void updateConnection();
  void updateRunning();

};




#endif // TRIGGCT_H
