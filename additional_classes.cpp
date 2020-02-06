#include "additional_classes.h"
#include "ui_script.h"
#include <QDebug>
#include <QFileDialog>
#include <QTimer>
#include <QClipboard>
#include <QMessageBox>

QSCheckBox::QSCheckBox(QWidget * parent) :
  QCheckBox(parent) {}



bool QSCheckBox::hitButton ( const QPoint & pos ) const  {
  QStyleOptionButton opt;
  initStyleOption(&opt);
  return style()
      ->subElementRect(QStyle::SE_CheckBoxIndicator, &opt, this)
      .contains(pos);
}







typedef QPair<QGridLayout*, int> GridColumnInfo;

class ColumnResizerPrivate {
public:
  ColumnResizerPrivate(ColumnResizer* q_ptr)
    : q(q_ptr)
    , m_updateTimer(new QTimer(q))
  {
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(0);
    QObject::connect(m_updateTimer, SIGNAL(timeout()), q, SLOT(updateWidth()));
  }

  void scheduleWidthUpdate()
  {
    m_updateTimer->start();
  }

  ColumnResizer* q;
  QTimer* m_updateTimer;
  QList<QWidget*> m_widgets;
  QList<GridColumnInfo> m_gridColumnInfoList;
};

ColumnResizer::ColumnResizer(QObject* parent)
  : QObject(parent)
  , d(new ColumnResizerPrivate(this))
{}

ColumnResizer::~ColumnResizer()
{
  delete d;
}

void ColumnResizer::addWidget(QWidget* widget) {
  d->m_widgets.append(widget);
  widget->installEventFilter(this);
  d->scheduleWidthUpdate();
}

void ColumnResizer::updateWidth() {
  int width = 0;
  foreach (QWidget* widget, d->m_widgets)
    width = qMax(widget->sizeHint().width(), width);
  foreach (GridColumnInfo info, d->m_gridColumnInfoList)
    info.first->setColumnMinimumWidth(info.second, width);
}

bool ColumnResizer::eventFilter(QObject*, QEvent* event) {
  if (event->type() == QEvent::Resize) {
    d->scheduleWidthUpdate();
  }
  return false;
}


void ColumnResizer::addWidgetsFromGridLayout(QGridLayout* layout, int column) {
  if (!layout) {
    qDebug() << "no layout to resize";
    return;
  }
  for (int row = 0; row < layout->rowCount(); ++row) {
    QLayoutItem* item = layout->itemAtPosition(row, column);
    if (!item) {
      continue;
    }
    QWidget* widget = item->widget();
    if (!widget) {
      continue;
    }
    addWidget(widget);
  }
  d->m_gridColumnInfoList << GridColumnInfo(layout, column);
}







bool  QPTEext::event(QEvent *event) {
  if (event->type() == QEvent::FocusOut)
    emit editingFinished();
  return QPlainTextEdit::event(event);
}

void  QPTEext::focusInEvent ( QFocusEvent * e ) {
  QPlainTextEdit::focusInEvent(e);
  emit focusIned();
}

void  QPTEext::focusOutEvent ( QFocusEvent * e ) {
  QPlainTextEdit::focusOutEvent(e);
  emit focusOuted();
}



CtGuiLineEdit::CtGuiLineEdit(QWidget *parent)
  : QLineEdit(parent)
{
  clearButton = new QToolButton(this);
  clearButton->setToolTip("Clear text");
  clearButton->setArrowType(Qt::LeftArrow);
  clearButton->setCursor(Qt::ArrowCursor);
  clearButton->setStyleSheet("QToolButton { border: none; padding: 0px; }");
  clearButton->hide();
  connect(clearButton, SIGNAL(clicked()), this, SLOT(clear()));
  connect(this, SIGNAL(textChanged(const QString&)), this, SLOT(updateCloseButton(const QString&)));
  int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
  setStyleSheet(QString("QLineEdit { padding-right: %1px; } ").arg(clearButton->sizeHint().width() + frameWidth + 1));
  QSize msz = minimumSizeHint();
  setMinimumSize(qMax(msz.width(), clearButton->sizeHint().height() + frameWidth * 2 + 2),
                 qMax(msz.height(), clearButton->sizeHint().height() + frameWidth * 2 + 2));
}

void CtGuiLineEdit::resizeEvent(QResizeEvent *) {
  QSize sz = clearButton->sizeHint();
  int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
  clearButton->move(rect().right() - frameWidth - sz.width(),
                    (rect().bottom() + 1 - sz.height())/2);
}









