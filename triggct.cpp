#include "triggct.h"
#include <QTime>
#include <QDebug>

TriggCT::TriggCT(QObject *parent) :
  QObject(parent),
  outModePv(new QEpicsPv(this)),
  outModePvRBV(new QEpicsPv(this)),
  startModePv(new QEpicsPv(this)),
  startPosPv(new QEpicsPv(this)),
  startPosPvRBV(new QEpicsPv(this)),
  stepPv(new QEpicsPv(this)),
  nofTrigsPv(new QEpicsPv(this)),
  motorPv(),
  iAmConnected(false)
{

  foreach(QEpicsPv * pv, findChildren<QEpicsPv*>())
    connect(pv, SIGNAL(connectionChanged(bool)), SLOT(updateConnection()));
  connect(outModePvRBV, SIGNAL(valueChanged(QVariant)), SLOT(updateMode()));

}

void TriggCT::updateConnection() {
  bool con = true;
  foreach(QEpicsPv * pv, findChildren<QEpicsPv*>())
    con &= pv->isConnected();
  if (con)
    motorPv=QEpicsPv::get(_prefix+":MOTOR").toString();
  if (con != iAmConnected)
    emit connectionChanged(iAmConnected=con);
}


void TriggCT::updateMode() {
  if ( ! outModePvRBV->isConnected() )
    return;
  emit runningChanged( isRunning() );
}


bool TriggCT::setPrefix(const QString & prefix, bool wait) {

  _prefix = prefix;
  outModePv->setPV(prefix+":CTRL");
  outModePvRBV->setPV(prefix+":CTRL:RBV");
  startModePv->setPV(prefix+":STARTMODE");
  startPosPv->setPV(prefix+":START");
  startPosPvRBV->setPV(prefix+":START:RBV");
  stepPv->setPV(prefix+":STEP");
  nofTrigsPv->setPV(prefix+":NTRIGS");
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
    while ( startPosPvRBV->isConnected() &&
            startPosition() != pos &&
            accTime.elapsed() < 500 )
      qtWait(startPosPvRBV, SIGNAL(valueUpdated(QVariant)), 500);
  }

  return startPosition() == pos;

}

bool TriggCT::setStep(double stp, bool wait) {

  if ( ! stepPv->isConnected() )
    return false;

  stepPv->set(stp);

  if ( wait ) {
    QTime accTime;
    accTime.start();
    while ( stepPv->isConnected() &&
            step() != stp &&
            accTime.elapsed() < 500 )
      qtWait(stepPv, SIGNAL(valueUpdated(QVariant)), 500);
  }

  return step() == stp;

}

bool TriggCT::setNofTrigs(int trgs, bool wait) {
  if ( ! nofTrigsPv->isConnected() ||
       trgs < 2 )
    return false;

  nofTrigsPv->set(trgs);

  if ( wait ) {
    QTime accTime;
    accTime.start();
    while ( nofTrigsPv->isConnected() &&
            trigs() != trgs &&
            accTime.elapsed() < 500 )
      qtWait(nofTrigsPv, SIGNAL(valueUpdated(QVariant)), 500);
  }

  return trigs() == trgs;

}


bool TriggCT::start(bool wait) {
  if ( ! outModePv->isConnected() )
    return false;
  outModePv->set("Auto");
  if (wait) {
    QTime accTime;
    accTime.start();
    while ( outModePvRBV->isConnected() &&
            outModePvRBV->get().toString() != "Auto" &&
            accTime.elapsed() < 500 )
      qtWait(this, SIGNAL(runningChanged(bool)), 500);
  }
  return isRunning();
}


bool TriggCT::stop(bool wait) {
  if ( ! outModePv->isConnected() )
    return false;
  outModePv->set("Off");
  if (wait) {
    QTime accTime;
    accTime.start();
    while ( outModePvRBV->isConnected() &&
            outModePvRBV->get().toString() != "Off" &&
            accTime.elapsed() < 500 )
      qtWait(this, SIGNAL(runningChanged(bool)), 500);
  }
  return ! isRunning();
}

