#include "shutter.h"
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QStandardItemModel>
#include <unistd.h>


static const QString shuttersListName = "listOfKnownShutters.ini";

QHash<QString, QStringList> readListOfKnownShutters() {

  QHash<QString, QStringList> toReturn;


  foreach(QString pth, QStringList() << QStandardPaths::standardLocations(QStandardPaths::AppDataLocation)
                                     << QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation) ) {
    foreach(QString cfg, QDir(pth).entryList(QStringList() << shuttersListName, QDir::Files) ) {
      QSettings config(pth + QDir::separator() + cfg, QSettings::IniFormat);
      if ( ! config.status() )
        foreach (const QString shuttername, config.childGroups()) {
          config.beginGroup(shuttername);
          const QStringList shutterdesc = QStringList()
          << config.value("doOpenPC").toString()
          << config.value("doOpenVal").toString()
          << config.value("doClosePC").toString()
          << config.value("doCloseVal").toString()
          << config.value("isOpenPC").toString()
          << config.value("isOpenVal").toString()
          << config.value("isClosedPC").toString()
          << config.value("isClosedVal").toString();
          config.endGroup();
          toReturn[shuttername] = shutterdesc;
        }
    }
  }

  return toReturn;

}

const QHash<QString, QStringList> Shutter::listOfKnownShutters = readListOfKnownShutters();
const QStringList Shutter::fakeShutter = QStringList()
    << QString() << QString() << QString() << QString() << QString() << QString() << QString() << QString();

Shutter::Shutter(QWidget *parent)
  : QWidget(parent)
  , ui(new Ui::Shutter)
  , customUi(new Ui::UShutterConf)
  , customDlg(new QDialog(parent))
  , doOpen   ( new PVorCOM(this), QString() )
  , doClose  ( new PVorCOM(this), QString() )
  , isOpen   ( new PVorCOM(this), QString() )
  , isClosed ( new PVorCOM(this), QString() )
{

  ui->setupUi(this);
  ui->selection->insertItems(0, listOfKnownShutters.keys());
  ui->selection->insertItem(0, "none"); // must be at the top (assumption for the onSelection())

  customUi->setupUi(customDlg);
  customUi->doCls->setPlaceholderText("as above");
  customUi->isCls->setPlaceholderText("as above");
  customUi->loadPreset->insertItems(1, listOfKnownShutters.keys());

  ColumnResizer * resizer = new ColumnResizer(customDlg);
  resizer->addWidgetsFromGridLayout( dynamic_cast<QGridLayout*>(customUi->doOpn->layout()), 1);
  resizer->addWidgetsFromGridLayout( dynamic_cast<QGridLayout*>(customUi->doCls->layout()), 1);
  resizer->addWidgetsFromGridLayout( dynamic_cast<QGridLayout*>(customUi->isOpn->layout()), 1);
  resizer->addWidgetsFromGridLayout( dynamic_cast<QGridLayout*>(customUi->isCls->layout()), 1);

  connect(customUi->loadPreset, SIGNAL(activated(int)), SLOT(onLoadPreset()));
  connect(ui->selection, SIGNAL(activated(int)), SLOT(onSelection()));
  connect(ui->toggle, SIGNAL(pressed()), SLOT(toggle()));
  connect(ui->status, SIGNAL(pressed()), SLOT(requestUpdate()));
  connect(isOpen.first, SIGNAL(valueUpdated(QString)), SLOT(onOpenUpdate(QString)));
  connect(isClosed.first, SIGNAL(valueUpdated(QString)), SLOT(onClosedUpdate(QString)));

}

Shutter::~Shutter()
{
  delete ui;
}



void Shutter::requestUpdate() {
  if (amFake) return;
  isOpen.first->get();
  if ( ! isClosed.second.isEmpty()  &&  isClosed.first == isOpen.first )
    isClosed.first->get();
}

Shutter::State Shutter::state() const {
  bool opn ( lastOpen == isOpen.second ) ,
       cls ( isClosed.first == isOpen.first && isClosed.second.isEmpty()
             ?  ! opn  :  lastClosed == isClosed.second );
  if      ( opn && ! cls) return OPEN;
  else if ( ! opn && cls) return CLOSED;
  else                    return BETWEEN;
}



