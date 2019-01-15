#ifndef POSITIONLIST_H
#define POSITIONLIST_H

#include <QWidget>
#include <QSettings>
#include "ui_positionlist.h"
#include <qcamotorgui.h>

class PositionList : public QWidget
{
  Q_OBJECT;

private:
  bool allOK;
  bool freezListUpdates;

public:

  Ui::PositionList * const ui; // do not change outside public only to store
  QCaMotorGUI * motui; // do not change outside public only to move and store

  explicit PositionList(QWidget *parent = 0);
  ~PositionList();

  void putMotor(QCaMotorGUI * motor);
  void putLabel(const QString & lab) {ui->label->setText(lab);}
  bool amOK() {return allOK;}
  void freezList(bool fz) {freezListUpdates=fz;}
  void emphasizeRow(int row=-1);

private slots:

  void updateAmOK();
  void updateNoF();
  void moveMotorHere();
  void getMotorPosition();

signals:

  void amOKchanged(bool);
  void parameterChanged();

};

#endif // POSITIONLIST_H
