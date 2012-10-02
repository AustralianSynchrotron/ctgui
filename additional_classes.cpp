#include "additional_classes.h"
#include "ui_script.h"
#include <QDebug>
#include <QFileDialog>





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






Script::Script(QWidget *parent) :
  QWidget(parent),
  ui(new Ui::Script)
{
  ui->setupUi(this);
  connect(ui->path, SIGNAL(textChanged(QString)), SLOT(evaluate()));
  connect(ui->browse, SIGNAL(clicked()), SLOT(browse()));
  connect(ui->execute, SIGNAL(clicked()), SLOT(onStartStop()));
  connect(ui->path, SIGNAL(editingFinished()), SIGNAL(editingFinished()));
  connect(&proc, SIGNAL(stateChanged(QProcess::ProcessState)),
          SLOT(onState(QProcess::ProcessState)));
}


Script::~Script() {
  delete ui;
}

void Script::setPath(const QString & _path) {
  ui->path->setText(_path);
}

const QString Script::path() const {
  return ui->path->text();
}

void Script::browse() {
  ui->path->setText(
        QFileDialog::getOpenFileName(0, "Command", QDir::currentPath()) );
}


void Script::evaluate() {
  ui->execute->setStyleSheet("");
  QProcess tmpproc;
  tmpproc.start("bash -c \"command -v " + ui->path->text() + "\"");
  tmpproc.waitForFinished();
  ui->path->setStyleSheet( tmpproc.readAll().isEmpty() ? "color: rgb(255, 0, 0);" : "");
}


bool Script::start() {
  if (isRunning())
    return false;
  proc.start("bash -c \"" + ui->path->text() + "\"");
  return isRunning();
}

int Script::waitStop() {
  QEventLoop q;
  connect(&proc, SIGNAL(finished(int)), &q, SLOT(quit()));
  if (isRunning())
    q.exec();
  return proc.exitCode();
}

void Script::onState(QProcess::ProcessState state) {

  ui->browse->setEnabled( state==QProcess::NotRunning );
  ui->path->setEnabled( state==QProcess::NotRunning );
  ui->execute->setText( state==QProcess::NotRunning ?
                         "Execute" : "Stop" );

  if (state==QProcess::NotRunning) {
    ui->execute->setStyleSheet( proc.exitCode() ? "color: rgb(255, 0, 0);" : "");
    emit finished(proc.exitCode());
  } else {
    emit started();
  }

};


