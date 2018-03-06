#include "positionlist.h"
#include "ui_positionlist.h"

PositionList::PositionList(QWidget *parent)
  : QWidget(parent)
  , allOK(false)
  , freezListUpdates(false)
  , ui(new Ui::PositionList)
  , motui(0)
{

  ui->setupUi(this);

  connect( ui->nof, SIGNAL(valueChanged(int)), SLOT(updateAmOK()));
  connect( ui->list, SIGNAL(itemChanged(QTableWidgetItem*)), SLOT(updateAmOK()));
  connect( ui->irregular, SIGNAL(toggled(bool)), SLOT(updateAmOK()));
  connect( ui->step, SIGNAL(valueChanged(double)), SLOT(updateAmOK()));

  connect( ui->nof, SIGNAL(valueChanged(int)), SIGNAL(parameterChanged()));
  connect( ui->step, SIGNAL(valueChanged(double)), SIGNAL(parameterChanged()));
  connect( ui->irregular, SIGNAL(toggled(bool)), SIGNAL(parameterChanged()));
  connect( ui->list, SIGNAL(itemChanged(QTableWidgetItem*)), SIGNAL(parameterChanged()));

  connect( ui->irregular, SIGNAL(toggled(bool)), ui->step, SLOT(setDisabled(bool)));

  ui->list->setItemDelegate(new NTableDelegate(ui->list));

}

PositionList::~PositionList() {
  delete ui;
}


void PositionList::putMotor(QCaMotorGUI * motor) { // assume is called only once !

  if (motui) // does not return on this error to make error visible
    qDebug() << "ERROR! Motor cannot be set more than once." ;

  motui=motor;
  QCaMotor * mot = motui->motor();
  ui->placeMotor->layout()->addWidget(motui->setupButton());
  ui->placePosition->layout()->addWidget(motui->currentPosition(true));

  connect( mot, SIGNAL(changedConnected(bool)),      SLOT(updateAmOK()));
  connect( mot, SIGNAL(changedPv(QString)),          SLOT(updateAmOK()));
  connect( mot, SIGNAL(changedUserLoLimit(double)),  SLOT(updateAmOK()));
  connect( mot, SIGNAL(changedUserHiLimit(double)),  SLOT(updateAmOK()));
  connect( mot, SIGNAL(changedLoLimitStatus(bool)),  SLOT(updateAmOK()));
  connect( mot, SIGNAL(changedHiLimitStatus(bool)),  SLOT(updateAmOK()));
  connect( mot, SIGNAL(changedMoving(bool)),         SLOT(updateAmOK()));

  connect( mot, SIGNAL(changedPv(QString)), SIGNAL(parameterChanged()));

  updateAmOK();

}



void PositionList::updateAmOK() {

  if (!motui)
    return;
  QCaMotor * mot = motui->motor();

  const int steps = ui->nof->value();

  while ( steps < ui->list->rowCount() )
    ui->list->removeRow( ui->list->rowCount()-1 );
  while ( steps > ui->list->rowCount() ) {
    const int crc = ui->list->rowCount();
    ui->list->insertRow(crc);
    ui->list->setItem(crc, 0, new QTableWidgetItem( QString::number(mot->getUserPosition() ) ) );
    ui->list->setVerticalHeaderItem(crc, new QTableWidgetItem(QString::number(crc)) );
  }

  bool newAllOK=true;
  foreach (QTableWidgetItem * item, ui->list->findItems("", Qt::MatchContains) ) {

    ui->list->blockSignals(true);
    if ( ui->irregular->isChecked() )
      item->setFlags( item->flags() | Qt::ItemIsEditable );
    else
      item->setFlags( item->flags() & ~Qt::ItemIsEditable );
    ui->list->blockSignals(false);

    if ( ! ui->irregular->isChecked() && ! freezListUpdates
         && mot->isConnected() && ! mot->isMoving() ) {
      double pos = mot->getUserPosition() + item->row() * ui->step->value();
      if ( pos != item->text().toDouble() )
        item->setText( QString::number(pos) );
    }

    bool isDouble;
    const double pos = item->text().toDouble(&isDouble);
    const bool isOK = ! mot->isConnected() ||
        ( isDouble &&  pos > mot->getUserLoLimit()  && pos < mot->getUserHiLimit() );
    item->setBackground( isOK ? QBrush() : QBrush(QColor(Qt::red)));
    newAllOK &= isOK;

  }

  static const QString warnStyle = "background-color: rgba(255, 0, 0, 128);";

  ui->irregular->setStyleSheet( newAllOK || ! ui->irregular->isChecked()
                                    ?  ""  : warnStyle  );

  bool isOK = ui->irregular->isChecked() || ! mot->isConnected() || ui->step->value() != 0.0;
  ui->step->setStyleSheet(isOK ?  ""  : warnStyle);
  newAllOK &= isOK;

  isOK = mot->getPv().isEmpty() || ( mot->isConnected() && ! mot->isMoving() );
  motui->setupButton()->setStyleSheet(isOK ?  ""  : warnStyle);
  newAllOK &= isOK;

  isOK = mot->getPv().isEmpty() || ! mot->getLimitStatus();
  motui->currentPosition(true)->setStyleSheet(isOK ?  ""  : warnStyle);
  newAllOK &= isOK;

  if (newAllOK != allOK)
    emit amOKchanged(allOK=newAllOK);

}