void EasyTabWidget::finilize() {
  for (int idx=0; idx<count();idx++) {
    wdgs << widget(idx);
    titles[wdgs[idx]] = tabText(idx);
  }
}

void EasyTabWidget::setTabVisible(QWidget * tab, bool visible) {

  const int
      idxL=wdgs.indexOf(tab),
      idxT=indexOf(tab);

  if ( idxL<0 || // does not exist in the list
       (idxT>=0) == visible ) // in the requested state
    return;

  if (visible) {
    int nidx=-1;
    int pidxL=idxL-1;
    while ( pidxL>=0 && nidx==-1 ) {
      int pidxT = indexOf(wdgs.at(pidxL));
      if (pidxT>=0)
        nidx=insertTab(pidxT+1, tab, titles[tab]) ;
      pidxL--;
    }
    if (nidx<0)
      insertTab(0,tab, titles[tab]);
  } else {
    removeTab(idxT);
  }

}

void EasyTabWidget::setTabTextColor(QWidget * tab, const QColor & color) {
  tabBar()->setTabTextColor(indexOf(tab), color);
}








CTprogressBar::CTprogressBar(QWidget * parent) :
  QProgressBar(parent),
  label (new QLabel(this))
{
  label->setAlignment(Qt::AlignCenter);
  label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
  label->raise();
}

void CTprogressBar::updateLabel() {
  label->setVisible( ! minimum() && ! maximum() && isTextVisible() );
}


void CTprogressBar::setTextVisible(bool visible) {
  QProgressBar::setTextVisible(visible);
  updateLabel();
}

void CTprogressBar::setValue(int value) {
  QProgressBar::setValue(value);
  if ( ! minimum() && ! maximum() && isTextVisible() ) {
    QString fmt = format();
    label->setText(fmt.replace("%v", QString::number(value)));
  }
}

void CTprogressBar::setMinimum(int minimum) {
  QProgressBar::setMinimum(minimum);
  updateLabel();
}

void CTprogressBar::setMaximum(int maximum) {
  QProgressBar::setMaximum(maximum);
  updateLabel();
}


void CTprogressBar::resizeEvent(QResizeEvent * event) {
  QProgressBar::resizeEvent(event);
  label->setMaximumSize(size());
  label->setMinimumSize(size());
}














const QString Script::shell = getenv("SHELL") ? getenv("SHELL") : "/bin/sh";

Script::Script(QObject *parent) :
  QObject(parent),
  proc(this),
  fileExec(),
  _path()
{
  connect(&proc, SIGNAL(stateChanged(QProcess::ProcessState)),
          SLOT(onState(QProcess::ProcessState)));
  if ( ! fileExec.open() )
    qDebug() << "ERROR! Unable to open temporary file.";
}


void Script::onState(QProcess::ProcessState state) {
  if (state==QProcess::NotRunning)
    emit finished(proc.exitCode());
  else if (state==QProcess::Running)
    emit started();
}

bool Script::start(const QString & par) {
  if ( ! fileExec.isOpen() || isRunning() )
    return false;
  if (path().isEmpty())
    return true;
  proc.start(shell + " " + fileExec.fileName() + " " + par );
  return isRunning();
}

int Script::waitStop() {
  QEventLoop q;
  connect(&proc, SIGNAL(finished(int)), &q, SLOT(quit()));
  if (isRunning())
    q.exec();
  return proc.exitCode();
}


const QString & Script::setPath(const QString &_p) {
  if ( ! fileExec.isOpen() || isRunning() )
    return path();
  fileExec.resize(0);
  fileExec.write( _p.toLatin1() );
  fileExec.write( " $@\n" );
  fileExec.flush();
  _path=_p;
  emit pathSet(path());
  return path();
}


int Script::evaluate(const QString & par) {
  QProcess tempproc;
  tempproc.start(shell + " -n " + fileExec.fileName() + " " + par);
  tempproc.waitForFinished();
  return tempproc.exitCode();
}


namespace Ui {
class UScript;
}


