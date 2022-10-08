#include "positionlist.h"
#include "ui_positionlist.h"
#include <QClipboard>
#include <QPainter>





NTableDelegate::NTableDelegate(QObject *parent) : QStyledItemDelegate(parent)
{
}

QWidget* NTableDelegate::createEditor(QWidget* parent,const QStyleOptionViewItem &option,const QModelIndex &index) const
{
  QLineEdit* editor = new QLineEdit(parent);
  QDoubleValidator* val = new QDoubleValidator(editor);
  val->setNotation(QDoubleValidator::StandardNotation);
  editor->setValidator(val);
  return editor;
}

void NTableDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
  double value = index.model()->data(index,Qt::EditRole).toDouble();
  QLineEdit* line = static_cast<QLineEdit*>(editor);
  line->setText(QString().setNum(value));
}

void NTableDelegate::setModelData(QWidget* editor,QAbstractItemModel* model,const QModelIndex &index) const
{
  QLineEdit* line = static_cast<QLineEdit*>(editor);
  QString value = line->text();
  model->setData(index,value);
}

void NTableDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
  editor->setGeometry(option.rect);
}






/*
class PListHeader : public QHeaderView {
public:
  using QHeaderView::QHeaderView;

protected:
  void paintSection(QPainter* painter, const QRect &rect, int logicalIndex) const override {
    painter->save();
    QHeaderView::paintSection(painter, rect, logicalIndex);
    painter->restore();
    if ( ! model() || logicalIndex != 3)
      return;

    QStyleOptionButton option;
    option.init(this);

    QRect checkbox_rect = style()->subElementRect(QStyle::SubElement::SE_CheckBoxIndicator, &option, this);
    checkbox_rect.moveRight(rect.right());

    bool checked = model()->headerData(logicalIndex, orientation(), Qt::CheckStateRole).toBool();

    //option.text=QString("Do");
    option.rect = checkbox_rect;
    option.state = checked ? QStyle::State_On : QStyle::State_Off;
    option.state = QStyle::State_Off;

    style()->drawPrimitive(QStyle::PE_IndicatorCheckBox, &option, painter);
  }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        QHeaderView::mouseReleaseEvent(event);
        if(model())
        {
            int section = logicalIndexAt(event->pos());
            if (section >= 0)
            {
                bool checked = model()->headerData(section, orientation(), Qt::CheckStateRole).toBool();
                model()->setHeaderData(section, orientation(), !checked, Qt::CheckStateRole);
                viewport()->update();
            }
        }
    }

};
*/








void QTableWidgetWithCopyPaste::copy() {
  QString selected_text;
  foreach ( QTableWidgetItem * item , selectedItems() )
    selected_text += item->text() + '\n';
  qApp->clipboard()->setText(selected_text);
}

void QTableWidgetWithCopyPaste::paste() {

  QString selected_text = qApp->clipboard()->text();
  QStringList cells = selected_text.split(QRegExp(QLatin1String("\\n|\\t| ")));
  if ( cells.empty() )
    return;
  int ccell=0;
  foreach ( QTableWidgetItem * item , selectedItems() )
    item->setText( ccell < cells.size() ? cells.at(ccell++) : "");

}

