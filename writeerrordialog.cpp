#include "writeerrordialog.h"
#include "ui_writeerrordialog.h"

writeErrorDialog::writeErrorDialog(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::writeErrorDialog)
{
  ui->setupUi(this);
}

writeErrorDialog::~writeErrorDialog()
{
  delete ui;
}
