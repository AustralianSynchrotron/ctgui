#include <QPlainTextEdit>
#include <QLabel>
#include <QDebug>
#include <QLineEdit>
#include <QToolButton>
#include <QStyle>
#include <QProcess>
#include <QTemporaryFile>
#include <QGridLayout>
#include <QTableWidget>
#include <QWidgetList>
#include <QProgressBar>
#include <QCheckBox>
#include "ui_upvorcom.h"

#include <qtpv.h>

#ifndef CTGUIADDITIONALCLASSES
#define CTGUIADDITIONALCLASSES

/*
namespace Ui {
  class QSCheckBox;
}
*/

class QSCheckBox : public QCheckBox {
  Q_OBJECT;
  //Ui::QSCheckBox ui;
public :
  QSCheckBox(QWidget * parent = 0);

protected:
  virtual bool hitButton ( const QPoint & pos ) const ;

};



struct ColumnResizerPrivate;
class ColumnResizer : public QObject {
  Q_OBJECT;
public:
  ColumnResizer(QObject* parent = 0);
  ~ColumnResizer();

  void addWidget(QWidget* widget);
  void addWidgetsFromGridLayout(QGridLayout*, int column);

private Q_SLOTS:
  void updateWidth();

protected:
  bool eventFilter(QObject*, QEvent* event);

private:
  ColumnResizerPrivate* const d;
};



class QPTEext : public QPlainTextEdit {
  Q_OBJECT;
public:
  QPTEext(QWidget * parent=0) : QPlainTextEdit(parent) {};
signals:
  void editingFinished();
  void focusIned();
  void focusOuted();
protected:
  bool event(QEvent *event);
  void focusInEvent ( QFocusEvent * e );
  void focusOutEvent ( QFocusEvent * e );
};


class CtGuiLineEdit : public QLineEdit {
  Q_OBJECT;
private:
  QToolButton *clearButton;
public:
  CtGuiLineEdit(QWidget *parent);
protected:
  void resizeEvent(QResizeEvent *);
private slots:
  void updateCloseButton(const QString& text) {
    clearButton->setVisible(!text.isEmpty());
  }
};



class EasyTabWidget : public QTabWidget {
  Q_OBJECT;
  QWidgetList wdgs;
  QHash<QWidget*, QString> titles;

public:
  EasyTabWidget( QWidget * parent = 0 ) : QTabWidget(parent) {}
  void finilize();
  void setTabVisible(QWidget * tab, bool visible);
  void setTabTextColor(QWidget * tab, const QColor & color);
  const QWidgetList & tabs() const {return wdgs;};
  QWidgetList tabs() {return wdgs;}

};



class CTprogressBar : public QProgressBar {
  Q_OBJECT;
  QLabel * label;
  void updateLabel();

public:
  CTprogressBar(QWidget * parent=0);
  void setTextVisible(bool visible) ;

public slots:
  void setValue(int value) ;
  void setMinimum(int minimum);
  void setMaximum(int maximum);

protected:
  virtual void	resizeEvent(QResizeEvent * event);

};


class Script : public QObject {
  Q_OBJECT;

private:
  QProcess proc;
  QTemporaryFile fileExec;
  QString _path;
  static const QString shell;

public:
  explicit Script(QObject *parent = 0);

  const QString & path() const {return _path;}
  const QString out() {return proc.readAllStandardOutput();}
  const QString err() {return proc.readAllStandardError();}
  int exitCode() const { return proc.exitCode(); }
  int waitStop();
  bool isRunning() const { return proc.state() != QProcess::NotRunning; }
  int evaluate(const QString & par = QString());
  static int executeOnce(const QString & par = QString()) { Script scr; return scr.execute(par); }

public slots:
  bool start(const QString & par = QString());
  void stop() {if (isRunning()) proc.kill();}
  int execute( const QString & par = QString() ) { return start(par) ? waitStop() : -1 ; }
  const QString & setPath( const QString & _p );

private slots:
  void onState(QProcess::ProcessState state);

signals:
  void pathSet(const QString & newPath);
  void finished(int status);
  void started();

};



namespace Ui {
class UScript;
}

class UScript : public QWidget {
  Q_OBJECT;

private:
  Ui::UScript *ui;

public:
  Script * script;

public:
  explicit UScript(QWidget *parent = 0);
  ~UScript();

  void addToColumnResizer(ColumnResizer * columnizer);

private slots:
  void browse();
  void onStartStop() { if (script->isRunning()) script->stop(); else script->start(); }
  void updateState();
  void updatePath();


signals:
  void editingFinished();

};




class PVorCOM : public QObject {
  Q_OBJECT;

  QEpicsPv * pv;
  Script * sr;

public:

  enum WhoAmI {
    EPICSPV,
    GOODSCRIPT,
    BADSCRIPT,
    RUNNINGSCRIPT
  };

  PVorCOM(QObject *parent = 0);

public slots:

  void setName( const QString & nm = QString() );
  const QString & getName() const { return pv->pv(); }
  void put( const QString & val = QString() ) ;
  const QString get() const  { return pv->isConnected()  ?  pv->get().toString()  :  sr->out();  }
  WhoAmI state();

signals:

  void valueUpdated( const QString & );
  void nameUpdated( const QString & );
  void stateUpdated(PVorCOM::WhoAmI);

private slots:

  void updateVal() { emit valueUpdated(get()); }
  void updateState() { emit stateUpdated(state());}

};


namespace Ui {
class UPVorCOM;
}


class UPVorCOM : public QWidget {
  Q_OBJECT;
private:
  Ui::UPVorCOM *ui;
public:
  PVorCOM * pvc;
  UPVorCOM (QWidget *parent = 0);
  void setPlaceholderText(const QString & text) { ui->name->setPlaceholderText(text); }
private slots:
  void setValueText(const QString & txt);
  void setPVCname() { pvc->setName(ui->name->text()); }
  void indicateState(PVorCOM::WhoAmI state);
  void evaluateScript();
};




#endif // CTGUIADDITIONALCLASSES
