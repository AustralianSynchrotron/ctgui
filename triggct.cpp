#include "triggct.h"
#include <QElapsedTimer>
#include <QDebug>

TriggCT::TriggCT(QObject *parent)
  : QObject(parent)
  , zebraConnectedPv(new QEpicsPv(this))
  , gateStartPv(new QEpicsPv(this))
  , gateStartPvRBV(new QEpicsPv(this))
  , gateWidthPv(new QEpicsPv(this))
  , gateWidthPvRBV(new QEpicsPv(this))
  , gateStepPv(new QEpicsPv(this))
  , gateNofPv(new QEpicsPv(this))
  , pulseStartPv(new QEpicsPv(this))
  , pulseStartPvRBV(new QEpicsPv(this))
  , pulseWidthPv(new QEpicsPv(this))
  , pulseStepPv(new QEpicsPv(this))
  , pulseStepPvRBV(new QEpicsPv(this))
  , pulseNofPv(new QEpicsPv(this))
  , pulseNofPvRBV(new QEpicsPv(this))
  , armPv(new QEpicsPv(this))
  , armPvRBV(new QEpicsPv(this))
  , disarmPv(new QEpicsPv(this))
  , p1DelayPv(new QEpicsPv(this))
  , p1WidthPv(new QEpicsPv(this))
  , p1UnitsPv(new QEpicsPv(this))
  , p1InputPv(new QEpicsPv(this))
  , out1ttlPv(new QEpicsPv(this))
  , _prefix()
  , iAmConnected(false)
{
  foreach(QEpicsPv * pv, findChildren<QEpicsPv*>())
    connect(pv, SIGNAL(connectionChanged(bool)), SLOT(updateConnection()));
  connect(armPvRBV, SIGNAL(valueChanged(QVariant)), SLOT(updateRunning()));
}

void TriggCT::updateConnection() {
  bool con = true;
  foreach(QEpicsPv * pv, findChildren<QEpicsPv*>()) {
    con &= pv->isConnected();
  }
  if (con) {
    QVariant conres = zebraConnectedPv->get();
    con  &=  conres.canConvert<int>()  && conres.toInt(0) == 1;
  }
  if (con != iAmConnected)
    emit connectionChanged(iAmConnected=con);
}


void TriggCT::updateRunning() {
  emit runningChanged( isRunning() );
}


bool TriggCT::setPrefix(const QString & newprefix, bool wait) {

  _prefix = newprefix;

  zebraConnectedPv ->setPV(newprefix+":CONNECTED");
  gateStartPv      ->setPV(newprefix+":PC_GATE_START");
  gateStartPvRBV   ->setPV(newprefix+":PC_GATE_START:RBV");
  gateWidthPv      ->setPV(newprefix+":PC_GATE_WID");
  gateWidthPvRBV   ->setPV(newprefix+":PC_GATE_WID:RBV");
  gateStepPv       ->setPV(newprefix+":PC_GATE_STEP");
  gateNofPv        ->setPV(newprefix+":PC_GATE_NGATE");
  pulseStartPv     ->setPV(newprefix+":PC_PULSE_START");
  pulseStartPvRBV  ->setPV(newprefix+":PC_PULSE_START:RBV");
  pulseWidthPv     ->setPV(newprefix+":PC_PULSE_WID");
  pulseStepPv      ->setPV(newprefix+":PC_PULSE_STEP");
  pulseStepPvRBV   ->setPV(newprefix+":PC_PULSE_STEP:RBV");
  pulseNofPv       ->setPV(newprefix+":PC_PULSE_MAX");
  pulseNofPvRBV    ->setPV(newprefix+":PC_PULSE_MAX:RBV");
  armPv            ->setPV(newprefix+":PC_ARM");
  armPvRBV         ->setPV(newprefix+":PC_ARM_OUT");
  disarmPv         ->setPV(newprefix+":PC_DISARM");  
  p1DelayPv        ->setPV(newprefix+":PULSE1_DLY");
  p1WidthPv        ->setPV(newprefix+":PULSE1_WID");
  p1UnitsPv        ->setPV(newprefix+":PULSE1_PRE"); // seconds: 1
  p1InputPv        ->setPV(newprefix+":PULSE1_INP"); // 31 for Eiger
  out1ttlPv        ->setPV(newprefix+":OUT1_TTL"); // 52 for Eiger, 31 for others
  updateConnection();

  if ( wait && ! isConnected() ) {
    QElapsedTimer accTime;
    accTime.start();
    while ( ! isConnected() && accTime.elapsed() < 500 )
      qtWait(this, SIGNAL(connectionChanged(bool)), 500);
  }

  return isConnected();

}


