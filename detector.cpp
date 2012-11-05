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
  exposurePv( new QEpicsPv(this) ),
  periodPv( new QEpicsPv(this) ),
  numberPv( new QEpicsPv(this) ),
  counterPv( new QEpicsPv(this) ),
  triggerModePv( new QEpicsPv(this) ),
  imageModePv( new QEpicsPv(this) ),
  aqPv( new QEpicsPv(this) ),
  filePathPv( new QEpicsPv(this) ),
  filePathExistsPv( new QEpicsPv(this) ),
  fileNamePv( new QEpicsPv(this) ),
  fileTemplatePv( new QEpicsPv(this) ),
  fileNumberPv( new QEpicsPv(this) ),
  fileLastNamePv(new QEpicsPv(this) ),
  autoSavePv( new QEpicsPv(this) ),
  writeStatusPv( new QEpicsPv(this) ),
  _con(false),
//  _counter(0),
  _name(QString())
{

  foreach( QEpicsPv * pv, findChildren<QEpicsPv*>() )
    connect(pv, SIGNAL(connectionChanged(bool)), SLOT(updateConnection()));

  connect(counterPv, SIGNAL(valueUpdated(QVariant)), SLOT(updateCounter()));
  connect(filePathPv, SIGNAL(valueChanged(QVariant)), SLOT(updatePath()));
  connect(fileTemplatePv, SIGNAL(valueChanged(QVariant)), SLOT(updateNameTemplate()));
  connect(fileNamePv, SIGNAL(valueChanged(QVariant)), SLOT(updateName()));
  connect(fileLastNamePv, SIGNAL(valueChanged(QVariant)), SLOT(updateLastName()));
  connect(aqPv, SIGNAL(valueChanged(QVariant)), SLOT(updateAcq()));

  connect(writeStatusPv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(aqPv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(exposurePv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(periodPv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(numberPv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(triggerModePv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(imageModePv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(filePathExistsPv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(writeStatusPv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));

  updateConnection();

}

QString Detector::cameraName(Detector::Camera cam) {
  switch(cam) {
    case ScintX : return "ScintX";
    case PCOedge1 : return "PCO.Edge 1";
    case PCOedge2 : return "PCO.Edge 2";
    default: return QString();
  }
}

Detector::Camera Detector::camera(const QString & _cameraName) {
  if (_cameraName =="ScintX") return ScintX;
  if (_cameraName =="PCO.Edge 1") return PCOedge1;
  if (_cameraName =="PCO.Edge 2") return PCOedge2;
  return NONE;
}

const QList<Detector::Camera> Detector::knownCameras =
    ( QList<Detector::Camera> ()
      << Detector::ScintX
      << Detector::PCOedge1
      << Detector::PCOedge2) ;


void Detector::setCamera(Camera _cam) {
  switch (_cam) {
    case ScintX:
      setCamera("ScintX1");
      break;
    case PCOedge1:
      setCamera("SR08ID01DET01");
      break;
    case PCOedge2:
      setCamera("SR08ID01DET02");
      break;
    default:
      foreach( QEpicsPv * pv, findChildren<QEpicsPv*>() )
        pv->setPV();
      break;
  }
}


void Detector::setCamera(const QString & pvName) {
  cameraPv=pvName;
  if ( ! pvName.isEmpty() ) {
    exposurePv->setPV(pvName+":CAM:AcquireTime_RBV");
    periodPv->setPV(pvName+":CAM:AcquirePeriod");
    numberPv->setPV(pvName+":CAM:NumImages");
    counterPv->setPV(pvName+":CAM:NumImagesCounter_RBV");
    triggerModePv->setPV(pvName+":CAM:TriggerMode");
    imageModePv->setPV(pvName+":CAM:ImageMode");
    aqPv->setPV(pvName+":CAM:Acquire");
    fileTemplatePv->setPV(pvName+":TIFF1:FileTemplate");
    fileNamePv->setPV(pvName+":TIFF1:FileName");
    fileLastNamePv->setPV(pvName+":TIFF1:FullFileName_RBV");
    fileNumberPv->setPV(pvName+":TIFF1:FileNumber");
    autoSavePv->setPV(pvName+":TIFF1:AutoSave");
    writeStatusPv->setPV(pvName+":TIFF1:WriteFile_RBV");
    filePathPv->setPV(pvName+":TIFF1:FilePath_RBV");
    filePathExistsPv->setPV(pvName+":TIFF1:FilePathExists_RBV");
  }
}


void Detector::updateConnection() {
  _con = true;
  foreach( QEpicsPv * pv, findChildren<QEpicsPv*>() )
    _con &= pv->isConnected();
  emit connectionChanged(_con);
}


void Detector::updatePath() {
  _path = fromVList(filePathPv->get());
  emit parameterChanged();
}

void Detector::updateName() {
  _name = fromVList(fileNamePv->get());
  emit nameChanged(_name);
}

void Detector::updateLastName() {
  _nameLast = fromVList(fileLastNamePv->get());
  emit lastNameChanged(_nameLast);
}

void Detector::updateNameTemplate() {
  _nameTemplate = fromVList(fileTemplatePv->get());
  emit templateChanged(_nameTemplate);
}


void Detector::updateCounter() {
  if ( counterPv->isConnected() )
    emit counterChanged(counterPv->get().toInt());
}

void Detector::updateAcq() {
  if (!isAcquiring())
    emit done();
}

bool Detector::setInterval(double val) {
  if ( ! periodPv->isConnected() || isAcquiring() )
    return false;
  if (period() != val) {
    periodPv->set(val);
    qtWait(periodPv, SIGNAL(valueUpdated(QVariant)), 500);
  }
  return period() == val;
}

bool Detector::setNumber(int val) {

  if ( ! numberPv->isConnected() || isAcquiring() )
    return false;

  numberPv->set(val);
  qtWait(numberPv, SIGNAL(valueUpdated(QVariant)), 500);

  const int reqIM = val>1 ? 1 : 0;
  if (imageMode() != reqIM) {
    imageModePv->set(reqIM);
    qtWait(imageModePv, SIGNAL(valueUpdated(QVariant)), 500);
  }

  const QString fileT = QString("%s%s") + (val==1 ? "" : "%05d");
  if ( fileTemplate() != fileT) {
    setNameTemplate(fileT);
    qtWait(fileTemplatePv, SIGNAL(valueUpdated(QVariant)), 500);
  }

  return
      number() == val &&
      imageMode() == reqIM &&
      fileTemplate() == fileT;

}

bool Detector::setNameTemplate(const QString & ntemp) {
  if ( ! fileTemplatePv->isConnected() || isAcquiring() )
    return false;
  if ( fileTemplate() != ntemp ) {
    fileTemplatePv->set(ntemp.toAscii());
    qtWait(fileTemplatePv, SIGNAL(valueUpdated(QVariant)), 500);
  }
  return fileTemplate() == ntemp ;
}

bool Detector::setName(const QString & fname) {
  if ( ! fileNamePv->isConnected() || isAcquiring() )
    return false;
  if ( fileName() != fname ) {
    fileNamePv->set(fname.toAscii());
    qtWait(fileNamePv, SIGNAL(valueUpdated(QVariant)), 500);
  }
  return fileName() == fname ;
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

  return true;

}


bool Detector::start() {
  if ( ! aqPv->isConnected() || ! writeStatusPv->isConnected() || isAcquiring() )
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




