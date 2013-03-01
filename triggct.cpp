#include "triggct.h"
#include <QTime>

TriggCT::TriggCT(QObject *parent) :
  QObject(parent),
  modePv(new QEpicsPv(this)),
  modePvRBV(new QEpicsPv(this)),
  startPosPv(new QEpicsPv(this)),
  stopPosPv(new QEpicsPv(this)),
  stepPv(new QEpicsPv(this)),
  pulseWidthPv(new QEpicsPv(this)),
  iAmConnected(false)
{

  foreach(QEpicsPv * pv, findChildren<QEpicsPv*>())
    connect(pv, SIGNAL(connectionChanged(bool)), SLOT(updateConnection()));

  connect(modePvRBV, SIGNAL(valueChanged(QVariant)), SLOT(updateMode()));

}

void TriggCT::updateConnection() {
  bool con = true;
  foreach(QEpicsPv * pv, findChildren<QEpicsPv*>())
    con &= pv->isConnected();
  if (con != iAmConnected)
    emit connectionChanged(iAmConnected=con);
}

void TriggCT::updateMode() {
  if ( ! modePvRBV->isConnected() )
    return;
  emit runningChanged( isRunning() );
}


bool TriggCT::setPrefix(const QString & prefix, bool wait) {

  modePv->setPV(prefix+":CTRL");
  modePvRBV->setPV(prefix+":CTRL:RBV");
  startPosPv->setPV(prefix+":START");
  stopPosPv->setPV(prefix+":STOP");
  stepPv->setPV(prefix+":STEP");
  pulseWidthPv->setPV(prefix+":PULSE");
  updateConnection();

  if ( wait && ! isConnected() ) {
    QTime accTime;
    accTime.start();
    while ( ! isConnected() && accTime.elapsed() < 500 )
    qtWait(this, SIGNAL(connectionChanged(bool)), 500);
  }

  return isConnected();

}


bool TriggCT::setStartPosition(double pos, bool wait) {

  if ( ! startPosPv->isConnected() )
    return false;

  startPosPv->set(pos);

  if ( wait ) {
    QTime accTime;
    accTime.start();
    while ( startPosPv->isConnected() &&
            startPosition() != pos &&
            accTime.elapsed() < 500 )
      qtWait(startPosPv, SIGNAL(valueUpdated(QVariant)), 500);
  }

  return startPosition() == pos;

}

bool TriggCT::setStopPosition(double pos, bool wait) {

  if ( ! stopPosPv->isConnected() )
    return false;

  stopPosPv->set(pos);

  if ( wait ) {
    QTime accTime;
    accTime.start();
    while ( stopPosPv->isConnected() &&
            stopPosition() != pos &&
            accTime.elapsed() < 500 )
      qtWait(stopPosPv, SIGNAL(valueUpdated(QVariant)), 500);
  }

  return stopPosition() == pos;

}

bool TriggCT::setStep(double _step, bool wait) {
  if ( ! stepPv->isConnected() )
    return false;

  stepPv->set(_step);

  if ( wait ) {
    QTime accTime;
    accTime.start();
    while ( stepPv->isConnected() &&
            step() != _step &&
            accTime.elapsed() < 500 )
      qtWait(stepPv, SIGNAL(valueUpdated(QVariant)), 500);
  }

  return step() == _step;

}


bool TriggCT::start(bool wait) {
  if ( ! modePv->isConnected() )
    return false;
  modePv->set("Auto");
  if (wait) {
    QTime accTime;
    accTime.start();
    while ( modePvRBV->isConnected() &&
            modePvRBV->get().toString() != "Auto" &&
            accTime.elapsed() < 500 )
      qtWait(this, SIGNAL(runningChanged(bool)), 500);
  }
  return isRunning();
}


bool TriggCT::stop(bool wait) {
  if ( ! modePv->isConnected() )
    return false;
  modePv->set("Off");
  if (wait) {
    QTime accTime;
    accTime.start();
    while ( modePvRBV->isConnected() &&
            modePvRBV->get().toString() != "Off" &&
            accTime.elapsed() < 500 )
      qtWait(this, SIGNAL(runningChanged(bool)), 500);
  }
  return ! isRunning();
}

