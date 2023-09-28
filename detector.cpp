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
  enableTifPv( new QEpicsPv(this) ),
  pathTifPv( new QEpicsPv(this) ),
  pathSetTifPv( new QEpicsPv(this) ),
  pathExistsTifPv( new QEpicsPv(this) ),
  nameTifPv( new QEpicsPv(this) ),
  nameTemplateTifPv( new QEpicsPv(this) ),
  fileNumberTifPv( new QEpicsPv(this) ),
  lastNameTifPv(new QEpicsPv(this) ),
  autoSaveTifPv( new QEpicsPv(this) ),
  autoIncrementTifPv( new QEpicsPv(this) ),
  writeStatusTifPv( new QEpicsPv(this) ),
  writeProggressTifPv( new QEpicsPv(this) ),
  writeModeTifPv( new QEpicsPv(this) ),
  captureTargetTifPv( new QEpicsPv(this) ),
  captureProgressTifPv( new QEpicsPv(this) ),
  captureTifPv( new QEpicsPv(this) ),
  captureStatusTifPv( new QEpicsPv(this) ),
  queUseTifPv( new QEpicsPv(this) ),
  enableHdfPv( new QEpicsPv(this) ),
  pathHdfPv( new QEpicsPv(this) ),
  pathSetHdfPv( new QEpicsPv(this) ),
  pathExistsHdfPv( new QEpicsPv(this) ),
  nameHdfPv( new QEpicsPv(this) ),
  nameTemplateHdfPv( new QEpicsPv(this) ),
  fileNumberHdfPv( new QEpicsPv(this) ),
  lastNameHdfPv( new QEpicsPv(this) ),
  autoSaveHdfPv( new QEpicsPv(this) ),
  autoIncrementHdfPv( new QEpicsPv(this) ),
  writeStatusHdfPv( new QEpicsPv(this) ),
  writeProggressHdfPv( new QEpicsPv(this) ),
  writeModeHdfPv( new QEpicsPv(this) ),
  captureTargetHdfPv( new QEpicsPv(this) ),
  captureProgressHdfPv( new QEpicsPv(this) ),
  captureHdfPv( new QEpicsPv(this) ),
  captureStatusHdfPv( new QEpicsPv(this) ),
  queUseHdfPv( new QEpicsPv(this) ),
  _con(false),
  _hdf(false),
  _camera(NONE),
  _nameTif(QString())
{

  // frequently changing parameters
  connect(counterPv, SIGNAL(valueUpdated(QVariant)), SLOT(updateCounter()));
  connect(fileNumberTifPv, SIGNAL(valueUpdated(QVariant)), SLOT(updateFileNumberTif()));
  connect(lastNameTifPv, SIGNAL(valueUpdated(QVariant)), SLOT(updatelastNameTif()));
  connect(queUseTifPv, SIGNAL(valueUpdated(QVariant)), SLOT(updateQueUseTif()));
  connect(fileNumberHdfPv, SIGNAL(valueUpdated(QVariant)), SLOT(updateFileNumberTif()));
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
  connect(enableTifPv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(pathExistsTifPv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(pathExistsHdfPv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(captureTargetTifPv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(captureTargetHdfPv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(captureStatusTifPv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));
  connect(captureStatusHdfPv, SIGNAL(valueChanged(QVariant)), SIGNAL(parameterChanged()));

  // writing-related pvs
  connect(writeStatusTifPv, SIGNAL(valueUpdated(QVariant)), SLOT(updateWriting()));
  connect(writeProggressTifPv, SIGNAL(valueUpdated(QVariant)), SLOT(updateWriting()));
  connect(captureProgressTifPv, SIGNAL(valueUpdated(QVariant)), SLOT(updateWriting()));
  connect(captureStatusTifPv, SIGNAL(valueChanged(QVariant)), SLOT(updateWriting()));
  connect(writeStatusHdfPv, SIGNAL(valueUpdated(QVariant)), SLOT(updateWriting()));
  connect(writeProggressHdfPv, SIGNAL(valueUpdated(QVariant)), SLOT(updateWriting()));
  connect(captureProgressHdfPv, SIGNAL(valueUpdated(QVariant)), SLOT(updateWriting()));
  connect(captureStatusHdfPv, SIGNAL(valueChanged(QVariant)), SLOT(updateWriting()));

  // parameters which need additional proc
  connect(aqPv, SIGNAL(valueChanged(QVariant)), SLOT(updateParameter()));
  connect(pathTifPv, SIGNAL(valueChanged(QVariant)), SLOT(updateParameter()));
  connect(nameTemplateTifPv, SIGNAL(valueChanged(QVariant)), SLOT(updateParameter()));
  connect(nameTifPv, SIGNAL(valueChanged(QVariant)), SLOT(updateParameter()));
  connect(pathHdfPv, SIGNAL(valueChanged(QVariant)), SLOT(updateParameter()));
  connect(nameTemplateHdfPv, SIGNAL(valueChanged(QVariant)), SLOT(updateParameter()));
  connect(nameHdfPv, SIGNAL(valueChanged(QVariant)), SLOT(updateParameter()));

  foreach( QEpicsPv * pv, findChildren<QEpicsPv*>() )
    connect(pv, SIGNAL(connectionChanged(bool)), SLOT(updateConnection()));
  updateConnection();

}

QString Detector::cameraName(Detector::Camera cam) {
  switch(cam) {
  case NONE : return "NONE";
  case ScintX : return "ScintX";
  case HamaPapa : return "HamaPapa";
  case PCOedge2B : return "PCO.Edge 2B";
  case PCOedge3B : return "PCO.Edge 3B";
  case PCOedgeFibre : return "PCO.Edge Fibre";
  case Argus : return "Argus";
  case CPro : return "CPro" ;
  case HamaMama : return "HamaMama";
  case Xenia : return "Xenia";
  case XeniaPPS : return "Xenia+PPS";
  case Eiger : return "Eiger";
  case EigerPPS : return "Eiger+PPS";
  default: return QString();
  }
}

Detector::Camera Detector::camera(const QString & _cameraName) {
  if (_cameraName =="ScintX") return ScintX;
  if (_cameraName =="HamaPapa") return HamaPapa;
  if (_cameraName =="Argus") return Argus;
  if (_cameraName =="PCO.Edge 2B") return PCOedge2B;
  if (_cameraName =="PCO.Edge 3B") return PCOedge3B;
  if (_cameraName =="PCO.Edge Fibre") return PCOedgeFibre;
  if (_cameraName =="CPro") return CPro;
  if (_cameraName =="HamaMama") return HamaMama;
  if (_cameraName =="Xenia") return Xenia;
  if (_cameraName =="Xenia+PPS") return XeniaPPS;
  if (_cameraName =="Eiger") return Eiger;
  if (_cameraName =="Eiger+PPS") return EigerPPS;
  return NONE;
}

const QList<Detector::Camera> Detector::knownCameras = ( QList<Detector::Camera> ()
      << Detector::NONE
      << Detector::ScintX
      << Detector::HamaPapa
      << Detector::Argus
      << Detector::PCOedge2B
      << Detector::PCOedge3B
      << Detector::PCOedgeFibre
      << Detector::CPro
      << Detector::HamaMama
      << Detector::Xenia
      << Detector::XeniaPPS
      << Detector::Eiger
      << Detector::EigerPPS
      ) ;


void Detector::setCamera(Camera _cam) {
  const Camera oldcam=_camera;
  _camera = _cam;
  switch (_cam) {
  case NONE:          setCamera("");                 break;
  case ScintX:        setCamera("SR08ID01DET05");    break;
  case HamaPapa:      setCamera("SR08ID01DET04");    break;
  case Argus:         setCamera("SR08ID01DET03");    break;
  case PCOedge2B:     setCamera("SR08ID01DET01");    break;
  case PCOedge3B:     setCamera("SR08ID01DET02");    break;
  case PCOedgeFibre:  setCamera("SR08ID01DETIOC10"); break;
  case CPro:          setCamera("SR08ID01DETIOC06"); break;
  case HamaMama:      setCamera("SR08ID01DETIOC08"); break;
  case Xenia:         setCamera("SR99ID01DALSA");    break;
  case XeniaPPS:      setCamera("SR99ID01DALSA");    break;
  case Eiger:         setCamera("SR08ID01E2");       break;
  case EigerPPS:      setCamera("SR08ID01E2");       break;
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

    enableTifPv->setPV(pvName + ":TIFF:EnableCallbacks");
    pathTifPv->setPV(pvName + ":TIFF:FilePath_RBV");
    pathSetTifPv->setPV(pvName + ":TIFF:FilePath");
    pathExistsTifPv->setPV(pvName + ":TIFF:FilePathExists_RBV");
    nameTifPv->setPV(pvName + ":TIFF:FileName");
    nameTemplateTifPv->setPV(pvName + ":TIFF:FileTemplate");
    lastNameTifPv->setPV(pvName + ":TIFF:FullFileName_RBV");
    autoSaveTifPv->setPV(pvName + ":TIFF:AutoSave");
    autoIncrementTifPv->setPV(pvName + ":TIFF:AutoIncrement");
    fileNumberTifPv->setPV(pvName + ":TIFF:FileNumber");
    writeStatusTifPv->setPV(pvName+":TIFF:WriteStatus");
    writeProggressTifPv->setPV(pvName+":TIFF:WriteFile_RBV");
    writeModeTifPv->setPV(pvName + ":TIFF:FileWriteMode");
    captureTargetTifPv->setPV(pvName + ":TIFF:NumCapture");
    captureProgressTifPv->setPV(pvName + ":TIFF:NumCaptured_RBV");
    captureTifPv->setPV(pvName + ":TIFF:Capture");
    captureStatusTifPv->setPV(pvName + ":TIFF:Capture_RBV");
    queUseTifPv->setPV(pvName + ":TIFF:QueueUse");

    enableHdfPv->setPV(pvName + ":HDF:EnableCallbacks");
    pathHdfPv->setPV(pvName + ":HDF:FilePath_RBV");
    pathSetHdfPv->setPV(pvName + ":HDF:FilePath");
    pathExistsHdfPv->setPV(pvName + ":HDF:FilePathExists_RBV");
    nameHdfPv->setPV(pvName + ":HDF:FileName");
    nameTemplateHdfPv->setPV(pvName + ":HDF:FileTemplate");
    lastNameHdfPv->setPV(pvName + ":HDF:FullFileName_RBV");
    autoSaveHdfPv->setPV(pvName + ":HDF:AutoSave");
    autoIncrementHdfPv->setPV(pvName + ":HDF:AutoIncrement");
    fileNumberHdfPv->setPV(pvName + ":HDF:FileNumber");
    writeStatusHdfPv->setPV(pvName + ":HDF:WriteStatus");
    writeProggressHdfPv->setPV(pvName + ":HDF:WriteFile_RBV");
    writeModeHdfPv->setPV(pvName + ":HDF:FileWriteMode");
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

  _nameTif = fromVList(nameTifPv->get());
  _nameTemplateTif = fromVList(nameTemplateTifPv->get());
  _pathTif = fromVList( pathTifPv->get() );
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
  else if ( ! queUseTifPv->isConnected() || ! queUseTifPv->get().toInt() ||
            ! queUseHdfPv->isConnected() || ! queUseHdfPv->get().toInt() )
    emit writingFinished();

  if ( writeStatusTifPv->get().toInt())
    emit writingError(lastName(TIF));
  if ( writeStatusHdfPv->get().toInt())
    emit writingError(lastName(HDF));

  if (sender() == captureStatusHdfPv || sender() == captureStatusTifPv)
    if (!isCapturing())
      emit capturingFinished();

  emit parameterChanged();

}



void Detector::updatelastNameTif() {
  if ( ! _camera )
    return;
  _lastName = fromVList(lastNameTifPv->get());
  if (_lastName != _lastNameTif)
    emit lastNameChanged(_lastNameTif=_lastName);
}

void Detector::updatelastNameHdf() {
  if ( ! _camera )
    return;
  _lastName = fromVList(lastNameHdfPv->get());
  if (_lastName != _lastNameHdf)
    emit lastNameChanged(_lastNameHdf=_lastName);
}

void Detector::updateFileNumberTif() {
  if ( ! _camera )
    return;
  emit fileNumberChanged( fileNumberTifPv->get().toInt() );
}

void Detector::updateQueUseTif() {
  if ( ! _camera )
    return;
  emit queTifChanged( queUseTifPv->get().toInt() );
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
  if (enableTifPv->get().toBool())
    return TIF;
  else if (hdfReady()  &&  enableHdfPv->get().toBool())
    return HDF;
  else
    return UNDEFINED;
}


bool Detector::imageFormat(Detector::ImageFormat fmt) const {
  if ( ! _camera )
    return false;
  if (fmt==TIF)
    return enableTifPv->get().toBool();
  else if (fmt==HDF)
    return hdfReady() && enableHdfPv->get().toBool();
  else
    return false;
}


const QString & Detector::path(Detector::ImageFormat fmt) const {
  if (fmt==TIF)
    return _pathTif;
  else if (fmt==HDF)
    return _pathHdf;
  else
    return emptyStr;
}


const QString & Detector::nameTemplate(Detector::ImageFormat fmt) const {
  if (fmt==TIF)
    return _nameTemplateTif;
  else if (fmt==HDF)
    return _nameTemplateHdf;
  else
    return emptyStr;
}

const QString & Detector::name(Detector::ImageFormat fmt) const {
  if (fmt==TIF)
    return _nameTif;
  else if (fmt==HDF)
    return _nameHdf;
  else
    return emptyStr;
}

const QString & Detector::lastName(Detector::ImageFormat fmt) const {
  if (fmt==TIF)
    return _lastNameTif;
  else if (fmt==HDF)
    return _lastNameHdf;
  else
    return _lastName;
}

bool Detector::pathExists(Detector::ImageFormat fmt) const {
  if ( ! _camera )
    return true;
  if (fmt==TIF)
    return pathExistsTifPv->get().toBool();
  else if (fmt==HDF)
    return ! hdfReady() ||  pathExistsHdfPv->get().toBool();
  else
    return pathExists(TIF) && pathExists(HDF);
}

bool Detector::isWriting(Detector::ImageFormat fmt) const {
  if ( ! _camera )
    return false;
  if (fmt==TIF)
    return writeProggressTifPv->get().toInt() || ( queUseTifPv->isConnected() ? queUseTifPv->get().toInt() : false );
  else if (fmt==HDF) {
    if ( ! hdfReady() )
      return false;
    return writeProggressHdfPv->get().toInt() || ( queUseHdfPv->isConnected() ? queUseHdfPv->get().toInt() : false );
  } else {
    auto qwrTf = writeProggressTifPv->get();
    auto qwrHd = writeProggressHdfPv->get();
    bool wrTf = writeProggressTifPv->get().toInt();
    bool wrHd = writeProggressHdfPv->get().toInt();
    bool wr = wrTf || wrHd;
    return isWriting(TIF) || isWriting(HDF);
  }
}



bool Detector::isCapturing(Detector::ImageFormat fmt) const {
  if ( ! _camera )
    return false;
  if (fmt==TIF)
    return captureStatusTifPv->get().toInt() ;
  else if (fmt==HDF) {
    if ( ! hdfReady() )
      return false;
    return captureStatusHdfPv->get().toInt() ;
  } else
    return isCapturing(TIF) || isCapturing(HDF);
}





bool Detector::setImageFormat(Detector::ImageFormat fmt) {
  if ( ! _camera )
    return true;
  if (fmt == UNDEFINED)
    return false;
  enableTifPv->set(fmt == TIF);
  enableHdfPv->set(fmt == HDF);
  qtWait(enableTifPv, SIGNAL(valueUpdated(QVariant)), 500);
  qtWait(enableHdfPv, SIGNAL(valueUpdated(QVariant)), 500);
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

  if ( ! numberPv->isConnected() || isAcquiring() )
    return false;

  int reqIM;
  if (val>0) {
    numberPv->set(val);
    qtWait(numberPv, SIGNAL(valueUpdated(QVariant)), 500);
    reqIM = val>1 ? 1 : 0;
  } else {
    reqIM = 2;
  }
  setImageMode(reqIM);

  return
      ( val < 1 || number() == val ) &&
      imageMode() == reqIM;

}

bool Detector::setNameTemplate(ImageFormat fmt, const QString & ntemp) {

  if ( ! _camera )
    return true;
  if ( isAcquiring() )
    return false;
  if (nameTemplate(fmt) == ntemp)
    return true;

  if ( fmt == TIF  && nameTemplateTifPv->isConnected() ) {
    nameTemplateTifPv->set(ntemp.toLatin1().append(char(0)));
    qtWait(nameTemplateTifPv, SIGNAL(valueUpdated(QVariant)), 500);
  } else if ( fmt == HDF  && nameTemplateHdfPv->isConnected() ) {
    if ( ! hdfReady() )
      return true;
    nameTemplateHdfPv->set(ntemp.toLatin1().append(char(0)));
    qtWait(nameTemplateHdfPv, SIGNAL(valueUpdated(QVariant)), 500);
  } else {
    return setNameTemplate(TIF, ntemp) && setNameTemplate(HDF, ntemp);
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

  if ( fmt == TIF  && nameTifPv->isConnected() ) {
    nameTifPv->set(fname.toLatin1().append(char(0)));
    qtWait(nameTifPv, SIGNAL(valueUpdated(QVariant)), 500);
  } else if ( fmt == HDF  && nameHdfPv->isConnected() ) {
    if ( ! hdfReady() )
      return true;
    nameHdfPv->set(fname.toLatin1().append(char(0)));
    qtWait(nameHdfPv, SIGNAL(valueUpdated(QVariant)), 500);
  } else {
    return setName(TIF, fname) && setName(HDF, fname);
  }
  return name(fmt) == fname ;

}

bool Detector::setPath(const QString & _path) {

  if ( ! _camera )
    return true;

  const QByteArray _ptoset = _path.toLatin1().append(char(0));

  if ( pathSetTifPv->isConnected()  &&  path(TIF) != _path )
    pathSetTifPv->set(_ptoset);
  if ( pathSetHdfPv->isConnected()  &&  path(HDF) != _path )
    pathSetHdfPv->set(_ptoset);
  // here set and check are split to ensure no signals are processed while qtWaiting.
  if ( pathSetTifPv->isConnected()  &&  path(TIF) != _path )
    qtWait(pathTifPv, SIGNAL(valueUpdated(QVariant)), 500);
  if ( pathSetHdfPv->isConnected()  &&  path(HDF) != _path )
    qtWait(pathHdfPv, SIGNAL(valueUpdated(QVariant)), 500);

  return path(TIF) == _path  &&  path(HDF) == _path;

}


bool Detector::setAutoSave(bool autoSave) {
  if ( ! _camera )
    return true;
  if ( imageFormat() == TIF ) {
    autoIncrementTifPv->set(1);
    autoSaveTifPv->set(autoSave ? 1 : 0);
    qtWait(autoSaveTifPv, SIGNAL(valueUpdated(QVariant)), 500);
    return autoSaveTifPv->get().toBool() == autoSave  &&
           autoIncrementTifPv->get().toBool();
  }
  if ( imageFormat() == HDF ) {
    autoIncrementHdfPv->set(1);
    autoSaveHdfPv->set(autoSave ? 1 : 0);
    qtWait(autoSaveHdfPv, SIGNAL(valueUpdated(QVariant)), 500);
    return autoSaveHdfPv->get().toBool() == autoSave &&
           autoIncrementHdfPv->get().toBool() ;
  }
  return true;
}



bool Detector::prepareForAcq(Detector::ImageFormat fmt, int nofFrames) {

  if ( ! _camera )
    return true;
  if ( isAcquiring() || fmt == UNDEFINED || nofFrames <= 0 )
    return false;

  setImageFormat(fmt);

  if ( fmt == TIF ) {

    if ( captureStatusTifPv->get().toInt() == 1 ) // capturing
      return false;
    writeModeTifPv->set(0); // 0 is "single"
    if ( writeModeTifPv->get() != 0 ) {
      qtWait(writeModeTifPv, SIGNAL(valueUpdated(QVariant)), 500);
      if ( writeModeTifPv->get() != 0 )
        return false;
    }

    setAutoSave(1);
    if (fileNumberTifPv->get().toInt() !=0 ) {
      fileNumberTifPv->set(0);
      qtWait(fileNumberTifPv, SIGNAL(valueUpdated(QVariant)), 500);
      if ( fileNumberTifPv->get().toInt() !=0 )
        return false;
    }

  } else if (fmt == HDF) {

    if ( ! hdfReady() )
      return false;
    if ( captureStatusHdfPv->get().toInt() == 1 ) // capturing
      return false;

    captureTargetHdfPv->set(nofFrames); // 0 frames for unlimited
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

  if ( _camera == HamaPapa ) {
     if ( ! setTriggerMode(1) )
       return false;
  } else if ( _camera == XeniaPPS  ||  _camera == EigerPPS ) {
      if ( ! setTriggerMode(2) )
        return false;
   } else if ( _camera != Argus ) {
     if ( ! setTriggerMode(0) )
       return false;
  }

  return true;

}


bool Detector::prepareForVid(Detector::ImageFormat fmt, int nofFrames) {

  if ( ! _camera )
    return true;
  if ( isAcquiring() || fmt == UNDEFINED  )
    return false;

  if ( nofFrames <= 0 ) // 0 frames for unlimited
    nofFrames = 0;
  setImageFormat(fmt);

  if ( fmt == TIF ) {

    setAutoSave(0);
    if ( captureStatusTifPv->get().toInt() == 1 ) // capturing
      return false;
    captureTargetTifPv->set(nofFrames);
    if ( captureTargetTifPv->get() !=nofFrames ) {
      qtWait(captureTargetTifPv, SIGNAL(valueUpdated(QVariant)), 500);
      if ( captureTargetTifPv->get() !=nofFrames )
        return false;
    }
    writeModeTifPv->set(2); // 2 is "stream"
    if ( writeModeTifPv->get() != 2 ) {
      qtWait(writeModeTifPv, SIGNAL(valueUpdated(QVariant)), 500);
      if ( writeModeTifPv->get() != 2 )
        return false;
    }
    captureTifPv->set(1);
    if ( captureStatusTifPv->get() != 1 ) {
      qtWait(captureStatusTifPv, SIGNAL(valueUpdated(QVariant)), 500);
      if ( captureStatusTifPv->get() != 1 )
        return false;
    }

  } else if (fmt == HDF) {

    if ( ! hdfReady() )
      return false;
    if ( captureStatusHdfPv->get().toInt() == 1 ) // capturing
      return false;

    captureTargetHdfPv->set(nofFrames); // 0 frames for unlimited
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

  if ( _camera == HamaPapa ) {
     if ( ! setTriggerMode(1) )
       return false;
  } else if ( _camera == XeniaPPS  ||  _camera == EigerPPS ) {
      if ( ! setTriggerMode(2) )
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

  if (tmode !=3 && _camera == Eiger )
    QEpicsPv::set(cameraPv + ":CAM:NumTriggers", 1, 500);

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
    case (PCOedgeFibre) :
      // mode = 2; // Ext + Soft
      mode = 4; // Ext Only
      break;
    case (XeniaPPS) :
    case (EigerPPS) :
      mode = 2;
      break;
    case (Eiger) :
      mode = 3; // External Enable
      QEpicsPv::set(cameraPv + ":CAM:NumTriggers", number(), 500);
      numberPv->set(1);
      qtWait(numberPv, SIGNAL(valueUpdated(QVariant)), 500);
      break;
    default :
      mode = 1;
      break;
    }
  }

  return setTriggerMode(mode);

}


bool Detector::startCapture() {
  if ( imageFormat() == HDF && captureHdfPv->isConnected() ) {
    captureHdfPv->set(1);
    return captureStatusHdfPv->get().toBool();
  } if ( imageFormat() == TIF && captureTifPv->isConnected() ) {
    captureTifPv->set(1);
    return captureStatusTifPv->get().toBool();
  } else
    return false;
}


bool Detector::start() {
  if ( ! _camera )
    return true;
  if ( ! aqPv->isConnected() || ! writeProggressTifPv->isConnected() || isAcquiring() )
    return false;
  aqPv->set(1);
  qtWait(aqPv, SIGNAL(valueUpdated(QVariant)), 2000);
  return aqPv->get().toInt();
}

void Detector::stop() {
  if ( aqPv->isConnected() )
    aqPv->set(0);
  if ( captureHdfPv->isConnected() )
    captureHdfPv->set(0);
  if ( captureTifPv->isConnected() )
    captureTifPv->set(0);
  setAutoSave(false);
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


void Detector::waitCaptured() {
  if ( ! _camera )
    return;
  if (isCapturing())
    qtWait(this, SIGNAL(capturingFinished()));
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
  while ( isWriting() || queUseTifPv->get().toInt() )
    qtWait(this, SIGNAL(writingFinished()), 500);

  //  timer.start();

}





