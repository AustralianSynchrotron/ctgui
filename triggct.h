#ifndef TRIGGCT_H
#define TRIGGCT_H

#include <QObject>
#include <qtpv.h>

class TriggCT : public QObject
{
  Q_OBJECT;

private:

  QEpicsPv * modePv;
  QEpicsPv * modePvRBV;
  QEpicsPv * startPosPv;
  QEpicsPv * stopPosPv;
  QEpicsPv * stepPv;
  QEpicsPv * pulseWidthPv;

  bool iAmConnected;

public:
  explicit TriggCT(QObject *parent = 0);

  bool isConnected() const {return iAmConnected;}
  bool isRunning() const {return isConnected() &&
        modePvRBV->get().toString() == "Auto";}

  double startPosition() const {return startPosPv->get().toDouble();}
  double stopPosition() const {return stopPosPv->get().toDouble();}
  double step() const {return stepPv->get().toDouble();}

signals:
  void connectionChanged(bool con);
  void runningChanged(bool run);

public slots:
  bool setPrefix(const QString & prefix, bool wait=false);
  bool setStartPosition(double pos, bool wait=false);
  bool setStopPosition(double pos, bool wait=false);
  bool setStep(double _step, bool wait=false);
  bool start(bool wait=false);
  bool stop(bool wait=false);

private slots:
  void updateConnection();
  void updateMode();

};

#endif // TRIGGCT_H
