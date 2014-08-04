#ifndef WRITEERRORDIALOG_H
#define WRITEERRORDIALOG_H

#include <QDialog>

namespace Ui {
class writeErrorDialog;
}

class writeErrorDialog : public QDialog
{
  Q_OBJECT

public:
  explicit writeErrorDialog(QWidget *parent = 0);
  ~writeErrorDialog();

private:
  Ui::writeErrorDialog *ui;
};

#endif // WRITEERRORDIALOG_H
