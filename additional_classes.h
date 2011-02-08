#include <QPlainTextEdit>
#include <QDebug>
#include <QLineEdit>
#include <QToolButton>
#include <QStyle>

#ifndef QPTEEXT_H
#define QPTEEXT_H



class QPTEext : public QPlainTextEdit {

  Q_OBJECT;

public:

  QPTEext(QWidget * parent=0) : QPlainTextEdit(parent) {};

signals:

  void editingFinished();
  void focusIned();
  void focusOuted();

protected:

  bool event(QEvent *event) {
    if (event->type() == QEvent::FocusOut)
      emit editingFinished();
    return QPlainTextEdit::event(event);
  }

  void focusInEvent ( QFocusEvent * e ) {
    QPlainTextEdit::focusInEvent(e);
    emit focusIned();
  }

  void focusOutEvent ( QFocusEvent * e ) {
    QPlainTextEdit::focusOutEvent(e);
    emit focusOuted();
  }


};


class CtGuiLineEdit : public QLineEdit
{
    Q_OBJECT;

private:
  QToolButton *clearButton;

public:
  CtGuiLineEdit(QWidget *parent)
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

protected:
  void resizeEvent(QResizeEvent *) {
    QSize sz = clearButton->sizeHint();
    int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
    clearButton->move(rect().right() - frameWidth - sz.width(),
                      (rect().bottom() + 1 - sz.height())/2);
  }

private slots:
  void updateCloseButton(const QString& text) {
    clearButton->setVisible(!text.isEmpty());
  }

};



#endif // QPTEEXT_H
