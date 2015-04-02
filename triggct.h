#ifndef TRIGGCT_H
#define TRIGGCT_H

#include <QObject>
#include <QPair>
#include <qtpv.h>

class TriggCT : public QObject
{
  Q_OBJECT;

private:



  QEpicsPv * outModePv;
  QEpicsPv * outModePvRBV;
  QEpicsPv * startModePv;
  QEpicsPv * startPosPv;
  QEpicsPv * startPosPvRBV;
  QEpicsPv * stepPv;
  QEpicsPv * rangePv;
  QEpicsPv * nofTrigsPv;
  QString motorPv;
  QString _prefix;

  bool iAmConnected;

public:
  explicit TriggCT(QObject *parent = 0);

  bool isConnected() const {return iAmConnected;}

  bool isRunning() const {return isConnected() &&
        outModePvRBV->get().toString() == "Auto";}

  const QString & motor() const {return motorPv;}
  const QString & prefix() const {return _prefix;}
  double startPosition() const {return startPosPvRBV->get().toDouble();}
  double step() const {return stepPv->get().toDouble();}
  double range() const {return rangePv->get().toDouble();}
  double trigs() const {return nofTrigsPv->get().toDouble();}

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
  void updateMode();

};

#endif // TRIGGCT_H
