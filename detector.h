#ifndef DETECTOR_H
#define DETECTOR_H

#include <QObject>
#include <qtpv.h>
#include <QTimer>


class Detector : public QObject {
  Q_OBJECT;

public:

  enum Camera {
    NONE=0,
    Amethyst,
    Ruby,
    BYPV
  };

private:

  Camera cam;
  QEpicsPv * expPv; // AcquireTime (_RBV)
  QEpicsPv * intPv; // AcquirePeriod
  QEpicsPv * numPv; //NumImages
  QEpicsPv * numCapPv; // NumCapture
  QEpicsPv * numCompletePv; // NumCapture
  QEpicsPv * triggerModePv;//TriggerMode
  QEpicsPv * imageModePv;//ImageMode
  QEpicsPv * aqPv; //Acquire
  QEpicsPv * fileNumberPv; // FileNumber
  QEpicsPv * fileNamePv; // FileName
  QEpicsPv * autoSavePv;
  QEpicsPv * capturePv;
  QEpicsPv * writeModePv;
  QEpicsPv * writeStatusPv;

  QTimer internalTimer;


  bool _con;
  bool _isReady;
  int _counter;
  QString _name;

public:

  Detector(QObject * parent=0);

  void setCamera(Camera _cam);
  void setCamera(const QString & pvName);

  inline double exposure() const {return expPv->get().toDouble();}
  inline double interval() const {return intPv->get().toDouble();}
  inline int num() const {return numPv->get().toInt();}
  inline bool isAcquiring() const {return aqPv->get().toInt();}
  inline bool isConnected() const {return _con;}
  inline bool isReady() const {return _isReady;}

public slots:

  void setExposure(double val);
  void setInterval(double val);
  void setNum(int val);
  void setNameTemplate(const QString & ntemp);
  bool prepare();
  void start();
  void stop();

signals:

  void connectionChanged(bool);
  void readynessChanged(bool);
  void exposureChanged(double);
  void done();
  void startImage(int);

private slots:

  void updateConnection();
  void updateReadyness();
  void updateAcquisition();
  void updateExposure();
  void updateCounter();
  void trig();

};





#endif // DETECTOR_H