void Shutter::onStatusUpdate() {
  const State st = state();
  switch (st) {
  case OPEN:
    ui->status->setText("Open");
    emit opened();
    break;
  case CLOSED:
    ui->status->setText("Closed");
    emit closed();
    break;
  case BETWEEN:
    ui->status->setText("Moving");
    break;
  }
  emit stateUpdated(st);
}


void Shutter::waitForState(State st) {
  if (amFake) return;
  requestUpdate();
  if (state() != st)
    qtWait(this, SIGNAL(stateUpdated(State)), 2000);
  if (state() != st) { // do it one more time 
    requestUpdate();
    qtWait(this, SIGNAL(stateUpdated(State)), 2000);
  }
}

void Shutter::open(bool wait) {
  doOpen.first->put(doOpen.second);
  if (amFake) return;
  usleep(100000); 
  if (wait && state() != OPEN)
    waitForState(OPEN);
}

void Shutter::close(bool wait) {
  doClose.first->put(doClose.second);
  if (amFake) return;
  usleep(100000);
  if (wait && state() != CLOSED)    
    waitForState(CLOSED);
}


QStringList Shutter::shutterConfiguration() const {
  return QStringList()
      << doOpen.first->getName() << doOpen.second
      << doClose.first->getName() << doClose.second
      << isOpen.first->getName() << isOpen.second
      << isClosed.first->getName() << isClosed.second;
}


const QStringList Shutter::readCustomDialog() const {
  return QStringList()
      << customUi->doOpn->pvc->getName() << customUi->doOpnVal->text()
      << customUi->doCls->pvc->getName() << customUi->doClsVal->text()
      << customUi->isOpn->pvc->getName() << customUi->isOpnVal->text()
      << customUi->isCls->pvc->getName() << customUi->isClsVal->text();
}

void Shutter::loadCustomDialog(const QStringList & desc) {
  if (desc.size() != 8) {
    qDebug() << "Wrong shutter description" << desc;
    return;
  }
  customUi->doOpn->pvc->setName(desc[0]);
  customUi->doOpnVal->setText(desc[1]);
  customUi->doCls->pvc->setName(desc[2]);
  customUi->doClsVal->setText(desc[3]);
  customUi->isOpn->pvc->setName(desc[4]);
  customUi->isOpnVal->setText(desc[5]);
  customUi->isCls->pvc->setName(desc[6]);
  customUi->isClsVal->setText(desc[7]);
}

void Shutter::onLoadPreset() {
  loadCustomDialog( customUi->loadPreset->currentIndex() ?
                    listOfKnownShutters[customUi->loadPreset->currentText()] : fakeShutter);
}


void Shutter::onSelection(){
  if ( ui->selection->currentIndex() == 0 ) // fake shutter
    setShutter(fakeShutter);
  else if ( ui->selection->currentIndex() == ui->selection->count()-1 )  {//custom
    QStringList currentCustom = readCustomDialog();
    if (customDlg->exec() == QDialog::Accepted)
      currentCustom = readCustomDialog();
    else
      loadCustomDialog(currentCustom);
    setShutter(currentCustom);
  } else {
    setShutter(ui->selection->currentText());
  }
}


void Shutter::setShutter(const QStringList & desc) {

  if (desc.size() != 8) {
    qDebug() << "Wrong shutter description" << desc;
    return;
  }
  amFake = std::addressof(desc) == std::addressof(fakeShutter);

  ui->status->setText("status");

  doOpen.first->setName(desc[0]);
  doOpen.second = desc[1];
  doClose.first->setName( desc[2].isEmpty() ? desc[0] : desc[2] );
  doClose.second = desc[3];
  isOpen.first->setName(desc[4]);
  isOpen.second = desc[5];
  isClosed.first->setName(desc[6].isEmpty() ? desc[4] : desc[6]);
  isClosed.second = desc[7];

  emit shutterChanged();

}


void Shutter::setShutter(const QString & shutterName) {
  const QString shn = shutterName.isEmpty()
      ? ui->selection->currentText() : shutterName;
  if ( knownShutters().contains(shn) )
      setShutter(listOfKnownShutters[shn]);
}

