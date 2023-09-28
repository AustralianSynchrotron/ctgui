#ifndef SHUTTER_H
#define SHUTTER_H

#include <QWidget>
#include <QPair>
#include <QStringList>
#include <QHash>
#include <QDialog>
#include "additional_classes.h"
#include "ui_shutter.h"
#include "ui_ushutterconf.h"


namespace Ui {
class Shutter;
class UShutterConf;
}

class Shutter : public QWidget {
  Q_OBJECT;

public:

  enum State {
    OPEN = 1,
    CLOSED = 0,
    BETWEEN = 2
  };

  Ui::Shutter *ui;
  Ui::UShutterConf *customUi;
  QDialog *customDlg;

private:

  typedef QPair<PVorCOM *, QString> PCVal;

  PCVal doOpen;
  PCVal doClose;
  PCVal isOpen;
  PCVal isClosed;

  QString lastOpen;
  QString lastClosed;
  void waitForState(State st);
  bool amFake=true;
  static const QStringList fakeShutter;
  static const QHash<QString, QStringList> listOfKnownShutters;

public:

  explicit Shutter(QWidget *parent = 0);

  ~Shutter();

  static const QStringList knownShutters() { return listOfKnownShutters.keys(); }

  void setShutter(const QStringList & desc);
  void setShutter(const QString & shutterName=QString());

  State state() const ;
  void loadCustomDialog(const QStringList & desc);
  const QStringList readCustomDialog() const;
  QStringList shutterConfiguration() const;


public slots:

  void setState(State st, bool wait=true) { if (st==CLOSED) open(wait) ; else if (st==OPEN) close(wait) ; }
  void open(bool wait=true) ;
  void close(bool wait=true);
  void toggle(bool wait=true) { if ( state() == CLOSED ) open(wait) ; else close(wait) ;}

private slots:

  void requestUpdate();
  void onOpenUpdate(const QString & newOpen) {lastOpen=newOpen; onStatusUpdate();}
  void onClosedUpdate(const QString & newClosed) {lastClosed=newClosed; onStatusUpdate();}
  void onStatusUpdate();
  void onSelection();
  void onLoadPreset();

signals:

  void shutterChanged();
  void stateUpdated(State);
  void opened();
  void closed();

};

#endif // SHUTTER_H
