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
#include <QStyledItemDelegate>

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



namespace Ui {
class Script;
}

class Script : public QWidget {
  Q_OBJECT;

private:
  Ui::Script *ui;
  QProcess proc;
  QTemporaryFile fileExec;

public:
  explicit Script(QWidget *parent = 0);
  ~Script();

  void setPath(const QString & _path);
  const QString out() {return proc.readAllStandardOutput();}
  const QString err() {return proc.readAllStandardError();}
  int waitStop();
  bool isRunning() const { return proc.pid(); };
  const QString path() const;

  void addToColumnResizer(ColumnResizer * columnizer);

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


class MotorPosition : public QWidget {
  Q_OBJECT;

public:

  explicit MotorPosition(QWidget *parent = 0);


};



// to be used for the doble numbers in the list of positions
// in the irregular step serial scans.

class NTableDelegate : public QStyledItemDelegate {
  Q_OBJECT;
public:
  NTableDelegate(QObject* parent);
  QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;
  void setEditorData(QWidget *editor, const QModelIndex &index) const;
  void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const;
  void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const;
};

// QTableWidget with support for copy and paste added
// Here copy and paste can copy/paste the entire grid of cells
class QTableWidgetWithCopyPaste : public QTableWidget
{
public:
  QTableWidgetWithCopyPaste(int rows, int columns, QWidget *parent) :
      QTableWidget(rows, columns, parent)
  {};

  QTableWidgetWithCopyPaste(QWidget *parent) :
    QTableWidget(parent)
  {};

private:
  void copy();
  void paste();

protected:
  virtual void keyPressEvent(QKeyEvent * event);
};



#endif // CTGUIADDITIONALCLASSES
