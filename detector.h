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
  QEpicsPv * filePathPv;
  QEpicsPv * filePathExistsPv;
  QEpicsPv * fileNamePv;
  QEpicsPv * fileTemplatePv;
  QEpicsPv * fileNumberPv;
  QEpicsPv * fileLastNamePv;
  QEpicsPv * autoSavePv;
  QEpicsPv * writeStatusPv;


  bool _con;
  QString cameraPv;
  QString _name;
  QString _nameTemplate;
  QString _nameLast;
  QString _path;

public:

  Detector(QObject * parent=0);

  void setCamera(Camera _cam);
  void setCamera(const QString & pvName);

  inline const QString & pv() const {return cameraPv;}
  inline double exposure() const {return exposurePv->get().toDouble();}
  inline double period() const {return periodPv->get().toDouble();}
  inline int number() const {return numberPv->get().toInt();}
  inline int counter() const {return counterPv->get().toInt();}
  inline int triggerMode() const {return triggerModePv->get().toInt();}
  inline const QString & triggerModeString() const {return triggerModePv->getEnumString();}
  inline int imageMode() const {return imageModePv->get().toInt();}
  inline const QString & imageModeString() const {return imageModePv->getEnumString();}
  inline const QString & filePath() const {return _path;}
  inline const QString & fileTemplate() const {return _nameTemplate;}
  inline const QString & fileName() const {return _name;}
  inline const QString & fileLastName() const {return _nameLast;}
  inline bool pathExists() { return filePathExistsPv->get().toBool();}
  inline bool isAcquiring() const {return aqPv->get().toInt();}
  inline bool isConnected() const {return _con;}

  void waitDone();

public slots:

  bool setInterval(double val);
  bool setNumber(int val);
  bool setName(const QString & fname);
  bool start();
  void stop();
  bool acquire();
  bool prepareForAcq();

signals:

  void connectionChanged(bool);
  void parameterChanged(); // exp int num trigM imagM path writeSt
  void counterChanged(int);
  void templateChanged(const QString &);
  void nameChanged(const QString &);
  void lastNameChanged(const QString &);
  void done();

private slots:

  void updateConnection();
  void updateCounter();
  void updatePath();
  void updateName();
  void updateNameTemplate();
  void updateLastName();
  void updateAcq();

private:
  bool setNameTemplate(const QString & ntemp);

};





#endif // DETECTOR_H
