#ifndef DETECTOR_H
#define DETECTOR_H

#include <QObject>
#include <qtpv.h>
#include <QTimer>
#include <QDebug>


class Detector : public QObject {
  Q_OBJECT;

public:

  enum Camera {
    NONE=0,
    PCOedge2B,
    PCOedge3B,
    PCOedgeFibre,
    ScintX,
    HamaPapa,
    Argus,
    CPro,
    HamaMama,
    Xenia,
    XeniaPPS,
    Eiger,
    EigerPPS,
    BYPV
  };

  enum ImageFormat {
    UNDEFINED=0,
    TIF,
    HDF
  };

  static QString cameraName(Camera cam);
  static const QList<Camera> knownCameras;
  static Camera camera(const QString & _cameraName);

private:

  QEpicsPv * setExposurePv; // AcquireTime
  QEpicsPv * exposurePv; // AcquireTime (_RBV)
  QEpicsPv * periodPv; // AcquirePeriod
  QEpicsPv * numberPv; //NumImages
  QEpicsPv * counterPv; // NumCapture
  QEpicsPv * triggerModePv;//TriggerMode
  QEpicsPv * imageModePv;//ImageMode
  QEpicsPv * imageModeRbvPv;//ImageMode
  QEpicsPv * aqPv; //Acquire

  QEpicsPv * enableTifPv; // Tif:EnableCallbacks
  QEpicsPv * pathTifPv;
  QEpicsPv * pathSetTifPv;
  QEpicsPv * pathExistsTifPv;
  QEpicsPv * nameTifPv;
  QEpicsPv * nameTemplateTifPv;
  QEpicsPv * fileNumberTifPv;
  QEpicsPv * lastNameTifPv;
  QEpicsPv * autoSaveTifPv;
  QEpicsPv * autoIncrementTifPv;
  QEpicsPv * writeStatusTifPv;
  QEpicsPv * writeProggressTifPv;
  QEpicsPv * writeModeTifPv;
  QEpicsPv * captureTargetTifPv;
  QEpicsPv * captureTargetTifRbvPv;
  QEpicsPv * captureProgressTifPv;
  QEpicsPv * captureTifPv;
  QEpicsPv * captureStatusTifPv;
  QEpicsPv * queUseTifPv;

  QEpicsPv * enableHdfPv; // HDF:EnableCallbacks
  QEpicsPv * pathHdfPv;
  QEpicsPv * pathSetHdfPv;
  QEpicsPv * pathExistsHdfPv;
  QEpicsPv * nameHdfPv;
  QEpicsPv * nameTemplateHdfPv;
  QEpicsPv * fileNumberHdfPv;
  QEpicsPv * lastNameHdfPv;
  QEpicsPv * autoSaveHdfPv;
  QEpicsPv * autoIncrementHdfPv;
  QEpicsPv * writeStatusHdfPv;
  QEpicsPv * writeProggressHdfPv;
  QEpicsPv * writeModeHdfPv;
  QEpicsPv * captureTargetHdfPv;
  QEpicsPv * captureTargetHdfRbvPv;
  QEpicsPv * captureProgressHdfPv;
  QEpicsPv * captureHdfPv;
  QEpicsPv * captureStatusHdfPv;
  QEpicsPv * queUseHdfPv;

  bool _con;
  bool _hdf;
  Camera _camera;
  QString cameraPv;
  QString _nameTif;
  QString _nameTemplateTif;
  QString _lastNameTif;
  QString _pathTif;
  QString _nameHdf;
  QString _nameTemplateHdf;
  QString _lastNameHdf;
  QString _pathHdf;
  QString _lastName;


public:

  Detector(QObject * parent=0);


  void setCamera(const QString & pvName);
  void setCamera(Camera _cam);

  inline Camera camera() const {return _camera;}
  inline const QString & pv() const {return cameraPv;}
  inline double exposure() const {return _camera ? exposurePv->get().toDouble() : 0 ;}
  inline double period() const { return _camera ? periodPv->get().toDouble() : 0 ; }
  inline int number() const {return _camera ? numberPv->get().toInt() : 0 ;}
  inline int counter() const {return _camera ? counterPv->get().toInt() : 0 ;}
  inline int triggerMode() const {return _camera ? triggerModePv->get().toInt() : 0 ;}
  inline const QString triggerModeString() const {return _camera ? triggerModePv->getEnumString() : QString() ;}
  inline int imageMode() const {return _camera ? imageModeRbvPv->get().toInt() : 0 ;}
  inline const QString imageModeString() const {return _camera ? imageModePv->getEnumString() : QString() ;}
  inline bool isAcquiring() const {return _camera  ? aqPv->get().toInt() : false ;}
  inline bool isConnected() const {return _camera && _con;}
  inline bool hdfReady() const {return _camera && _hdf;}

  ImageFormat imageFormat() const;
  bool imageFormat(ImageFormat fmt) const;
  const QString & path(ImageFormat fmt) const;
  const QString & nameTemplate(ImageFormat fmt) const ;
  const QString & name(ImageFormat fmt) const;
  const QString & lastName(ImageFormat fmt = UNDEFINED) const;
  int toCapture(ImageFormat fmt = UNDEFINED) const;
  int captured(ImageFormat fmt = UNDEFINED) const;
  bool pathExists(ImageFormat fmt = UNDEFINED) const ;
  bool isWriting(ImageFormat fmt = UNDEFINED) const ;
  bool isCapturing(ImageFormat fmt = UNDEFINED) const ;

  void waitDone();
  void waitWritten();
  void waitCaptured();

public slots:

  bool setExposure(double val);
  bool setPeriod(double val);
  bool setNumber(int val);
  bool setImageMode(int imode);
  bool setTriggerMode(int tmode);
  bool setHardwareTriggering(bool set);
  bool setImageFormat(ImageFormat fmt);
  bool setName(ImageFormat fmt, const QString & fname);
  bool setNameTemplate(ImageFormat fmt, const QString & ntemp);
  bool setPath(const QString & _path);
  bool startCapture();
  void stopCapture();
  bool start();
  void stop();
  bool acquire();
  bool prepareForAcq(ImageFormat fmt, int nofFrames);
  bool prepareForVid(ImageFormat fmt, int nofFrames=0);
  bool setAutoSave(bool autoSave);

signals:

  void connectionChanged(bool);
  void parameterChanged();
  void counterChanged(int);
  void fileNumberChanged(int);
  void lastNameChanged(const QString & name);
  void writingError(const QString & filename);
  void queTifChanged(int);
  void queHdfChanged(int);

  void writingStarted();
  void writingFinished();
  void capturingFinished();
  void done();


private slots:

  void updateConnection();
  void updateParameter();
  void updateCounter();
  void updateWriting();

  void updateFileNumberTif();
  void updatelastNameTif();
  void updateQueUseTif();

  void updatelastNameHdf();
  void updateQueUseHdf();

};





#endif // DETECTOR_H
