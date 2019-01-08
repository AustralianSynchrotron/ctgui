#include "detector.h"
#include <QDebug>
#include <QProcess>
#include <QTime>
#include <QApplication>


static const QString emptyStr="";

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
  enableTiffPv( new QEpicsPv(this) ),
  pathTiffPv( new QEpicsPv(this) ),
  pathSetTiffPv( new QEpicsPv(this) ),
  pathExistsTiffPv( new QEpicsPv(this) ),
  nameTiffPv( new QEpicsPv(this) ),
  nameTemplateTiffPv( new QEpicsPv(this) ),
  fileNumberTiffPv( new QEpicsPv(this) ),
  lastNameTiffPv(new QEpicsPv(this) ),
  autoSaveTiffPv( new QEpicsPv(this) ),
  autoIncrementTiffPv( new QEpicsPv(this) ),
  writeStatusTiffPv( new QEpicsPv(this) ),
  writeProggressTiffPv( new QEpicsPv(this) ),
  queUseTiffPv( new QEpicsPv(this) ),
  enableHdfPv( new QEpicsPv(this) ),
  pathHdfPv( new QEpicsPv(this) ),
  pathSetHdfPv( new QEpicsPv(this) ),
  pathExistsHdfPv( new QEpicsPv(this) ),
  nameHdfPv( new QEpicsPv(this) ),
  nameTemplateHdfPv( new QEpicsPv(this) ),
  lastNameHdfPv( new QEpicsPv(this) ),
  autoSaveHdfPv( new QEpicsPv(this) ),
  autoIncrementHdfPv( new QEpicsPv(this) ),
  writeStatusHdfPv( new QEpicsPv(this) ),
  writeModeHdfPv( new QEpicsPv(this) ),
  writeProggressHdfPv( new QEpicsPv(this) ),
  captureTargetHdfPv( new QEpicsPv(this) ),
  captureProgressHdfPv( new QEpicsPv(this) ),
  captureHdfPv( new QEpicsPv(this) ),
  captureStatusHdfPv( new QEpicsPv(this) ),
  queUseHdfPv( new QEpicsPv(this) ),
  _con(false),
  _hdf(false),
  _camera(NONE),
  _nameTiff(QString())
{

  // frequently changing parameters
  connect(counterPv, SIGNAL(valueUpdated(QVariant)), SLOT(updateCounter()));
  connect(fileNumberTiffPv, SIGNAL(valueUpdated(QVariant)), SLOT(updateFileNumberTiff()));
  connect(lastNameTiffPv, SIGNAL(valueUpdated(QVariant)), SLOT(updatelastNameTiff()));
  connect(queUseTiffPv, SIGNAL(valueUpdated(QVariant)), SLOT(updateQueUseTiff()));
  connect(lastNameHdfPv, SIGNAL(valueUpdated(QVariant)), SLOT(updatelastNameHdf()));
  connect(queUseHdfPv, SIGNAL(valueUpdated(QVariant)), SLOT(updateQueUseHdf()));

  // common CAM params
  connect(exposurePv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(periodPv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(numberPv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(triggerModePv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(imageModePv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));

  // file plugin params
  connect(enableHdfPv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(enableTiffPv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(pathExistsTiffPv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(pathExistsHdfPv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(captureTargetHdfPv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(captureStatusHdfPv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));

  // writing-related pvs
  connect(writeStatusTiffPv, SIGNAL(valueUpdated(QVariant)), SLOT(updateWriting()));
  connect(writeProggressTiffPv, SIGNAL(valueUpdated(QVariant)), SLOT(updateWriting()));
  connect(writeStatusHdfPv, SIGNAL(valueUpdated(QVariant)), SLOT(updateWriting()));
  connect(writeProggressHdfPv, SIGNAL(valueUpdated(QVariant)), SLOT(updateWriting()));
  connect(captureProgressHdfPv, SIGNAL(valueUpdated(QVariant)), SLOT(updateWriting()));

  // parameters which need additional proc
  connect(aqPv, SIGNAL(valueChanged(QVariant)), SLOT(updateParameter()));
  connect(pathTiffPv, SIGNAL(valueChanged(QVariant)), SLOT(updateParameter()));
  connect(nameTemplateTiffPv, SIGNAL(valueChanged(QVariant)), SLOT(updateParameter()));
  connect(nameTiffPv, SIGNAL(valueChanged(QVariant)), SLOT(updateParameter()));
  connect(pathHdfPv, SIGNAL(valueChanged(QVariant)), SLOT(updateParameter()));
  connect(nameTemplateHdfPv, SIGNAL(valueChanged(QVariant)), SLOT(updateParameter()));
  connect(nameHdfPv, SIGNAL(valueChanged(QVariant)), SLOT(updateParameter()));

  foreach( QEpicsPv * pv, findChildren<QEpicsPv*>() )
    connect(pv, SIGNAL(connectionChanged(bool)), SLOT(updateConnection()));
  updateConnection();

}

QString Detector::cameraName(Detector::Camera cam) {
  switch(cam) {
  case ScintX : return "ScintX";
  case HamaGranny : return "HamaGranny";
  case PCOedge2B : return "PCO.Edge 2B";
  case PCOedge3B : return "PCO.Edge 3B";
  case Argus : return "Argus";
  case CPro : return "CPro" ;
  case HamaMama : return "HamaMama";
  default: return QString();
  }
}

Detector::Camera Detector::camera(const QString & _cameraName) {
  if (_cameraName =="ScintX") return ScintX;
  if (_cameraName =="HamaGranny") return HamaGranny;
  if (_cameraName =="Argus") return Argus;
  if (_cameraName =="PCO.Edge 2B") return PCOedge2B;
  if (_cameraName =="PCO.Edge 3B") return PCOedge3B;
  if (_cameraName =="CPro") return CPro;
  if (_cameraName =="HamaMama") return HamaMama;
  return NONE;
}

const QList<Detector::Camera> Detector::knownCameras = ( QList<Detector::Camera> ()
      << Detector::ScintX
      << Detector::HamaGranny
      << Detector::Argus
      << Detector::PCOedge2B
      << Detector::PCOedge3B
      << Detector::CPro
      << Detector::HamaMama
      ) ;


void Detector::setCamera(Camera _cam) {
  const Camera oldcam=_camera;
  _camera = _cam;
  switch (_cam) {
  case ScintX:     setCamera("SR08ID01DET05");    break;
  case HamaGranny: setCamera("SR08ID01DET04");    break;
  case Argus:      setCamera("SR08ID01DET03");    break;
  case PCOedge2B:  setCamera("SR08ID01DET01");    break;
  case PCOedge3B:  setCamera("SR08ID01DET02");    break;
  case CPro:       setCamera("SR08ID01DETIOC06"); break;
  case HamaMama:   setCamera("SR08ID01DETIOC08"); break;
  default:
    _camera = oldcam;
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

    enableTiffPv->setPV(pvName + ":TIFF:EnableCallbacks");
    nameTemplateTiffPv->setPV(pvName + ":TIFF:FileTemplate");
    nameTiffPv->setPV(pvName + ":TIFF:FileName");
    lastNameTiffPv->setPV(pvName + ":TIFF:FullFileName_RBV");
    fileNumberTiffPv->setPV(pvName + ":TIFF:FileNumber");
    autoSaveTiffPv->setPV(pvName + ":TIFF:AutoSave");
    autoIncrementTiffPv->setPV(pvName + ":TIFF:AutoIncrement");
    writeProggressTiffPv->setPV(pvName+":TIFF:WriteFile_RBV");
    writeStatusTiffPv->setPV(pvName+":TIFF:WriteStatus");
    pathTiffPv->setPV(pvName + ":TIFF:FilePath_RBV");
    pathSetTiffPv->setPV(pvName + ":TIFF:FilePath");
    pathExistsTiffPv->setPV(pvName + ":TIFF:FilePathExists_RBV");
    queUseTiffPv->setPV(pvName + ":TIFF:QueueUse");

    enableHdfPv->setPV(pvName + ":HDF:EnableCallbacks");
    pathHdfPv->setPV(pvName + ":HDF:FilePath_RBV");
    pathSetHdfPv->setPV(pvName + ":HDF:FilePath");
    pathExistsHdfPv->setPV(pvName + ":HDF:FilePathExists_RBV");
    nameHdfPv->setPV(pvName + ":HDF:FileName");
    nameTemplateHdfPv->setPV(pvName + ":HDF:FileTemplate");
    lastNameHdfPv->setPV(pvName + ":HDF:FullFileName_RBV");
    autoSaveHdfPv->setPV(pvName + ":HDF:AutoSave");
    autoIncrementHdfPv->setPV(pvName + ":HDF:AutoIncrement");
    writeStatusHdfPv->setPV(pvName + ":HDF:WriteStatus");
    writeModeHdfPv->setPV(pvName + ":HDF:FileWriteMode");
    writeProggressHdfPv->setPV(pvName + ":HDF:WriteFile_RBV");
    captureTargetHdfPv->setPV(pvName + ":HDF:NumCapture");
    captureProgressHdfPv->setPV(pvName + ":HDF:NumCaptured_RBV");
    captureHdfPv->setPV(pvName + ":HDF:Capture");
    captureStatusHdfPv->setPV(pvName + ":HDF:Capture_RBV");
    queUseHdfPv->setPV(pvName + ":HDF:QueueUse");

  }

}


void Detector::updateConnection() {
  if ( ! _camera )
    return;
  bool new_con = true;
  bool new_hdf = true;
  foreach( QEpicsPv * pv, findChildren<QEpicsPv*>() ) {
    if ( pv->pv().contains(":HDF:") )
      new_hdf &= pv->isConnected();
    else // some of our detectors do not have HDF support yet.
      new_con &= pv->isConnected();
  }
  if (new_con != _con || new_hdf != _hdf) {
    _hdf = new_con && new_hdf;
    emit connectionChanged(_con=new_con);
  }
}


void Detector::updateParameter() {

  if ( ! _camera )
    return;

  if ( sender() == aqPv  &&  ! isAcquiring() ) {
    emit done();
    QCoreApplication::processEvents();
  }

  _nameTiff = fromVList(nameTiffPv->get());
  _nameTemplateTiff = fromVList(nameTemplateTiffPv->get());
  _pathTiff = fromVList( pathTiffPv->get() );
  _nameHdf = fromVList(nameHdfPv->get());
  _nameTemplateHdf = fromVList(nameTemplateHdfPv->get());
  _pathHdf = fromVList( pathHdfPv->get() );

  emit parameterChanged();

}


void Detector::updateWriting() {

  if ( ! _camera )
    return;

  if (isWriting())
    emit writingStarted();
  else if ( ! queUseTiffPv->isConnected() || ! queUseTiffPv->get().toInt() ||
            ! queUseHdfPv->isConnected() || ! queUseHdfPv->get().toInt() )
    emit writingFinished();

  if ( writeStatusTiffPv->get().toInt())
    emit writingError(lastName(TIFF));
  if ( writeStatusHdfPv->get().toInt())
    emit writingError(lastName(HDF));

}



void Detector::updatelastNameTiff() {
  if ( ! _camera )
    return;
  _lastName = fromVList(lastNameTiffPv->get());
  if (_lastName != _lastNameTiff)
    emit lastNameChanged(_lastNameTiff=_lastName);
}

void Detector::updatelastNameHdf() {
  if ( ! _camera )
    return;
  _lastName = fromVList(lastNameHdfPv->get());
  if (_lastName != _lastNameHdf)
    emit lastNameChanged(_lastNameHdf=_lastName);
}

void Detector::updateFileNumberTiff() {
  if ( ! _camera )
    return;
  emit fileNumberChanged( fileNumberTiffPv->get().toInt() );
}

void Detector::updateQueUseTiff() {
  if ( ! _camera )
    return;
  emit queTiffChanged( queUseTiffPv->get().toInt() );
}

void Detector::updateQueUseHdf() {
  if ( ! _camera )
    return;
  emit queHdfChanged( queUseHdfPv->get().toInt() );
}




void Detector::updateCounter() {
  if ( ! _camera )
    return;
  if ( ! counterPv->isConnected() )
    return;
  emit counterChanged( counterPv->get().toInt() );
}




Detector::ImageFormat Detector::imageFormat() const {
  if ( ! _camera )
    return UNDEFINED;
  if (enableTiffPv->get().toBool())
    return TIFF;
  else if (enableHdfPv->get().toBool())
    return HDF;
  else
    return UNDEFINED;
}


bool Detector::imageFormat(Detector::ImageFormat fmt) const {
  if ( ! _camera )
    return false;
  if (fmt==TIFF)
    return enableTiffPv->get().toBool();
  else if (fmt==HDF)
    return enableHdfPv->get().toBool();
  else
    return false;
}


const QString & Detector::path(Detector::ImageFormat fmt) const {
  if (fmt==TIFF)
    return _pathTiff;
  else if (fmt==HDF)
    return _pathHdf;
  else
    return emptyStr;
}


const QString & Detector::nameTemplate(Detector::ImageFormat fmt) const {
  if (fmt==TIFF)
    return _nameTemplateTiff;
  else if (fmt==HDF)
    return _nameTemplateHdf;
  else
    return emptyStr;
}

const QString & Detector::name(Detector::ImageFormat fmt) const {
  if (fmt==TIFF)
    return _nameTiff;
  else if (fmt==HDF)
    return _nameHdf;
  else
    return emptyStr;
}

const QString & Detector::lastName(Detector::ImageFormat fmt) const {
  if (fmt==TIFF)
    return _lastNameTiff;
  else if (fmt==HDF)
    return _lastNameHdf;
  else
    return _lastName;
}

bool Detector::pathExists(Detector::ImageFormat fmt) const {
  if ( ! _camera )
    return true;
  if (fmt==TIFF)
    return pathExistsTiffPv->get().toBool();
  else if (fmt==HDF)
    return pathExistsHdfPv->get().toBool();
  else
    return pathExists(TIFF) && pathExists(HDF);
}

bool Detector::isWriting(Detector::ImageFormat fmt) const {
  if ( ! _camera )
    return false;
  if (fmt==TIFF)
    return writeProggressTiffPv->get().toInt() || ( queUseTiffPv->isConnected() ? queUseTiffPv->get().toInt() : false );
  else if (fmt==HDF)
    return writeProggressHdfPv->get().toInt() || ( queUseHdfPv->isConnected() ? queUseHdfPv->get().toInt() : false );
  else
    return isWriting(TIFF) || isWriting(HDF);
}






bool Detector::setImageFormat(Detector::ImageFormat fmt) {
  if ( ! _camera )
    return true;
  if (fmt == UNDEFINED)
    return false;
  enableTiffPv->set(fmt == TIFF);
  enableHdfPv->set(fmt == HDF);
  qtWait(enableTiffPv, SIGNAL(valueUpdated(QVariant)), 500);
  qtWait(enableHdfPv, SIGNAL(valueUpdated(QVariant)), 500);
  //if ( fmt==HDF )
  //  writeModeHdfPv->se ();

  return imageFormat() == fmt;
}


bool Detector::setExposure(double val) {
  if ( ! _camera )
    return true;
  if ( ! setExposurePv->isConnected() || isAcquiring() )
    return false;
  setExposurePv->set(val);
  if (exposure() != val)
    qtWait(exposurePv, SIGNAL(valueUpdated(QVariant)), 500);
  return exposure() == val;
}



bool Detector::setPeriod(double val) {
  if ( ! _camera )
    return true;
  if ( ! periodPv->isConnected() || isAcquiring() )
    return false;
  periodPv->set(val);
  if (period() != val)
    qtWait(periodPv, SIGNAL(valueUpdated(QVariant)), 500);
  return period() == val;
}

bool Detector::setNumber(int val) {

  setenv("DETAQNUM", QString::number(val).toLatin1(), 1);
  if ( ! _camera )
    return true;

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

bool Detector::setNameTemplate(ImageFormat fmt, const QString & ntemp) {

  if ( ! _camera )
    return true;
  if ( isAcquiring() )
    return false;
  if (nameTemplate(fmt) == ntemp)
    return true;

  if ( fmt == TIFF  && nameTemplateTiffPv->isConnected() ) {
    nameTemplateTiffPv->set(ntemp.toLatin1().append(char(0)));
    qtWait(nameTemplateTiffPv, SIGNAL(valueUpdated(QVariant)), 500);
  } else if ( fmt == HDF  && nameTemplateHdfPv->isConnected() ) {
    nameTemplateHdfPv->set(ntemp.toLatin1().append(char(0)));
    qtWait(nameTemplateHdfPv, SIGNAL(valueUpdated(QVariant)), 500);
  } else {
    return setNameTemplate(TIFF, ntemp) && setNameTemplate(HDF, ntemp);
  }
  return nameTemplate(fmt) == ntemp ;

}

bool Detector::setName(ImageFormat fmt, const QString & fname) {

  setenv("DETFILENAME", fname.toLatin1(), 1);
  if ( ! _camera )
    return true;
  if ( isAcquiring() )
    return false;
  if (name(fmt) == fname)
    return true;

  if ( fmt == TIFF  && nameTiffPv->isConnected() ) {
    nameTiffPv->set(fname.toLatin1().append(char(0)));
    qtWait(nameTiffPv, SIGNAL(valueUpdated(QVariant)), 500);
  } else if ( fmt == HDF  && nameHdfPv->isConnected() ) {
    nameHdfPv->set(fname.toLatin1().append(char(0)));
    qtWait(nameHdfPv, SIGNAL(valueUpdated(QVariant)), 500);
  } else {
    return setName(TIFF, fname) & setName(HDF, fname);
  }
  return name(fmt) == fname ;

}

bool Detector::setPath(const QString & _path) {

  if ( ! _camera )
    return true;

  const QByteArray _ptoset = _path.toLatin1().append(char(0));

  if ( pathSetTiffPv->isConnected()  &&  path(TIFF) != _path )
    pathSetTiffPv->set(_ptoset);
  if ( pathSetHdfPv->isConnected()  &&  path(HDF) != _path )
    pathSetHdfPv->set(_ptoset);
  // here set and check are split to ensure no signals are processed while qtWaiting.
  if ( pathSetTiffPv->isConnected()  &&  path(TIFF) != _path )
    qtWait(pathTiffPv, SIGNAL(valueUpdated(QVariant)), 500);
  if ( pathSetHdfPv->isConnected()  &&  path(HDF) != _path )
    qtWait(pathHdfPv, SIGNAL(valueUpdated(QVariant)), 500);

  return path(TIFF) == _path  &&  path(HDF) == _path;

}


bool Detector::setAutoSave(bool autoSave) {
  if ( ! _camera )
    return true;
  if ( autoSaveTiffPv->get().toBool() != autoSave ) {
    autoSaveTiffPv->set(autoSave ? 1 : 0);
    qtWait(autoSaveTiffPv, SIGNAL(valueUpdated(QVariant)), 500);
  }
  autoIncrementTiffPv->set(1);
  if ( autoSaveHdfPv->get().toBool() != autoSave ) {
    autoSaveHdfPv->set(autoSave ? 1 : 0);
    qtWait(autoSaveHdfPv, SIGNAL(valueUpdated(QVariant)), 500);
  }
  autoIncrementHdfPv->set(1);
  return
      autoSaveTiffPv->get().toBool() == autoSave  &&
      autoIncrementTiffPv->get().toBool() &&
      autoSaveHdfPv->get().toBool() == autoSave &&
      autoIncrementHdfPv->get().toBool() ;
}



bool Detector::prepareForAcq(Detector::ImageFormat fmt, int nofFrames) {

  if ( ! _camera )
    return true;
  if ( isAcquiring() || fmt == UNDEFINED || nofFrames <= 0 )
    return false;

  setImageFormat(fmt);
  if ( ! setAutoSave(true) )
    return false;

  if ( fmt == TIFF ) {
    if (fileNumberTiffPv->get().toInt() !=0 ) {
      fileNumberTiffPv->set(0);
      qtWait(fileNumberTiffPv, SIGNAL(valueUpdated(QVariant)), 500);
      if ( fileNumberTiffPv->get().toInt() !=0 )
        return false;
    }

  } else if (fmt == HDF) {

    if ( captureStatusHdfPv->get().toInt() == 1 ) // capturing
      return false;

    captureTargetHdfPv->set(nofFrames);
    if ( captureTargetHdfPv->get() !=nofFrames ) {
      qtWait(captureTargetHdfPv, SIGNAL(valueUpdated(QVariant)), 500);
      if ( captureTargetHdfPv->get() !=nofFrames )
        return false;
    }
    writeModeHdfPv->set(2); // 2 is "stream"
    if ( writeModeHdfPv->get() != 2 ) {
      qtWait(writeModeHdfPv, SIGNAL(valueUpdated(QVariant)), 500);
      if ( writeModeHdfPv->get() != 2 )
        return false;
    }

    captureHdfPv->set(1);
    if ( captureStatusHdfPv->get() != 1 ) {
      qtWait(captureStatusHdfPv, SIGNAL(valueUpdated(QVariant)), 500);
      if ( captureStatusHdfPv->get() != 1 )
        return false;
    }

  }

  if ( _camera == HamaGranny ) {
     if ( ! setTriggerMode(1) )
       return false;
  } else if ( _camera != Argus ) {
     if ( ! setTriggerMode(0) )
       return false;
  }

  return true;

}


bool Detector::setImageMode(int imode) {

  if ( ! _camera )
    return true;
  if ( ! imageModePv->isConnected() )
    return false;
  if ( imageMode() == imode )
    return true;

  imageModePv->set(imode);
  qtWait(imageModePv, SIGNAL(valueUpdated(QVariant)), 500);
  return imageModePv->get().toInt() == imode;

}


bool Detector::setTriggerMode(int tmode) {

  if ( ! _camera )
    return true;
  if ( ! triggerModePv->isConnected() )
    return false;
  if ( triggerMode() == tmode )
    return true;

  triggerModePv->set(tmode);
  qtWait(triggerModePv, SIGNAL(valueUpdated(QVariant)), 500);
  return triggerModePv->get().toInt() == tmode;

}


bool Detector::setHardwareTriggering(bool set) {

  if ( ! _camera )
    return true;

  int mode = 0; // soft triggered
  if (set) {
    switch ( _camera ) {
    case (PCOedge2B) :
    case (PCOedge3B) :
      // mode = 2; // Ext + Soft
      mode = 4; // Ext Only
      break;
    default :
      mode = 1;
      break;
    }
  }

  return setTriggerMode(mode);

}


bool Detector::start() {

  if ( ! _camera )
    return true;
  if ( ! aqPv->isConnected() || ! writeProggressTiffPv->isConnected() || isAcquiring() )
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
  if ( ! _camera )
    return true;
  if ( ! start() )
    return false;
  waitDone();
  return true;
}

void Detector::waitDone() {
  if ( ! _camera )
    return;
  if (isAcquiring())
    qtWait(this, SIGNAL(done()));
}


void Detector::waitWritten() {

  if ( ! _camera )
    return;

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
  while ( isWriting() || queUseTiffPv->get().toInt() )
    qtWait(this, SIGNAL(writingFinished()), 500);

  //  timer.start();

}





