#include "detector.h"
#include <QDebug>
#include <QProcess>
#include <QTime>
#include <QApplication>

/*
const QHash<Detector::Camera, int> Detector::triggerModes = Detector::init_triggerModes();
QHash<Detector::Camera, int> Detector::init_triggerModes() {
  QHash<Camera, int> ret;
  ret.insert(NONE, 0);
  ret.insert(Ruby, 0);
  ret.insert(Amethyst, 0); // internal
  ret.insert(BYPV, 5); // fixed rate
  return ret;
}


const QHash<Detector::Camera, int> Detector::writeModes = Detector::init_writeModes();
QHash<Detector::Camera, int> Detector::init_writeModes() {
  QHash<Camera, int> ret;
  ret.insert(NONE, 0);
  ret.insert(Ruby, 0); // single
  ret.insert(Amethyst, 0); // single
  ret.insert(BYPV, 1); // captured
  return ret;
}

const QHash<Detector::Camera, int> Detector::imageModes = Detector::init_imageModes();
QHash<Detector::Camera, int> Detector::init_imageModes() {
  QHash<Camera, int> ret;
  ret.insert(NONE, 0);
  ret.insert(Ruby, 1); //multiple
  ret.insert(Amethyst, 0); // single
  ret.insert(BYPV, 1); // multiple
  return ret;
}
*/


Detector::Detector(QObject * parent) :
  QObject(parent),
  cam(NONE),
  expPv( new QEpicsPv(this) ),
  intPv( new QEpicsPv(this) ),
  numPv( new QEpicsPv(this) ),
  numCapPv( new QEpicsPv(this) ),
  numCompletePv( new QEpicsPv(this) ),
  triggerModePv( new QEpicsPv(this) ),
  imageModePv( new QEpicsPv(this) ),
  aqPv( new QEpicsPv(this) ),
  fileNumberPv( new QEpicsPv(this) ),
  fileNamePv( new QEpicsPv(this) ),
  autoSavePv( new QEpicsPv(this) ),
  capturePv( new QEpicsPv(this) ),
  writeModePv( new QEpicsPv(this) ),
  writeStatusPv( new QEpicsPv(this) ),
  _con(false),
  _counter(0),
  _name(QString())
{

  foreach( QEpicsPv * pv, findChildren<QEpicsPv*>() ) {
    connect(pv, SIGNAL(connectionChanged(bool)), SLOT(updateConnection()));
    connect(pv, SIGNAL(valueChanged(QVariant)), SLOT(updateReadyness()));
  }
  connect(numCompletePv, SIGNAL(valueUpdated(QVariant)), SLOT(updateCounter()));
  connect(&internalTimer, SIGNAL(timeout()), SLOT(trig()));
  connect(aqPv, SIGNAL(valueChanged(QVariant)), SLOT(updateAcquisition()));
  connect(expPv, SIGNAL(valueChanged(QVariant)), SLOT(updateExposure()));
  updateConnection();

}


void Detector::setCamera(Camera _cam) {

  cam=_cam;

  if (_cam == Amethyst) {
    const QString basePv = "ScintX1:";
    expPv->setPV(basePv+"cam1:AcquireTime");
    intPv->setPV(basePv+"cam1:AcquirePeriod");
    numPv->setPV(basePv+"cam1:NumImages");
    numCompletePv->setPV(basePv+"cam1:NumImages");
    numCapPv->setPV(basePv+"TIFF1:NumCapture");
    triggerModePv->setPV(basePv+"cam1:TriggerMode");
    imageModePv->setPV(basePv+"cam1:ImageMode");
    aqPv->setPV(basePv+"cam1:Acquire");
    fileNumberPv->setPV(basePv+"TIFF1:FileNumber");
    fileNamePv->setPV(basePv+"TIFF1:FileName");
    writeModePv->setPV(basePv+"TIFF1:FileWriteMode");
    capturePv->setPV(basePv+"TIFF1:Capture");
    autoSavePv->setPV(basePv+"TIFF1:AutoSave");
    writeStatusPv->setPV(basePv+"TIFF1:WriteFile_RBV");
  } else if (_cam == Ruby) {
      const QString basePv = "PCOIOC:";
      expPv->setPV(basePv+"cam1:AcquireTime_RBV");
      intPv->setPV(basePv+"cam1:AcquirePeriod");
      numCompletePv->setPV(basePv+"cam1:NumImagesCounter_RBV");
      numPv->setPV(basePv+"cam1:NumImages");
      numCapPv->setPV(basePv+"TIFF1:NumCapture");
      triggerModePv->setPV(basePv+"cam1:TriggerMode");
      imageModePv->setPV(basePv+"cam1:ImageMode");
      aqPv->setPV(basePv+"cam1:Acquire");
      fileNumberPv->setPV(basePv+"TIFF1:FileNumber");
      fileNamePv->setPV(basePv+"TIFF1:FileName");
      writeModePv->setPV(basePv+"TIFF1:FileWriteMode");
      capturePv->setPV(basePv+"TIFF1:Capture");
      autoSavePv->setPV(basePv+"TIFF1:AutoSave");
      writeStatusPv->setPV(basePv+"TIFF1:WriteFile_RBV");
  } else {
    cam=NONE;
    foreach( QEpicsPv * pv, findChildren<QEpicsPv*>() )
      pv->setPV("");
  }

}

