#include <QPlainTextEdit>
#include <QDebug>
#include <QLineEdit>
#include <QToolButton>
#include <QStyle>
#include <QProcess>

#ifndef CTGUIADDITIONALCLASSES
#define CTGUIADDITIONALCLASSES



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





namespace Ui {
class Script;
}

class Script : public QWidget {
  Q_OBJECT;

private:
  Ui::Script *ui;
  QProcess proc;

public:
  explicit Script(QWidget *parent = 0);
  ~Script();

  void setPath(const QString & _path);
  const QString out() {return proc.readAllStandardOutput();}
  const QString err() {return proc.readAllStandardError();}
  int waitStop();
  bool isRunning() const { return proc.pid(); };
  const QString path() const;

public slots:
  bool start();
  int execute() { return start() ? waitStop() : -1 ; };
  void stop() {if (isRunning()) proc.kill();};

private slots:
  void browse();
  void evaluate();
  void onState(QProcess::ProcessState state);
  void onStartStop() { if (isRunning()) stop(); else start(); };

signals:
  void editingFinished();
  void executed();
  void finished(int status);
  void started();

};



class FilterFileTemplateEvent : public QObject {
  Q_OBJECT;
public:
  FilterFileTemplateEvent(QObject * parent=0) :
    QObject(parent) {}
protected:
  bool eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::FocusIn ||
        event->type() == QEvent::FocusOut )
      emit focusInOut();
    return QObject::eventFilter(obj,event);
  }
signals:
  void focusInOut();
};



#endif // CTGUIADDITIONALCLASSES
