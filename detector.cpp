#include "detector.h"
#include <QDebug>
#include <QProcess>
#include <QTime>
#include <QApplication>



static QString fromVList(const QVariant & vlist) {
  if ( ! vlist.isValid() || vlist.type() != QVariant::List )
    return QString();
  QString ret;
  foreach (QVariant var, vlist.toList())
    if ( ! var.canConvert(QVariant::Char) )
      return QString();
    else if (var.toChar() == 0)
      return ret;
    else
      ret += var.toChar();
  return ret;
}


Detector::Detector(QObject * parent) :
  QObject(parent),
  setExposurePv( new QEpicsPv(this) ),
  exposurePv( new QEpicsPv(this) ),
  periodPv( new QEpicsPv(this) ),
  numberPv( new QEpicsPv(this) ),
  counterPv( new QEpicsPv(this) ),
  triggerModePv( new QEpicsPv(this) ),
  imageModePv( new QEpicsPv(this) ),
  aqPv( new QEpicsPv(this) ),
  pathPv( new QEpicsPv(this) ),
  pathPvSet( new QEpicsPv(this) ),
  pathExistsPv( new QEpicsPv(this) ),
  namePv( new QEpicsPv(this) ),
  nameTemplatePv( new QEpicsPv(this) ),
  fileNumberPv( new QEpicsPv(this) ),
  lastNamePv(new QEpicsPv(this) ),
  autoSavePv( new QEpicsPv(this) ),
  writeStatusPv( new QEpicsPv(this) ),
  writeProggressPv( new QEpicsPv(this) ),
  queUsePv( new QEpicsPv(this) ),
  _con(false),
  _camera(NONE),
  _name(QString())
{

  foreach( QEpicsPv * pv, findChildren<QEpicsPv*>() )
    connect(pv, SIGNAL(connectionChanged(bool)), SLOT(updateConnection()));

  connect(counterPv, SIGNAL(valueUpdated(QVariant)), SLOT(updateCounter()));
  connect(pathPv, SIGNAL(valueChanged(QVariant)), SLOT(updatePath()));
  connect(nameTemplatePv, SIGNAL(valueChanged(QVariant)), SLOT(updateNameTemplate()));
  connect(namePv, SIGNAL(valueChanged(QVariant)), SLOT(updateName()));
  connect(lastNamePv, SIGNAL(valueChanged(QVariant)), SLOT(updateLastName()));
  connect(aqPv, SIGNAL(valueChanged(QVariant)), SLOT(updateAcq()));
  connect(writeProggressPv, SIGNAL(valueChanged(QVariant)), SLOT(updateWriting()));
  connect(writeStatusPv, SIGNAL(valueUpdated(QVariant)), SLOT(onWritingStatus()));
  connect(exposurePv, SIGNAL(valueChanged(QVariant)), SLOT(updateExposure()));
  connect(periodPv, SIGNAL(valueChanged(QVariant)), SLOT(updatePeriod()));
  connect(numberPv, SIGNAL(valueChanged(QVariant)), SLOT(updateTotalImages()));
  connect(queUsePv, SIGNAL(valueChanged(QVariant)), SLOT(updateWriting()));

  connect(aqPv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(triggerModePv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(imageModePv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(pathExistsPv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(writeStatusPv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(writeProggressPv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(queUsePv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));

  updateConnection();

}

QString Detector::cameraName(Detector::Camera cam) {
  switch(cam) {
  case ScintX : return "ScintX";
  case Hamamatsu : return "Hamamatsu";
  case PCOedge1 : return "PCO.Edge 1";
  case PCOedge2 : return "PCO.Edge 2";
  case Argus : return "Argus";
  default: return QString();
  }
}

Detector::Camera Detector::camera(const QString & _cameraName) {
  if (_cameraName =="ScintX") return ScintX;
  if (_cameraName =="Hamamatsu") return Hamamatsu;
  if (_cameraName =="Argus") return Argus;
  if (_cameraName =="PCO.Edge 1") return PCOedge1;
  if (_cameraName =="PCO.Edge 2") return PCOedge2;
  return NONE;
}

const QList<Detector::Camera> Detector::knownCameras =
    ( QList<Detector::Camera> ()
      << Detector::ScintX
      << Detector::Hamamatsu
      << Detector::Argus
      << Detector::PCOedge1
      << Detector::PCOedge2) ;


void Detector::setCamera(Camera _cam) {
  switch (_cam) {
  case ScintX:
    _camera = ScintX;
    setCamera("SR08ID01DET05");
    break;
  case Hamamatsu:
    _camera = Hamamatsu;
    setCamera("SR08ID01DET04");
    break;
  case Argus:
    _camera = Argus;
    setCamera("SR08ID01DET03");
    break;
  case PCOedge1:
    _camera = PCOedge1;
    setCamera("SR08ID01DET01");
    break;
  case PCOedge2:
    _camera = PCOedge2;
    setCamera("SR08ID01DET02");
    break;
  default:
    foreach( QEpicsPv * pv, findChildren<QEpicsPv*>() )
      pv->setPV();
    break;
  }
}


void Detector::setCamera(const QString & pvName) {

  cameraPv = pvName;

  if ( ! pvName.isEmpty() ) {

    setExposurePv->setPV(pvName+":CAM:AcquireTime");
    exposurePv->setPV(pvName+":CAM:AcquireTime_RBV");
    periodPv->setPV(pvName+":CAM:AcquirePeriod");
    numberPv->setPV(pvName+":CAM:NumImages");
    counterPv->setPV(pvName+":CAM:NumImagesCounter_RBV");
    triggerModePv->setPV(pvName+":CAM:TriggerMode");
    imageModePv->setPV(pvName+":CAM:ImageMode");
    aqPv->setPV(pvName+":CAM:Acquire");

    nameTemplatePv->setPV(pvName + ":TIFF:FileTemplate");
    namePv->setPV(pvName + ":TIFF:FileName");
    lastNamePv->setPV(pvName + ":TIFF:FullFileName_RBV");
    fileNumberPv->setPV(pvName + ":TIFF:FileNumber");
    autoSavePv->setPV(pvName + ":TIFF:AutoSave");
    writeProggressPv->setPV(pvName+":TIFF:WriteFile_RBV");
    writeStatusPv->setPV(pvName+":TIFF:WriteStatus");
    pathPv->setPV(pvName + ":TIFF:FilePath_RBV");
    pathPvSet->setPV(pvName + ":TIFF:FilePath");
    pathExistsPv->setPV(pvName + ":TIFF:FilePathExists_RBV");
    queUsePv->setPV(pvName + ":TIFF:QueueUse");

  }

}


void Detector::updateConnection() {
  _con = true;
  foreach( QEpicsPv * pv, findChildren<QEpicsPv*>() ) {
    _con &= pv->isConnected();
  }
  emit connectionChanged(_con);
}


void Detector::updatePath() {
  _path = fromVList(pathPv->get());
  emit pathChanged(_path);
}

void Detector::updateExposure() {
  if ( exposurePv->isConnected() )
    emit exposureChanged(exposurePv->get().toDouble());
}

void Detector::updatePeriod() {
  if ( periodPv->isConnected() )
    emit periodChanged(periodPv->get().toDouble());
}

void Detector::updateName() {
  _name = fromVList(namePv->get());
  emit nameChanged(_name);
}

void Detector::updateLastName() {
  const QString new_lastName = fromVList(lastNamePv->get());
  if (new_lastName != _lastName)
    emit lastNameChanged(_lastName=new_lastName);
}

void Detector::updateNameTemplate() {
  _nameTemplate = fromVList(nameTemplatePv->get());
  emit templateChanged(_nameTemplate);
}


void Detector::updateCounter() {
  if ( ! counterPv->isConnected() )
    return;
  int cnt = counterPv->get().toInt();
  emit counterChanged(cnt);
}

void Detector::updateTotalImages() {
  if ( ! numberPv->isConnected() )
    return;
  int tot = numberPv->get().toInt();
  emit totalImagesChanged(tot);
}


void Detector::updateAcq() {
  if (!isAcquiring()) {
    emit done();
    QCoreApplication::processEvents();
  }
}

void Detector::updateWriting() {
  if (isWriting())
    emit writingStarted();
  else if ( ! queUsePv->isConnected() || ! queUsePv->get().toInt() )
    emit writingFinished();
}


void Detector::onWritingStatus() {
  if ( writeStatusPv->get().toInt() )
    emit writingError( lastName() );
}






bool Detector::setExposure(double val) {
  if ( ! setExposurePv->isConnected() || isAcquiring() )
    return false;
  setExposurePv->set(val);
  if (exposure() != val)
    qtWait(exposurePv, SIGNAL(valueUpdated(QVariant)), 500);
  return exposure() == val;
}



double Detector::period() const {
  return periodPv->get().toDouble();
}


bool Detector::setPeriod(double val) {
  if ( ! periodPv->isConnected() || isAcquiring() )
    return false;
  periodPv->set(val);
  if (period() != val)
    qtWait(periodPv, SIGNAL(valueUpdated(QVariant)), 500);
  return period() == val;
}

bool Detector::setNumber(int val) {

  if ( ! numberPv->isConnected() || isAcquiring() || val < 1)
    return false;

  numberPv->set(val);
  qtWait(numberPv, SIGNAL(valueUpdated(QVariant)), 500);

  const int reqIM = val>1 ? 1 : 0;
  setImageMode(reqIM);

  if (val == 1)
    setPeriod(0);

  return
      number() == val &&
      imageMode() == reqIM;

}

bool Detector::setNameTemplate(const QString & ntemp) {

  if ( ! nameTemplatePv->isConnected() || isAcquiring() )
    return false;
  if ( nameTemplate() != ntemp ) {
    nameTemplatePv->set(ntemp.toAscii().append(char(0)));
    qtWait(nameTemplatePv, SIGNAL(valueUpdated(QVariant)), 500);
  }
  return nameTemplate() == ntemp ;
}

bool Detector::setName(const QString & fname) {

  if ( ! namePv->isConnected() || isAcquiring() )
    return false;
  if ( name() != fname ) {
      namePv->set(fname.toAscii().append(char(0)));
    qtWait(namePv, SIGNAL(valueUpdated(QVariant)), 500);
  }
  return name() == fname ;
}

bool Detector::setPath(const QString & _path) {

  if ( ! pathPvSet->isConnected() )
    return false;
  if ( path() != _path ) {
      pathPvSet->set(_path.toAscii().append(char(0)));
    qtWait(pathPv, SIGNAL(valueUpdated(QVariant)), 500);
  }
  return path() == _path ;

}



bool Detector::setAutoSave(bool autoSave) {
  if ( ! autoSavePv->isConnected() )
    return false;
  if ( autoSavePv->get().toBool() != autoSave ) {
    autoSavePv->set(autoSave ? 1 : 0);
    qtWait(autoSavePv, SIGNAL(valueUpdated(QVariant)), 500);
    if ( autoSavePv->get().toBool() != autoSave )
      return false;
  }
  return true;
}



bool Detector::prepareForAcq() {
  if ( ! fileNumberPv->isConnected() || ! autoSavePv->isConnected() || isAcquiring() )
    return false;

  if (fileNumberPv->get().toInt() !=0 ) {
    fileNumberPv->set(0);
    qtWait(fileNumberPv, SIGNAL(valueUpdated(QVariant)), 500);
    if ( fileNumberPv->get().toInt() !=0 )
      return false;
  }

  if ( ! autoSavePv->get().toBool() ) {
    autoSavePv->set(1);
    qtWait(autoSavePv, SIGNAL(valueUpdated(QVariant)), 500);
    if ( ! autoSavePv->get().toBool() )
      return false;
  }

  if ( _camera == Hamamatsu ) {
     if ( ! setTriggerMode(1) )
       return false;
  } else if ( _camera != Argus ) {
     if ( ! setTriggerMode(0) )
       return false;
  }
  return true;

}


bool Detector::setImageMode(int imode) {

  if ( ! imageModePv->isConnected() )
    return false;
  if ( imageMode() == imode )
    return true;

  imageModePv->set(imode);
  qtWait(imageModePv, SIGNAL(valueUpdated(QVariant)), 500);
  return imageModePv->get().toInt() == imode;

}


bool Detector::setTriggerMode(int tmode) {

  if ( ! triggerModePv->isConnected() )
    return false;
  if ( triggerMode() == tmode )
    return true;

  triggerModePv->set(tmode);
  qtWait(triggerModePv, SIGNAL(valueUpdated(QVariant)), 500);
  return triggerModePv->get().toInt() == tmode;

}


bool Detector::setHardwareTriggering(bool set) {

  int mode = 0; // soft triggered
  if (set) {
    switch ( _camera ) {
    case (PCOedge1) :
    case (PCOedge2) :
      mode = 2; // Ext + Soft
      break;
    default :
      mode = 1;
      break;
    }
  }

  return setTriggerMode(mode);

}


bool Detector::start() {

  if ( ! aqPv->isConnected() || ! writeProggressPv->isConnected() || isAcquiring() )
    return false;
  aqPv->set(1);
  qtWait(aqPv, SIGNAL(valueUpdated(QVariant)), 2000);
  return aqPv->get().toInt();
}

void Detector::stop() {
  if ( aqPv->isConnected() )
    aqPv->set(0);
}

bool Detector::acquire() {
  if ( ! start() )
    return false;
  waitDone();
  return true;
}

void Detector::waitDone() {
  if (isAcquiring())
    qtWait(this, SIGNAL(done()));
}


void Detector::waitWritten() {

  /*
  QTimer timer;
  timer.setSingleShot(true);
  timer.setInterval(500); // maximum time to start writing next frame

  connect(this, SIGNAL(writingStarted()), &timer, SLOT(stop()));
  connect(this, SIGNAL(writingFinished()), &timer, SLOT(start()));
  connect(lastNamePv, SIGNAL(valueUpdated(QVariant)), &timer, SLOT(start()));

  QEventLoop q;
  connect(&timer, SIGNAL(tiksysgmeout()), &q, SLOT(quit()));
  */

  //if ( ! isConnected() )
  //  return;
  while ( isWriting() || queUsePv->get().toInt() )
    qtWait(this, SIGNAL(writingFinished()), 500);

  //  timer.start();

}