bool TriggCT::setStartPosition(double pos, bool wait) {
  if ( ! isConnected() )
    return false;
  gateStartPv->set(pos);
  pulseStartPv->set(0);
  if (wait) {
    QElapsedTimer accTime;
    accTime.start();
    while ( isConnected() &&
            startPosition() != pos &&
            accTime.elapsed() < 500 )
      qtWait(gateStartPvRBV, SIGNAL(valueUpdated(QVariant)), 500);
  }
  return startPosition() == pos;
}


bool TriggCT::setStep(double stp, bool wait) {  
  if ( ! isConnected() )
    return false;
  pulseStepPv->set(stp);
  pulseWidthPv->set(stp*0.75);
  if ( wait ) {
    QElapsedTimer accTime;
    accTime.start();
    while ( isConnected() &&
            step() != stp &&
            accTime.elapsed() < 500 )
      qtWait(pulseStepPvRBV, SIGNAL(valueUpdated(QVariant)), 500);
  }
  return step() == stp;
}


bool TriggCT::setRange(double rng, bool wait) {
  if ( ! isConnected() )
    return false;
  gateWidthPv->set(rng);
  gateStepPv->set(rng+0.1);
  if ( wait ) {
    QElapsedTimer accTime;
    accTime.start();
    while ( isConnected() &&
            range() != rng &&
            accTime.elapsed() < 500 )
      qtWait(gateWidthPvRBV, SIGNAL(valueUpdated(QVariant)), 500);
  }
  return range() == rng;
}


bool TriggCT::setNofTrigs(int trgs, bool wait) {
  if ( ! isConnected() || trgs < 2 )
    return false;
  pulseNofPv->set(trgs);
  if ( wait ) {
    QElapsedTimer accTime;
    accTime.start();
    while ( isConnected() &&
            trigs() != trgs &&
            accTime.elapsed() < 500 )
      qtWait(pulseNofPvRBV, SIGNAL(valueUpdated(QVariant)), 500);
  }
  return trigs() == trgs;
}


bool TriggCT::start(bool wait) {
  if ( ! isConnected() )
    return false;
  if (isRunning())
    stop(true);
  if (isRunning()) // did not stop
    return false;
  gateNofPv->set(1);
  armPv->set(1);
  if (wait) {
    QElapsedTimer accTime;
    accTime.start();
    while ( isConnected() &&
            ! isRunning() &&
            accTime.elapsed() < 500 )
      qtWait(this, SIGNAL(runningChanged(bool)), 500);
  }
  return isRunning();
}


bool TriggCT::stop(bool wait) {
  if ( ! isConnected() )
    return false;
  disarmPv->set(1);
  if (wait) {
    QElapsedTimer accTime;
    accTime.start();
    while ( isConnected() &&
            isRunning() &&
            accTime.elapsed() < 500 )
      qtWait(this, SIGNAL(runningChanged(bool)), 500);
  }
  return ! isRunning();
}


bool TriggCT::setExposure(float exp_seconds) {
  if ( ! isConnected() )
    return false;
  if (exp_seconds == 0.0f) {
    out1ttlPv->set(31); // PC_PULSE
  } else {
    p1UnitsPv->set(1); // seconds
    p1DelayPv->set(0);
    p1WidthPv->set(exp_seconds);
    p1InputPv->set(31); // PC_PULSE
    out1ttlPv->set(52); // PULSE1
  }
}








