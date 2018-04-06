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

  connect( ui->nof, SIGNAL(valueChanged(int)), SLOT(updateNoF()) );
  connect( ui->list, SIGNAL(itemChanged(QTableWidgetItem*)), SLOT(updateAmOK()));
  connect( ui->irregular, SIGNAL(toggled(bool)), SLOT(updateAmOK()));
  connect( ui->step, SIGNAL(valueChanged(double)), SLOT(updateAmOK()));

  connect( ui->nof, SIGNAL(valueChanged(int)), SIGNAL(parameterChanged()));
  connect( ui->step, SIGNAL(valueChanged(double)), SIGNAL(parameterChanged()));
  connect( ui->irregular, SIGNAL(toggled(bool)), SIGNAL(parameterChanged()));
  connect( ui->list, SIGNAL(itemChanged(QTableWidgetItem*)), SIGNAL(parameterChanged()));

  connect( ui->irregular, SIGNAL(toggled(bool)), ui->step, SLOT(setDisabled(bool)));

  ui->list->setItemDelegateForColumn(0, new NTableDelegate(ui->list));
  ui->list->horizontalHeader()->setStretchLastSection(false);
  ui->list->horizontalHeader()->setResizeMode(0, QHeaderView::Stretch);
  ui->list->horizontalHeader()->setResizeMode(1, QHeaderView::Fixed);

  updateNoF();

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

  connect( mot, SIGNAL(changedConnected(bool)),      SLOT(updateNoF()));
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


void PositionList::updateNoF() {


  const int steps = ui->nof->value();

  while ( steps < ui->list->rowCount() ) {

    delete ui->list->cellWidget( ui->list->rowCount()-1, 1 );
    ui->list->removeRow( ui->list->rowCount()-1 );

  } while ( steps > ui->list->rowCount() ) {

    const int crc = ui->list->rowCount();
    ui->list->insertRow(crc);

    QToolButton * btn = new QToolButton(this);
    connect(btn, SIGNAL(clicked(bool)), SLOT(moveMotorHere()));
    btn->setText("...");
    btn->setToolTip("Move motor here");
    ui->list->setCellWidget(crc, 1, btn );

    const double mpos = motui ? motui->motor()->getUserPosition() :  0 ;
    ui->list->setItem(crc, 0, new QTableWidgetItem( QString::number(mpos) ) );
    ui->list->setVerticalHeaderItem(crc, new QTableWidgetItem(QString::number(crc)) );

  }

  updateAmOK();

}

void PositionList::updateAmOK() {

  if (!motui)
    return;
  QCaMotor * mot = motui->motor();

  const int steps = ui->list->rowCount();

  bool newAllOK=true;
  for ( int crow=0 ; crow < steps ; crow++ ) {

    QTableWidgetItem * item = ui->list->item(crow, 0);

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

    ui->list->cellWidget(crow, 1)->setEnabled( ! mot->isMoving() );

    bool isDouble;
    const double pos = item->text().toDouble(&isDouble);
    const bool isOK = ! mot->isConnected() ||
        ( isDouble &&  pos > mot->getUserLoLimit()  && pos < mot->getUserHiLimit() );
    item->setBackground( isOK ? QBrush() : QBrush(QColor(Qt::red)));
    newAllOK &= isOK;

  }


  static const QString warnStyle = "background-color: rgba(255, 0, 0, 128);";

  ui->list->setColumnHidden(1, ! ui->irregular->isChecked() );
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



void PositionList::emphasizeRow(int row) {
  if ( row < 0  ||  row >= ui->list->rowCount() )
    ui->list->setCurrentItem(0);
  else
    ui->list->setCurrentCell(row, 0);
}




void PositionList::moveMotorHere() {
  for ( int crow=0 ; crow<ui->list->rowCount() ; crow++ )
    if ( sender() == ui->list->cellWidget(crow, 1) ) {
      motui->motor()->goUserPosition( ui->list->item(crow, 0)->text().toDouble(), QCaMotor::STARTED );
      return;
    }
}