void Detector::setCamera(const QString & pvName) {

  cam = BYPV;

  expPv->setPV(pvName+":AcquireTime");
  intPv->setPV(pvName+":AcquirePeriod");
  numPv->setPV(pvName+":NumImages");
  numCompletePv->setPV(pvName+":NumImages");
  numCapPv->setPV(pvName+":NumCapture");
  triggerModePv->setPV(pvName+":TriggerMode");
  imageModePv->setPV(pvName+":ImageMode");
  aqPv->setPV(pvName+":Acquire");
  fileNumberPv->setPV(pvName+":FileNumber");
  fileNamePv->setPV(pvName+":FileName");
  writeModePv->setPV(pvName+":FileWriteMode");
  capturePv->setPV(pvName+":Capture");
  autoSavePv->setPV(pvName+":AutoSave");
  writeStatusPv->setPV(pvName+":WriteFile_RBV");

}



void Detector::updateReadyness() {
  if (cam == NONE) {
    _isReady=false;
  } else {
    _isReady =
        isConnected() &&
        ! _counter &&
        ! aqPv->get().toInt() &&
        autoSavePv->get().toInt() /* &&
        triggerModePv->get().toInt() == triggerModes[cam] &&
        imageModePv->get().toInt() == imageModes[cam] &&
        writeModePv->get().toInt() == writeModes[cam] */ ;
  }
  emit readynessChanged(_isReady);
}



void Detector::updateConnection() {
  if (cam == Amethyst || cam == Ruby) {
    _con = true;
    foreach( QEpicsPv * pv, findChildren<QEpicsPv*>() )
      _con &= pv->isConnected();
    if (_con) {
      autoSavePv->set(1);
/*      triggerModePv->set(triggerModes[cam]);
      imageModePv->set(imageModes[cam]); // Multiple
      writeModePv->set(writeModes[cam]); */
    }
  } else {
    _con=false;
  }
  updateReadyness();
  emit connectionChanged(_con);
}


bool Detector::prepare() {

  if ( ! isConnected() )
    return false;

  /*
  if (triggerModePv->get().toInt() != triggerModes[cam] ) {
    triggerModePv->set(triggerModes[cam]);
    qtWait(triggerModePv, SIGNAL(valueUpdated(QVariant)), 500);
  }

  if (imageModePv->get().toInt() != imageModes[cam] ) {
    imageModePv->set(imageModes[cam]);
    qtWait(imageModePv, SIGNAL(valueUpdated(QVariant)), 500);
  }

  if (writeModePv->get().toInt() != writeModes[cam]) {
    writeModePv->set(writeModes[cam]); // Capture
    qtWait(writeModePv, SIGNAL(valueUpdated(QVariant)), 500);
  }
  */

  if (fileNumberPv->get().toInt() !=0 ) {
    fileNumberPv->set(0);
    qtWait(fileNumberPv, SIGNAL(valueUpdated(QVariant)), 500);
  }

  /*
  if (cam != Amethyst  &&  capturePv->get().toInt() != 1) {
    capturePv->set(1);
    qtWait(capturePv, SIGNAL(valueUpdated(QVariant)), 500);
  }
  */

  if ( ! autoSavePv->get().toBool() ) {
    autoSavePv->set(1);
    qtWait(autoSavePv, SIGNAL(valueUpdated(QVariant)), 500);
  }


  return isReady() &&
      fileNumberPv->get().toInt() == 0;

}


