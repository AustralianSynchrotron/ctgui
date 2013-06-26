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
    ScintX,
    PCOedge1,
    PCOedge2,
    BYPV
  };

  static QString cameraName(Camera cam);
  static const QList<Camera> knownCameras;
  static Camera camera(const QString & _cameraName);

private:

  QEpicsPv * exposurePv; // AcquireTime (_RBV)
  QEpicsPv * periodPv; // AcquirePeriod
  QEpicsPv * numberPv; //NumImages
  QEpicsPv * counterPv; // NumCapture
  QEpicsPv * triggerModePv;//TriggerMode
  QEpicsPv * imageModePv;//ImageMode
  QEpicsPv * aqPv; //Acquire
  QEpicsPv * pathPv;
  QEpicsPv * pathExistsPv;
  QEpicsPv * namePv;
  QEpicsPv * nameTemplatePv;
  QEpicsPv * fileNumberPv;
  QEpicsPv * lastNamePv;
  QEpicsPv * autoSavePv;
  QEpicsPv * writeStatusPv;
  QEpicsPv * delayPv;



  bool _con;
  QString cameraPv;
  QString _name;
  QString _nameTemplate;
  QString _lastName;
  QString _path;

  //QStringList _names;


public:

  Detector(QObject * parent=0);

  void setCamera(Camera _cam);
  void setCamera(const QString & pvName);

  inline const QString & pv() const {return cameraPv;}
  inline double exposure() const {return exposurePv->get().toDouble();}
  // inline double period() const {return periodPv->get().toDouble();}
  inline double period() const {return exposure() + delayPv->get().toDouble();}
  inline int number() const {return numberPv->get().toInt();}
  inline int counter() const {return counterPv->get().toInt();}
  inline int triggerMode() const {return triggerModePv->get().toInt();}
  inline const QString & triggerModeString() const {return triggerModePv->getEnumString();}
  inline int imageMode() const {return imageModePv->get().toInt();}
  inline const QString & imageModeString() const {return imageModePv->getEnumString();}
  inline const QString & path() const {return _path;}
  inline const QString & nameTemplate() const {return _nameTemplate;}
  inline const QString & name() const {return _name;}
  inline const QString & lastName() const {return _lastName;}
  inline bool pathExists() { return pathExistsPv->get().toBool();}
  inline bool isAcquiring() const {return aqPv->get().toInt();}
  inline bool isWriting() const {return writeStatusPv->get().toInt();}
  inline bool isConnected() const {return _con;}

  void waitDone();
  void waitWritten();

public slots:

  bool setPeriod(double val);
  bool setNumber(int val);
  bool setName(const QString & fname);
  bool setNameTemplate(const QString & ntemp);
  bool setHardwareTriggering(bool set);
  bool start();
  void stop();
  bool acquire();
  bool prepareForAcq();
  bool setAutoSave(bool autoSave);

signals:

  void exposureChanged(double);
  void connectionChanged(bool);
  void parameterChanged(); // exp int num trigM imagM path writeSt
  void counterChanged(int);
  void templateChanged(const QString &);
  void nameChanged(const QString &);
  void lastNameChanged(const QString &);
  void writingStarted();
  void frameWritingFinished();
  void done();

private slots:

  void updateExposure();
  void updateConnection();
  void updateCounter();
  void updatePath();
  void updateName();
  void updateNameTemplate();
  void updateLastName();
  void updateAcq();
  void updateWriting();

};





#endif // DETECTOR_H