UScript::UScript(QWidget *parent) :
  QWidget(parent),
  ui(new Ui::UScript),
  script (new Script(this))
{

  ui->setupUi(this);

  connect(ui->path, SIGNAL(textChanged(QString)), script, SLOT(setPath(QString)));
  connect(ui->browse, SIGNAL(clicked()), SLOT(browse()));
  connect(ui->execute, SIGNAL(clicked()), SLOT(onStartStop()));
  connect(ui->path, SIGNAL(editingFinished()), SIGNAL(editingFinished()));

  connect(script, SIGNAL(started()), SLOT(updateState()));
  connect(script, SIGNAL(finished(int)), SLOT(updateState()));
  connect(script, SIGNAL(pathSet(QString)), SLOT(updatePath()));

}


UScript::~UScript() {
  delete ui;
}


void UScript::browse() {
  ui->path->setText(
        QFileDialog::getOpenFileName(0, "Command", QDir::currentPath()) );
}

void UScript::addToColumnResizer(ColumnResizer * columnizer) {
  if ( ! columnizer )
    return;
  columnizer->addWidgetsFromGridLayout(ui->gridLayout, 1);
  columnizer->addWidgetsFromGridLayout(ui->gridLayout, 2);
}



void UScript::updateState() {
  ui->browse->setEnabled( ! script->isRunning() );
  ui->path->setEnabled( ! script->isRunning() );
  ui->execute->setText( script->isRunning() ? "Stop" : "Execute" );
  ui->execute->setStyleSheet( ! script->isRunning() && script->exitCode()
                              ? "color: rgb(255, 0, 0);" : "");
}

void UScript::updatePath() {
  ui->path->setText(script->path());
  ui->execute->setStyleSheet("");
  ui->path->setStyleSheet( script->evaluate() ? "color: rgb(255, 0, 0);" : "");
}




PVorCOM::PVorCOM(QObject *parent)
  : QObject(parent)
  , pv(new QEpicsPv(this))
  , sr(new Script(this))
{
  setName();
  connect(pv, SIGNAL(valueUpdated(QVariant)), SLOT(updateVal()));
  connect(sr, SIGNAL(finished(int)), SLOT(updateVal()));
  connect(pv, SIGNAL(connectionChanged(bool)), SLOT(updateState()));
  connect(sr, SIGNAL(started()), SLOT(updateState()));
  connect(sr, SIGNAL(finished(int)), SLOT(updateState()));

}


void PVorCOM::setName( const QString & nm) {
  pv->setPV(nm);
  sr->setPath(nm);
  emit nameUpdated(nm);
}


void PVorCOM::put( const QString & val) {
  if ( ! pv->isConnected()) sr->execute(val);
  else if ( ! val.isEmpty() ) pv->set(val);
}

PVorCOM::WhoAmI PVorCOM::state() {
  if (pv->isConnected())
    return EPICSPV;
  if (sr->isRunning())
    return RUNNINGSCRIPT;
  if (sr->exitCode())
    return BADSCRIPT;
  return GOODSCRIPT;
}



UPVorCOM::UPVorCOM (QWidget *parent)
  : QWidget(parent)
  , ui(new Ui::UPVorCOM)
  , pvc (new PVorCOM(this))
{
  ui->setupUi(this);
  connect(ui->name, SIGNAL(editingFinished()), SLOT(setPVCname()));
  connect(ui->name, SIGNAL(textChanged(QString)), SLOT(evaluateScript()));
  connect(ui->val, SIGNAL(clicked()), pvc, SLOT(put()));
  connect(pvc, SIGNAL(valueUpdated(QString)), SLOT(setValueText(QString)));
  connect(pvc, SIGNAL(nameUpdated(QString)), ui->name, SLOT(setText(QString)));
  connect(pvc, SIGNAL(stateUpdated(PVorCOM::WhoAmI)), SLOT(indicateState(PVorCOM::WhoAmI)));
}

void UPVorCOM::setValueText(const QString & txt) {
  ui->val->setToolTip(txt);
  QString btxt=txt.trimmed();
  btxt = btxt.left( qMin(10, btxt.indexOf(QRegExp("[\a\f\r\n\t]"))));
  if ( btxt.size() < txt.size() - 1 ) // -1 here to allow last \n
    btxt += " ...";
  ui->val->setText(btxt);
}


void UPVorCOM::indicateState(PVorCOM::WhoAmI state) {
  ui->val->setEnabled( state == PVorCOM::GOODSCRIPT || state == PVorCOM::BADSCRIPT );
  ui->val->setStyleSheet( state == PVorCOM::BADSCRIPT ? "color: rgb(255, 0, 0);" : "" );
}

void UPVorCOM::evaluateScript() {

}