void Detector::updateAcquisition() {
  if ( ! aqPv->isConnected() || aqPv->get().toInt() )
    return;
  if ( (cam == Amethyst && !_counter) ||
       cam == Ruby )
    emit done();
}


void Detector::updateExposure() {
  if ( expPv->isConnected() )
    emit exposureChanged(expPv->get().toDouble());;
}


void Detector::updateCounter() {
  if (cam==Amethyst)
    return;
  if ( numCompletePv->isConnected() )
    emit startImage(numCompletePv->get().toInt());
}

void Detector::setExposure(double val) {
  if (isConnected())
    expPv->set(val);
}

void Detector::setInterval(double val) {
  if ( ! isConnected() )
    return;
  if (cam == Amethyst) {
    internalTimer.setInterval(val*1000);
    intPv->set(0);
  } else {
    if (intPv->get().toDouble() != val ) {
      intPv->set(val);
      qtWait(intPv, SIGNAL(valueUpdated(QVariant)), 500);
    }
  }
}

void Detector::setNum(int val) {
  if (isConnected()) {
    if (cam != Amethyst && cam != Ruby)
      capturePv->set(0);
    if (numPv->get().toInt() != val ) {
      numPv->set(val);
      qtWait(numPv, SIGNAL(valueUpdated(QVariant)), 500);
    }
    if (numCapPv->get().toInt() != val ) {
      numCapPv->set(val);
      qtWait(numCapPv, SIGNAL(valueUpdated(QVariant)), 500);
    }
  }
}

void Detector::setNameTemplate(const QString & ntemp) {
  if (isReady()) {
    // epicsqt does not support the array input yet.
    /*
    QVariantList nm;
    for (int curchar=0 ; curchar < ntemp.length(); curchar++)
      nm << (QVariant) (uchar) ntemp.at(curchar).toAscii();
    nm << (QVariant) (uchar) 0;
    fileNamePv->set( nm );
    */
    QString toPut = QString::number(ntemp.length());
    for (int curchar=0 ; curchar < ntemp.length(); curchar++)
      toPut += " " + QString::number( (unsigned) ntemp.at(curchar).toAscii() );
    toPut += " 0";
    qDebug() << ntemp << toPut;
    QProcess::execute("caput -ta " + fileNamePv->pv() + " " + toPut);
  }
}

void Detector::start() {
  QApplication::processEvents();
  if (isReady() &&
      fileNumberPv->get().toInt() == 0  &&
      ( cam == Amethyst || cam == Ruby || capturePv->get().toInt() == 1 ) ) {
    qDebug() << "Starting aq"<< QTime::currentTime().toString("hh:mm:ss.zzz");
    if (cam==Amethyst) {
      if (_counter)
        qDebug() << "Amethyst is aquiring. Will not start acquisition.";
      else {
        internalTimer.start();
        trig();
      }
    } else
      aqPv->set(1);
  }
}

void Detector::stop() {
  if (isConnected()) {
    qDebug() << "Stoppping aq"<< QTime::currentTime().toString("hh:mm:ss.zzz");
    if (cam==Amethyst) {
      internalTimer.stop();
      _counter=0;
    } else
      aqPv->set(0);
  }
}



void Detector::trig() {
  if (cam!=Amethyst)
    return;
  if ( aqPv->get().toInt() || writeStatusPv->get().toInt() ) {
    qDebug() << "Requested acquisition while acquiring" << _counter << aqPv->get().toInt() << writeStatusPv->get().toInt();
    aqPv->set(0);
  } else {
    aqPv->set(1);
    emit startImage(_counter);
  }
  _counter++;
  if ( _counter >= numPv->get().toInt() ) {
    internalTimer.stop();
    _counter=0;
  }
}




