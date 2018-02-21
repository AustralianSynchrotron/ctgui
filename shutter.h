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

class Shutter : public QObject {
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
  void waitForUpdate();
  static const QStringList fakeShutter;
  static const QHash<QString, QStringList> listOfKnownShutters;
  const QStringList readCustomDialog() const;
  void loadCustomDialog(const QStringList & desc);

public:

  explicit Shutter(QWidget *parent = 0);

  ~Shutter();

  static const QStringList knownShutters() { return listOfKnownShutters.keys(); }

  void setShutter(const QStringList & desc = QStringList() );

  void setShutter(const QString & shutterName) {
    if ( knownShutters().contains(shutterName) )
        setShutter(listOfKnownShutters[shutterName]);
  }

  State state() const ;

public slots:

  void open(bool wait=true) ;
  void close(bool wait=true);
  void toggle(bool wait=true) { if ( state() == CLOSED ) open(wait) ; else close(wait) ;}

private slots:

  void requestUpdate();
  void onSelection();
  void onOpenUpdate(const QString & newOpen) {lastOpen=newOpen; onStatusUpdate();}
  void onClosedUpdate(const QString & newClosed) {lastClosed=newClosed; onStatusUpdate();}
  void onStatusUpdate();

signals:

  void stateUpdated(State);
  void opened();
  void closed();

};

#endif // SHUTTER_H