void QTableWidgetWithCopyPaste::keyPressEvent(QKeyEvent * event)
{
  if(event->matches(QKeySequence::Copy) )
    copy();
  else if(event->matches(QKeySequence::Paste) )
    paste();
  else
    QTableWidget::keyPressEvent(event);

}








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
  connect( ui->list->horizontalHeader(), SIGNAL(sectionClicked(int)), SLOT(updateToDo(int)));
  connect( ui->irregular, SIGNAL(toggled(bool)), ui->step, SLOT(setDisabled(bool)));

  ui->list->setItemDelegateForColumn(0, new NTableDelegate(ui->list));
  QHeaderView * header = ui->list->horizontalHeader();
  //PListHeader * header = new PListHeader(Qt::Horizontal, ui->list);
  //ui->list->setHorizontalHeader(header);
  header->setStretchLastSection(false);
  #if QT_VERSION >= 0x050000
  header->setSectionResizeMode(0, QHeaderView::Stretch);
  header->setSectionResizeMode(1, QHeaderView::Fixed);
  header->setSectionResizeMode(2, QHeaderView::Fixed);
  header->setSectionResizeMode(3, QHeaderView::Fixed);
  #else
  header->setResizeMode(0, QHeaderView::Stretch);
  header->setResizeMode(1, QHeaderView::Fixed);
  header->setResizeMode(2, QHeaderView::Fixed);
  header->setResizeMode(3, QHeaderView::Fixed);
  #endif

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
    delete ui->list->cellWidget( ui->list->rowCount()-1, 2 );
    ui->list->removeRow( ui->list->rowCount()-1 );
  }

  while ( steps > ui->list->rowCount() ) {

    const int crc = ui->list->rowCount();
    ui->list->insertRow(crc);

    QToolButton * btn;

    btn = new QToolButton(this);
    connect(btn, SIGNAL(clicked(bool)), SLOT(moveMotorHere()));
    btn->setText("...");
    btn->setToolTip("Move motor here");
    ui->list->setCellWidget(crc, 1, btn );
    ui->list->resizeColumnToContents(1);

    btn = new QToolButton(this);
    connect(btn, SIGNAL(clicked(bool)), SLOT(getMotorPosition()));
    btn->setText("...");
    btn->setToolTip("Get current motor position");
    ui->list->setCellWidget(crc, 2, btn );
    ui->list->resizeColumnToContents(2);

    QSCheckBox * dome = new QSCheckBox(this);
    dome->setToolTip("If not ticked, will skip this scan in the experiment.");
    dome->setStyleSheet( "text-align: center; margin-left:50%; margin-right:50%;" );
    ui->list->setCellWidget(crc, doMeCol, dome );
    ui->list->resizeColumnToContents(doMeCol);

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
  const bool isRegular = ! ui->irregular->isChecked();

  bool newAllOK=true;
  for ( int crow=0 ; crow < steps ; crow++ ) {

    QTableWidgetItem * item = ui->list->item(crow, 0);

    ui->list->blockSignals(true);
    if ( ! isRegular )
      item->setFlags( item->flags() | Qt::ItemIsEditable );
    else
      item->setFlags( item->flags() & ~Qt::ItemIsEditable );
    ui->list->blockSignals(false);

    if ( isRegular && ! freezListUpdates
         && mot->isConnected() && ! mot->isMoving() ) {
      double pos = mot->getUserPosition() + item->row() * ui->step->value();
      if ( pos != item->text().toDouble() )
        item->setText( QString::number(pos) );
    }

    ui->list->cellWidget(crow, 1)->setEnabled( ! mot->isMoving() );
    ui->list->cellWidget(crow, 2)->setEnabled( ! mot->isMoving() );

    bool isDouble;
    const double pos = item->text().toDouble(&isDouble);
    const bool isOK = ! mot->isConnected() ||
        ( isDouble &&  pos > mot->getUserLoLimit()  && pos < mot->getUserHiLimit() );
    item->setBackground( isOK ? QBrush() : QBrush(QColor(Qt::red)));
    newAllOK &= isOK;

  }


  static const QString warnStyle = "background-color: rgba(255, 0, 0, 128);";

  ui->list->setColumnHidden(1, isRegular );
  ui->list->setColumnHidden(2, isRegular );
  ui->irregular->setStyleSheet( newAllOK || isRegular ?  ""  : warnStyle  );

  bool isOK = ! isRegular || ! mot->isConnected() || ui->step->value() != 0.0;
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
      motui->motor()->goUserPosition( position(crow), QCaMotor::STARTED );
      return;
    }
}

void PositionList::getMotorPosition() {
  for ( int crow=0 ; crow<ui->list->rowCount() ; crow++ )
    if ( sender() == ui->list->cellWidget(crow, 2) ) {
      const double mpos = motui ? motui->motor()->getUserPosition() :  0 ;
      ui->list->item(crow, 0)->setText(QString::number(mpos));
    }
}


void PositionList::done(int row) {
  if (row<0)
    for ( int crow=0 ; crow<ui->list->rowCount() ; crow++ )
      done(crow);
  else if (row < ui->list->rowCount())
    ((QSCheckBox*)ui->list->cellWidget(row, doMeCol))->setChecked(false);
}


void PositionList::todo(int row) {
  if (row<0)
    for ( int crow=0 ; crow<ui->list->rowCount() ; crow++ )
      todo(crow);
  else if (row < ui->list->rowCount())
    ((QSCheckBox*)ui->list->cellWidget(row, doMeCol))->setChecked(true);
}



bool PositionList::doAny() const {
  for (int crow=0 ; crow < ui->list->rowCount() ; crow++)
    if (doMe(crow))
      return true;
  return false;
}

bool PositionList::doAll() const {
  for (int crow=0 ; crow < ui->list->rowCount() ; crow++)
    if (!doMe(crow))
      return false;
  return true;
}

int PositionList::nextToDo() const {
  for ( int crow=0 ; crow<ui->list->rowCount() ; crow++ )
    if (((QSCheckBox*)ui->list->cellWidget(crow, doMeCol))->isChecked() )
      return crow;
  return -1;
}


void PositionList::updateToDo(int index) {
  if (index!=3)
    return;
  const bool setState = ! doAll();
  for (int crow=0 ; crow < ui->list->rowCount() ; crow++)
    ((QSCheckBox*)ui->list->cellWidget(crow, doMeCol))->setChecked(setState);
}
