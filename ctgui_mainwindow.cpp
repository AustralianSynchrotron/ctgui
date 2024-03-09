#include <QDebug>
#include <QFileDialog>
#include <QVector>
#include <QProcess>
#include <QtCore>
#include <QSettings>
#include <QFile>
#include <QTime>
#include <unistd.h>
#include <QApplication>
#include <QMessageBox>
#include <QStandardPaths>
#include <functional>

#include <poptmx.h>
#include "additional_classes.h"
#include "ctgui_mainwindow.h"
#include "ui_ctgui_mainwindow.h"

using namespace  std;


#define innearList qobject_cast<PositionList*> ( ui->innearListPlace->layout()->itemAt(0)->widget() )
#define outerList qobject_cast<PositionList*> ( ui->outerListPlace->layout()->itemAt(0)->widget() )
#define loopList qobject_cast<PositionList*> ( ui->loopListPlace->layout()->itemAt(0)->widget() )
#define sloopList qobject_cast<PositionList*> ( ui->sloopListPlace->layout()->itemAt(0)->widget() )


static const QString warnStyle = "background-color: rgba(255, 0, 0, 128);";
static const QString ssText = "Start experiment";

const QString MainWindow::storedState =
              QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/.ctgui";




class HWstate {

private:
  Detector * det;
  Shutter * shutSec;
  Shutter::State shutSecSate;
  Shutter * shutPri;
  Shutter::State shutPriSate;
  QString dettifname;
  QString dethdfname;
  bool detinrun;
  int detimode;
  int dettmode;
  float detperiod;

public:

  HWstate(Detector *_det, Shutter * _shutS, Shutter * _shutP)
    : det(_det), shutSec(_shutS), shutPri(_shutP)
  {
    store();
  }

  void store() {
    shutSecSate = shutSec->state();
    shutPriSate = shutPri->state();
    dettifname = det->name(Detector::TIF);
    dethdfname = det->name(Detector::HDF);
    detinrun = det->isAcquiring();
    detimode = det->imageMode();
    dettmode = det->triggerMode();
    detperiod = det->period();
  }

  void restore() {
    // shutSec->setState(shutSecState);
    // shutPri->setState(shutPriState);
    det->setName(Detector::TIF, dettifname) ;
    det->setName(Detector::HDF, dethdfname) ;
    if (detinrun) det->start(); else det->stop();
    det->setImageMode(detimode);
    det->setTriggerMode(dettmode);
    det->setPeriod(detperiod);
  }

};

static HWstate * preVideoState = 0;





QString configName(QObject * obj) {
  if (const Shutter* shut = qobject_cast<const Shutter*>(obj))
    return configName(shut->ui->selection);
  else if (QCaMotorGUI* mot = qobject_cast<QCaMotorGUI*>(obj))
    return configName(mot->setupButton());
  else if (const QTableWidgetOtem* wdgitm = qobject_cast<const QTableWidgetOtem*>(obj))
    return configName(wdgitm->tableWidget()) + "/" + wdgitm->property(configProp).toString();
  else if (const QButtonGroup* btns = qobject_cast<const QButtonGroup*>(obj)) {
    if (btns->buttons().size())
      return configName(btns->buttons().first()) + "/" + btns->property(configProp).toString();
  } else if  (QWidget * wdg = qobject_cast<QWidget*>(obj)) {
    QStringList toRet;
    QObject * curO = wdg;
    while (curO) {
      if (curO->property(configProp).isValid())
        toRet.push_front(curO->property(configProp).toString());
      curO = curO->parent();
    }
    return toRet.join('/');
  }
  return QString();
};



QString obj2str (QObject* obj, bool clean=true) {
  auto quoteNonEmpty = [&clean](const QString & str){ return clean || str.isEmpty() ? str : "'" + str + "'"; };
  auto specValue = [&clean](variant<QSpinBox*, QDoubleSpinBox*, QTimeEdit*> specValWdg) {
    return visit( [&clean](auto &x) { return ( ! clean && x->specialValueText() == x->text() ) ? " (" + x->text() + ")" : "" ; } , specValWdg);
  };
  if (!obj)
    return "";
  else if (QLineEdit* cobj = qobject_cast<QLineEdit*>(obj))
    return quoteNonEmpty(cobj->text());
  else if (QPlainTextEdit* cobj = qobject_cast<QPlainTextEdit*>(obj))
    return quoteNonEmpty(cobj->toPlainText());
  else if (QAbstractButton* cobj = qobject_cast<QAbstractButton*>(obj))
    return cobj->isChecked() ? "true" : "false";
  else if (QComboBox* cobj = qobject_cast<QComboBox*>(obj))
    return quoteNonEmpty(cobj->currentText());
  else if (QSpinBox* cobj = qobject_cast<QSpinBox*>(obj))
    return QString::number(cobj->value()) + specValue(cobj);
  else if (QDoubleSpinBox* cobj = qobject_cast<QDoubleSpinBox*>(obj))
    return QString::number(cobj->value()) + specValue(cobj);
  else if (QTimeEdit* cobj = qobject_cast<QTimeEdit*>(obj))
    return cobj->time().toString() + specValue(cobj);
  else if (QLabel* cobj = qobject_cast<QLabel*>(obj))
    return quoteNonEmpty(cobj->text());
  else if (UScript* cobj = qobject_cast<UScript*>(obj))
    return quoteNonEmpty(cobj->script->path());
  else if (QCaMotorGUI* cobj = qobject_cast<QCaMotorGUI*>(obj))
    return cobj->motor()->getPv();
  else if (QButtonGroup* cobj = qobject_cast<QButtonGroup*>(obj))
    return cobj->checkedButton() ? quoteNonEmpty(cobj->checkedButton()->text()) : "";
  else if (QTableWidgetOtem* cobj = qobject_cast<QTableWidgetOtem*>(obj)){
    QTableWidget * wdg = cobj->tableWidget();
    // following commented line returns -1 for headeritem: have to get it manually
    // const int column = cobj->column();
    const int column = [&wdg,&cobj]() {
      for (int col=0; col < wdg->columnCount() ; col++)
        if (wdg->horizontalHeaderItem(col)==cobj)
          return col;
      qDebug() << "Can't find column in tablewidget with given horizontal header item.";
      return -1;
    } ();
    if (column<0)
      return "";
    QStringList list;
    for (int row = 0 ; row < wdg->rowCount() ; row++)
      if ( QCheckBox* chBox = wdg->cellWidget(row, column)->findChild<QCheckBox*>() )
        list << ( chBox->isChecked() ? "true" : "false" );
      else
        list << wdg->item(row, column)->text();
    return clean  ?  list.join(',')  :  "["+list.join(',')+"]" ;
  } else
    qDebug() << "Cannot get default value of object" << obj
             << ": unsupported type" << obj->metaObject()->className();
  return "";
}



int str2obj (QObject* obj, const QString & sval) {

  QVariant rval = sval;
  if (!obj) {
    qDebug() << "Zero QObject";
    return 0;

  } else if (QLineEdit* cobj = qobject_cast<QLineEdit*>(obj))
    cobj->setText(sval);

  else if (QPlainTextEdit* cobj = qobject_cast<QPlainTextEdit*>(obj))
    cobj->setPlainText(sval);

  else if (QAbstractButton* cobj = qobject_cast<QAbstractButton*>(obj)) {
    if ( rval.convert(QMetaType::Bool) )
      cobj->setChecked(rval.toBool());
    else {
      qDebug() << sval << "can't convert to bool";
      return 0;
    }

  } else if (QComboBox* cobj = qobject_cast<QComboBox*>(obj)) {
    const int idx = cobj->findText(sval);
    if ( idx >= 0 )
      cobj->setCurrentIndex(idx);
    else if ( cobj->isEditable() )
      cobj->setEditText(sval);
    else {
      QStringList knownFields;
      for (int cidx=0 ; cidx < cobj->count() ; cidx++ )
        knownFields << cobj->itemText(cidx);
      qDebug() << cobj->metaObject()->className() << sval << "not in existing values:" << knownFields;
      return 0;
    }

  } else if (QSpinBox* cobj = qobject_cast<QSpinBox*>(obj)) {
    if ( rval.convert(QMetaType::Int) )
      cobj->setValue(rval.toInt());
    else {
      qDebug() << sval << "can't convert to integer";
      return 0;
    }

  } else if (QDoubleSpinBox* cobj = qobject_cast<QDoubleSpinBox*>(obj)) {
    if ( rval.convert(QMetaType::Double) )
      cobj->setValue(rval.toDouble());
    else {
      qDebug() << sval << "can't convert to float";
      return 0;
    }

  } else if (QTimeEdit* cobj = qobject_cast<QTimeEdit*>(obj)) {
    if ( rval.convert(QMetaType::QTime) )
      cobj->setTime(rval.toTime());
    else {
      qDebug() << sval << "can't convert to QTime";
      return 0;
    }

  } else if (QLabel* cobj = qobject_cast<QLabel*>(obj))
    cobj->setText(sval);

  else if (UScript* cobj = qobject_cast<UScript*>(obj))
    cobj->script->setPath(sval);

  else if (QCaMotorGUI* cobj = qobject_cast<QCaMotorGUI*>(obj))
    cobj->motor()->setPv(sval);

  else if (QButtonGroup* cobj = qobject_cast<QButtonGroup*>(obj)) {
    QStringList knownFields;
    foreach (QAbstractButton * but, cobj->buttons()) {
      knownFields << but->text();
      if (but->text() == sval) {
        but->setChecked(true);
        return 1;
      }
    }
    qDebug() << cobj->metaObject()->className() << sval << "not in existing values:" << knownFields;
    return 0;

  } else if (QTableWidgetOtem* cobj = qobject_cast<QTableWidgetOtem*>(obj)){
    QTableWidget * wdg = cobj->tableWidget();
    const int rows = wdg->rowCount();
    const int column = cobj->column();
    const QStringList list = sval.split(',');
    if (list.size() != rows) {
      QString msg = "Warning. List size " + QString::number(list.size()) + " produced from string \""
                  + sval + "\" is not same as table size " + QString::number(rows) + ".";
      if (PositionList * lst = qobject_cast<PositionList*>(wdg->parentWidget()))
        msg += " Possibly size option " + configName(lst->ui->nof) + " is set after the content (option "
             + configName(cobj) + ")?";
      qDebug() << msg;
    }
    for (int row = 0 ; row < min(rows, list.size()) ; row++) {
      if ( list[row].isEmpty() )
        continue;
      if ( QCheckBox* chBox = wdg->cellWidget(row, column)->findChild<QCheckBox*>() ) {
        QVariant curv = QVariant::fromValue(list[row]);
        if (curv.convert(QMetaType::Bool))
          chBox->setChecked(curv.toBool());
        else
          qDebug() << sval << "can't convert to bool. Skipping row" << row ;
      } else
        wdg->item(row, column)->setText(list[row]);
    }

  } else {
    qDebug() << "Cannot parse value of object" << obj
             << "into cli arguments: unsupported type" << obj->metaObject()->className();
    return 0;
  }
  return 1;
}



std::string type_desc (QObject* obj) {
  if (!obj) {
    qDebug() << "Zero QObject";
    return "";
  } else if (qobject_cast<const QLineEdit*>(obj))
    return "string";
  else if (qobject_cast<const QPlainTextEdit*>(obj))
    return "text";
  else if (qobject_cast<const QAbstractButton*>(obj))
    return "bool";
  else if (const QComboBox* cobj = qobject_cast<const QComboBox*>(obj))
    return cobj->isEditable() ? "string" : "enum";
  else if (qobject_cast<const QSpinBox*>(obj))
    return "int";
  else if (qobject_cast<const QDoubleSpinBox*>(obj))
    return "float";
  else if (qobject_cast<const QTimeEdit*>(obj))
    return "QTime";
  else if (qobject_cast<const QLabel*>(obj))
    return "string";
  else if (qobject_cast<const UScript*>(obj))
    return "command";
  else if (qobject_cast<const QCaMotorGUI*>(obj))
    return "EpicsPV";
  else if (qobject_cast<const QButtonGroup*>(obj))
    return "enum";
  else if (qobject_cast<const QTableWidgetOtem*>(obj))
    return "list";
  else
    qDebug() << "Cannot add value of object" << obj
             << "into cli arguments: unsupported type" << obj->metaObject()->className();
  return obj->metaObject()->className();
};
int _conversion (QObject* _val, const string & in) {
  str2obj(_val, QString::fromStdString(in));
  return 1;
}



void addOpt( poptmx::OptionTable & otable, QObject * sobj, const QString & sname
           , const string & preShort="", const string & preLong="" ){

  if (Shutter* cobj = qobject_cast<Shutter*>(sobj)) {
    addOpt( otable, cobj->ui->selection, sname
          , cobj->whatsThis().toStdString(), cobj->toolTip().toStdString());
    return;
  }

  string shortDesc;
  string longDesc;
  QString ttAdd = "\n\n--" + sname ;

  if (QButtonGroup* cobj = qobject_cast< QButtonGroup*>(sobj)) {
    QStringList knownFields;
    foreach (QAbstractButton * but, cobj->buttons()) {
      but->setToolTip(but->toolTip() + ttAdd + " \"" + but->text() + "\"");
      knownFields << "'" + but->text() + "'";
    }
    shortDesc = cobj->property("whatsThis").toString().toStdString();
    const QString tt = cobj->property("toolTip").toString();
    longDesc = "Possible values: " + knownFields.join(", ").toStdString() + ".\n"
             + tt.toStdString();
    cobj->setProperty("toolTip", tt + ttAdd );
  } else if (QComboBox* cobj = qobject_cast<QComboBox*>(sobj)) {
    QStringList knownFields;
    for (int cidx=0 ; cidx < cobj->count() ; cidx++ )
      knownFields << "'" + cobj->itemText(cidx) + "'";
    longDesc = "Possible values: " + knownFields.join(", ").toStdString()
        + (cobj->isEditable() ? " or any text" : "") + ".\n";
  } else if ( QAbstractSpinBox* cobj = qobject_cast<QAbstractSpinBox*>(sobj) ) {
    QString specialValText = cobj->specialValueText();
    if (!specialValText.isEmpty()) {
      QString specialVal;
      if (QSpinBox* sbobj = qobject_cast<QSpinBox*>(sobj))
        specialVal = QString::number(sbobj->minimum());
      else if (QDoubleSpinBox* sbobj = qobject_cast<QDoubleSpinBox*>(sobj))
        specialVal = QString::number(sbobj->minimum());
      else if (QTimeEdit* sbobj = qobject_cast<QTimeEdit*>(sobj))
        specialVal = sbobj->minimumTime().toString();
      longDesc = ("Special value " + specialVal + " for '"  + specialValText + "'.\n")
          .toStdString();
    }
  }

  if ( qobject_cast<QWidget*>(sobj)  ||  qobject_cast<QTableWidgetOtem*>(sobj)) {
    variant<QWidget*, QTableWidgetOtem*> ctrlWdg = (QWidget*) nullptr;
    if (QCaMotorGUI* cobj = qobject_cast<QCaMotorGUI*>(sobj))
      ctrlWdg = cobj->setupButton();
    else if (QWidget * wdg = qobject_cast<QWidget*>(sobj))
      ctrlWdg = wdg;
    else if (QTableWidgetOtem * item = qobject_cast<QTableWidgetOtem*>(sobj))
      ctrlWdg = item;
    shortDesc = visit( [](auto &x) { return x->whatsThis(); }, ctrlWdg).toStdString();
    longDesc += visit( [](auto &x) { return x->toolTip(); }, ctrlWdg).toStdString();
    ttAdd += " " + QString::fromStdString(type_desc(sobj));
    visit( [&ttAdd](auto &x) { x->setToolTip( x->toolTip() + ttAdd ); }, ctrlWdg);
  }

  // adding option to parse table
  if (shortDesc.empty() && preShort.empty())
    qDebug() << "Empty whatsThis for " << sobj << QString::fromStdString(longDesc);
  const string svalue = obj2str(sobj, false).toStdString();
  otable.add( poptmx::OPTION, sobj, 0, sname.toStdString()
            , shortDesc + preShort, longDesc + preLong
            , svalue.empty() ? "<none>" : svalue);

};





MainWindow::MainWindow(int argc, char *argv[], QWidget *parent) :
  QMainWindow(parent),
  isLoadingState(false),
  ui(new Ui::MainWindow),
  bgOrigin(0),
  bgAcquire(0),
  bgEnter(0),
  shutterPri(new Shutter),
  shutterSec(new Shutter),
  det(new Detector(this)),
  tct(new TriggCT(this)),
  thetaMotor(new QCaMotorGUI),
  spiralMotor(new QCaMotorGUI),
  bgMotor(new QCaMotorGUI),
  dynoMotor(new QCaMotorGUI),
  dyno2Motor(new QCaMotorGUI),
  stopMe(true)
{

  QList<QCaMotorGUI*> allMotors;
  // Prepare UI
  {

  ui->setupUi(this);
  foreach ( QWidget * sprl , findChildren<QWidget*>(
              QRegularExpression("spiral", QRegularExpression::CaseInsensitiveOption)) )
    sprl->hide(); // until spiral CT is implemented
  ui->control->finilize();
  ui->control->setCurrentIndex(0);
  foreach ( QMDoubleSpinBox * spb , findChildren<QMDoubleSpinBox*>() )
    spb->setConfirmationRequired(false);
  foreach ( QMSpinBox * spb , findChildren<QMSpinBox*>() )
    spb->setConfirmationRequired(false);

  prsSelection << ui->aqsSpeed << ui->stepTime << ui->flyRatio;
  selectPRS(); // initial selection only
  foreach (QMDoubleSpinBox* _prs, prsSelection)
    connect( _prs, SIGNAL(entered()), SLOT(selectPRS()) );

  ColumnResizer * resizer;
  resizer = new ColumnResizer(this);
  ui->preRunScript->addToColumnResizer(resizer);
  ui->postRunScript->addToColumnResizer(resizer);
  resizer = new ColumnResizer(this);
  ui->preScanScript->addToColumnResizer(resizer);
  ui->postScanScript->addToColumnResizer(resizer);
  resizer = new ColumnResizer(this);
  ui->preSubLoopScript->addToColumnResizer(resizer);
  ui->postSubLoopScript->addToColumnResizer(resizer);
  resizer = new ColumnResizer(this);
  ui->preAqScript->addToColumnResizer(resizer);
  ui->postAqScript->addToColumnResizer(resizer);

  ui->startStop->setText(ssText);
  ui->statusBar->setObjectName("StatusBar");
  ui->statusBar->setStyleSheet("#StatusBar {color: rgba(255, 0, 0, 128);}");
  QPushButton * save = new QPushButton("Save");
  save->setFlat(true);
  connect(save, SIGNAL(clicked()), SLOT(saveConfiguration()));
  ui->statusBar->addPermanentWidget(save);
  QPushButton * load = new QPushButton("Load");
  load->setFlat(true);
  connect(load, SIGNAL(clicked()), SLOT(loadConfiguration()));
  ui->statusBar->addPermanentWidget(load);

  for (int caqmod=0 ; caqmod < AQMODEEND ; caqmod++)
    ui->aqMode->insertItem(caqmod, AqModeString( AqMode(caqmod) ));
  ui->scanProgress->hide();
  ui->dynoProgress->hide();
  ui->serialProgress->hide();
  ui->multiProgress->hide();
  ui->stepTime->setSuffix("s");
  ui->exposureInfo->setSuffix("s");
  ui->detExposure->setSuffix("s");
  ui->detPeriod->setSuffix("s");
  foreach (Detector::Camera cam , Detector::knownCameras)
    ui->detSelection->addItem(Detector::cameraName(cam));

  auto placeMotor = [&allMotors](QCaMotorGUI *mot, QWidget *ctrlHere, QWidget *posHere){
    mot->setupButton()->setToolTip(ctrlHere->toolTip());
    mot->setupButton()->setWhatsThis(ctrlHere->whatsThis());
    if (ctrlHere->property(configProp).isValid()) {
      mot->setupButton()->setProperty(
        configProp, ctrlHere->property(configProp));
      ctrlHere->setProperty(configProp,QVariant());
    }
    place(mot->setupButton(), ctrlHere);
    place(mot->currentPosition(true), posHere);
    allMotors << mot;
  };
  placeMotor(thetaMotor, ui->placeThetaMotor, ui->placeScanCurrent);
  placeMotor(spiralMotor, ui->placeSpiralMotor, ui->placeSpiralCurrent);
  placeMotor(bgMotor, ui->placeBGmotor, ui->placeBGcurrent);
  placeMotor(dynoMotor, ui->placeDynoMotor, ui->placeDynoCurrent);
  placeMotor(dyno2Motor, ui->placeDyno2Motor, ui->placeDyno2Current);

  foreach( PositionList * lst, findChildren<PositionList*>() ) {
    QCaMotorGUI * mot = lst->motui;
    mot->setupButton()->setToolTip("Motor to position list item.");
    allMotors << mot;
  }

  }

  // Update Ui's
  {

  updateUi_expPath();
  updateUi_pathSync();
  updateUi_serials();
  updateUi_ffOnEachScan();
  updateUi_scanRange();
  updateUi_aqsPP();
  updateUi_scanStep();
  updateUi_expOverStep();
  updateUi_thetaMotor();
  updateUi_bgTravel();
  updateUi_bgInterval();
  updateUi_dfInterval();
  updateUi_bgMotor();
  updateUi_loops();
  updateUi_dynoSpeed();
  updateUi_dynoMotor();
  updateUi_dyno2Speed();
  updateUi_dyno2Motor();
  updateUi_detector();
  updateUi_aqMode();
  updateUi_hdf();

  onSerialCheck();
  onFFcheck();
  onDynoCheck();
  onMultiCheck();
  onDyno2();
  onDetectorSelection();

  }

  // Connect signals from UI elements
  {
  connect(ui->browseExpPath, SIGNAL(clicked()), SLOT(onWorkingDirBrowse()));
  connect(ui->checkSerial, SIGNAL(toggled(bool)), SLOT(onSerialCheck()));
  connect(ui->checkFF, SIGNAL(toggled(bool)), SLOT(onFFcheck()));
  connect(ui->checkDyno, SIGNAL(toggled(bool)), SLOT(onDynoCheck()));
  connect(ui->checkMulti, SIGNAL(toggled(bool)), SLOT(onMultiCheck()));
  connect(ui->testSerial, SIGNAL(clicked(bool)), SLOT(onSerialTest()));
  connect(ui->testFF, SIGNAL(clicked()), SLOT(onFFtest()));
  connect(ui->testScan, SIGNAL(clicked()), SLOT(onScanTest()));
  connect(ui->testMulti, SIGNAL(clicked()), SLOT(onLoopTest()));
  connect(ui->testDyno, SIGNAL(clicked()), SLOT(onDynoTest()));
  connect(ui->dynoDirectionLock, SIGNAL(toggled(bool)), SLOT(onDynoDirectionLock()));
  connect(ui->dynoSpeedLock, SIGNAL(toggled(bool)), SLOT(onDynoSpeedLock()));
  connect(ui->dyno2, SIGNAL(toggled(bool)), SLOT(onDyno2()));
  connect(ui->testDetector, SIGNAL(clicked()), SLOT(onDetectorTest()));
  connect(ui->vidStartStop, SIGNAL(clicked()), SLOT(onVideoRecord()));
  connect(ui->vidReady, SIGNAL(clicked()), SLOT(onVideoGetReady()));
  connect(ui->startStop, SIGNAL(clicked()), SLOT(onStartStop()));
  connect(ui->swapSerialLists, SIGNAL(clicked(bool)), SLOT(onSwapSerial()));
  connect(ui->swapLoopLists, SIGNAL(clicked(bool)), SLOT(onSwapLoops()));
  }


  // Populate list of configuration parameters with elements of the UI
  // which have configProp ("saveToConfig") property set.
  {
  QList<QObject*> addThemAfterNof;
  QWidget * wdg = ui->control;
  while ( wdg = wdg->nextInFocusChain() ,
          wdg && wdg != ui->control )
  {
    if ( ui->control->tabs().contains(wdg) )
      continue;
    QObject * obj = wdg;
    if ( QAbstractButton * but = qobject_cast<QAbstractButton*>(wdg)
       ; but && but->group() && but->group()->property(configProp).isValid() )
      obj = but->group();
    if ( ! obj->property(configProp).isValid()  )
      continue;
    if ( QAbstractButton * but = qobject_cast<QAbstractButton*>(wdg) ) {
      foreach( QCaMotorGUI* mot, allMotors )
        if (mot->setupButton() == but)
          obj=mot;
    }
    if ( configs.contains(obj) )
      continue;
    if (PositionList* cobj = qobject_cast<PositionList*>(obj)) {
      // Data in the columns must be loaded after correct number of raws are set via ui->nof of the list.
      // Therefore instead of adding following to configs immediately, they are put aside and  added
      // later when table is resized.
      if (addThemAfterNof.size()) {
        qDebug() << "Bug. PositionList table must be empty by now. How did I get here?";
        configs.append(addThemAfterNof);
        addThemAfterNof.clear();
      }
      addThemAfterNof << dynamic_cast<QTableWidgetOtem*>(cobj->ui->list->horizontalHeaderItem(0))
                      << dynamic_cast<QTableWidgetOtem*>(cobj->ui->list->horizontalHeaderItem(3));
    } else {
      configs << obj;
      if ( PositionList * parentPositionList = qobject_cast<PositionList*>(obj->parent())
         ; parentPositionList  &&  parentPositionList->ui->nof == obj ) {
        if (!addThemAfterNof.size())
          qDebug() << "Bug. PositionList is empty wwhen must not be. How did I get here?";
        else
          configs.append(addThemAfterNof);
        addThemAfterNof.clear();
      }
    }
  }
  }

  // Load configuration and parse cmd options
  {

  string configFile = storedState.toStdString();
  string fakestr;
  bool fakebool;
  auto constructOptionTable = [this,&configFile,&fakestr,&fakebool](bool fake) {

    poptmx::OptionTable table("CT acquisition", "Executes CT experiment.");
    table
        .add(poptmx::NOTE, "ARGUMENTS:")
        .add(poptmx::ARGUMENT, &configFile, "configuration", "Configuration file.",
             "Ini file to be loaded on start.", configFile)
        .add(poptmx::NOTE, "OPTIONS:");
    QString lastSection = "";
    foreach (auto obj, configs) {
      QString sname = configName(obj);
      if ( QString section = sname.split('/').first()
         ; section != lastSection )
      {
        table.add(poptmx::NOTE, "Section " + section.toStdString());
        lastSection = section;
      }
      if (fake)
        table.add(poptmx::OPTION, &fakestr, 0, configName(obj).toStdString(), "shortDesc", "");
      else
        addOpt(table, obj, sname);
    }
    table
        .add(poptmx::NOTE, "ACTIONS:")
        .add(poptmx::OPTION, &startExp, 0, "startExp", "Starts the experiment",
             "Launches execution of the experiment as if manually pressing \"Start\".")
        .add(poptmx::OPTION, &startVid, 0, "startVid", "Starts video recording",
             "Launches video recording as if \"Get ready\" and \"Record\" buttons were pressed manually.")
        .add(poptmx::OPTION, &reportHealth, 0, "health", "Check if configuration allows start.",
             "Check if loaded configuration is self consistent to allow experiment execution."
             " Returns 0 if everything is fine, non-zero otherwise.")
        .add(poptmx::OPTION, &failAfter, 0, "fail", "Maximum time in seconds to wait for readiness",
             "If the system does not become available for requested action withing specified time,"
             " consider it a failure. Default is 1 second.")
        .add(poptmx::OPTION, &keepUi, 0, "keepui", "Keeps UI open on completion.",
             "Has no effect if no launchers were started. By default application"
             " exits after a launcher has finished. This option allows to keep it open.")
        .add(poptmx::OPTION, &headless, 0, "headless", "Starts launcher without UI.",
             "Only makes sense with one of the above processing launchers;"
             " without it the flag is ignored and UI shows.");
    if (fake)
      table
          .add(poptmx::OPTION, &fakebool, 'h', "help", " ", "")
          .add(poptmx::OPTION, &fakebool, 'u', "usage", " ", "")
          .add(poptmx::OPTION, &fakebool, 'v', "verbose", " ", "");
    else
      table.add_standard_options(&beverbose);

    return table;

  };


  // Parse and load configuration file
  poptmx::OptionTable ftable = constructOptionTable(true);
  ftable.parse(argc,argv);
  loadConfiguration(QString::fromStdString(configFile));
  // Only after loading configFile, I prepare and parse argv properly.
  poptmx::OptionTable otable = constructOptionTable(false);

  #define timeToReturn(retVal, msg) {\
    if (!msg.empty()) \
      qDebug() << QString::fromStdString(msg); \
    QTimer::singleShot(0, [](){QApplication::exit(retVal);}); \
    return; \
  }

  if (!otable.parse(argc, argv))
    timeToReturn(0, string());
  if ( ! startExp  &&  ! startVid ) {
    for (void * var : initializer_list<void*>{&headless, &keepUi, &failAfter} )
      if (otable.count(var))
        timeToReturn(1, string("Option " + otable.desc(var) + " can only be used together with "
            + otable.desc(&startExp) + " or " + otable.desc(&startVid) +"."));
    keepUi=true;
    headless=false;
  } else {
    if (!otable.count(&headless))
      headless=false;
    if (!otable.count(&keepUi))
      keepUi=false;
  }
  if ( reportHealth ) {

  }
  if ( 1 < otable.count(&startExp) + otable.count(&startVid) + otable.count(&reportHealth) )
    timeToReturn(1, string("Only one of the following options can be used at a time: " +
        otable.desc(&startExp) + otable.desc(&startVid) + otable.desc(&reportHealth) + ".") );
  if (otable.count(&headless) && otable.count(&keepUi))
    timeToReturn(1, string("Incompatible options " + otable.desc(&headless) + " and " + otable.desc(&keepUi) + "."));

  #undef timeToReturn

  }

  storeCurrentState();
  // connect changes in the configuration elements to store state
  foreach (auto obj, configs) {
    const char * sig = 0;
    if ( qobject_cast<QLineEdit*>(obj) ||
         qobject_cast<QPlainTextEdit*>(obj) ||
         qobject_cast<UScript*>(obj) ||
         qobject_cast<QSpinBox*>(obj) ||
         qobject_cast<QAbstractSpinBox*>(obj) )
      sig = SIGNAL(editingFinished());
    else if (qobject_cast<QComboBox*>(obj))
      sig = SIGNAL(currentIndexChanged(int));
    else if (qobject_cast<QAbstractButton*>(obj))
      sig = SIGNAL(toggled(bool));
    else if ( qobject_cast<QButtonGroup*>(obj))
      sig = SIGNAL(buttonClicked(int));
    else if (qobject_cast<QCaMotorGUI*>(obj)) {
      obj = qobject_cast<QCaMotorGUI*>(obj)->motor();
      sig = SIGNAL(changedPv());
    } else if (qobject_cast<QTableWidgetOtem*>(obj)) {
      obj = qobject_cast<QTableWidgetOtem*>(obj)->tableWidget();
      sig = SIGNAL(itemChanged(QTableWidgetItem*));
    } else if (qobject_cast<Shutter*>(obj))
      sig = SIGNAL(shutterChanged());
    else
      qDebug() << "Do not know how to connect object " << obj
               << "to slot" << SLOT(storeCurrentState());
    if (sig)
      connect (obj, sig, SLOT(storeCurrentState()));
  }

  auto afterStart = [this](){
    if ( ! this->headless )
      this->show();
    if (this->startExp)
      this->onStartStop();
    if (this->startVid)
      this->onVideoRecord();
    if ( ! this->keepUi )
      this->close();
  };
  QTimer::singleShot(0, afterStart);

}



MainWindow::~MainWindow() {
  delete ui;
}


void MainWindow::saveConfiguration(QString fileName) {

  if ( fileName.isEmpty() )
    fileName = QFileDialog::getSaveFileName(0, "Save configuration", QDir::currentPath());
  QSettings config(fileName, QSettings::IniFormat);
  config.clear();

  config.setValue("version", QString::number(APP_VERSION));
  foreach (auto obj, configs) {
    QString fname = configName(obj);
    if (fname.startsWith("general/", Qt::CaseInsensitive))
      fname.remove(0,8);
    if (const PositionList *pl = qobject_cast<const PositionList*>(obj)) {
      config.setValue(fname + "/positions", obj2str(dynamic_cast<QTableWidgetOtem*>(pl->ui->list->horizontalHeaderItem(0))));
      config.setValue(fname + "/todos", obj2str(dynamic_cast<QTableWidgetOtem*>(pl->ui->list->horizontalHeaderItem(3))));
    } else if (const Shutter * shut =  qobject_cast<const Shutter*>(obj)) {
      config.setValue(fname, obj2str(shut->ui->selection));
      config.setValue(fname+"_custom", shut->readCustomDialog());
    } else
      config.setValue(fname, obj2str(obj));
  }
  if (const QMDoubleSpinBox * prs = selectedPRS())
    config.setValue("scan/fixprs", prs->objectName());
  if (det) {
    config.setValue("hardware/detectorpv", det->pv());
    config.setValue("hardware/detectorexposure", det->exposure());
    config.setValue("hardware/detectorsavepath", det->path(uiImageFormat()));
  }

  setenv("SERIALMOTORIPV", innearList->motui->motor()->getPv().toLatin1() , 1);
  setenv("SERIALMOTOROPV", outerList->motui->motor()->getPv().toLatin1() , 1);
  setenv("LOOPMOTORIPV", loopList->motui->motor()->getPv().toLatin1() , 1);
  setenv("SUBLOOPMOTOROPV", sloopList->motui->motor()->getPv().toLatin1() , 1);

}


void MainWindow::loadConfiguration(QString fileName) {

  if ( fileName.isEmpty() )
    fileName = QFileDialog::getOpenFileName(0, "Load configuration", QDir::currentPath());
  QSettings config(fileName, QSettings::IniFormat);
  isLoadingState=true;

  auto fromConf2Obj = [](QObject * obj, QString key, const QSettings & config){
    if (key.startsWith("general/", Qt::CaseInsensitive))
      key.remove(0,8);
    if (!config.contains(key))
      qDebug() << "Key" << key << "not found in config file" << config.fileName();
    else {
      QString val = config.value(key).toString();
      if (!val.isEmpty())
        str2obj(obj, val);
    }
  };

  //const QString ver = config.value("version").toString();
  if (config.contains("scan/fixprs")) {
    const QString fixprsName = config.value("scan/fixprs").toString();
    foreach (QAbstractSpinBox* prs, prsSelection)
      if ( fixprsName == prs->objectName() )
        selectPRS(prs);
    selectPRS(selectedPRS()); // if nothing found
  }
  foreach (auto obj, configs) {
    const QString fname = configName(obj);
    if (const PositionList *pl = qobject_cast<const PositionList*>(obj)) {
      fromConf2Obj(dynamic_cast<QTableWidgetOtem*>(pl->ui->list->horizontalHeaderItem(0)), fname + "/positions",config);
      fromConf2Obj(dynamic_cast<QTableWidgetOtem*>(pl->ui->list->horizontalHeaderItem(3)), fname + "/todos",config);
    } else if (Shutter * shut =  qobject_cast<Shutter*>(obj) ) {
      const QString cname = fname+"_custom";
      if (config.contains(cname)) {
        shut->loadCustomDialog(config.value(cname).toStringList());
        fromConf2Obj(shut->ui->selection, fname, config);
        shut->setShutter();
      }
    } else
      fromConf2Obj(obj, fname, config);
  }

  if ( ui->expPath->text().isEmpty()  || fileName.isEmpty() )
    ui->expPath->setText( QFileInfo(fileName).absolutePath());

  isLoadingState=false;

}


Detector::ImageFormat MainWindow::uiImageFormat() const {
  if (ui->tiffFormat->isChecked())
    return Detector::TIF;
  else if (ui->hdfFormat->isChecked())
    return Detector::HDF;
  else
    return Detector::UNDEFINED; // should never happen
}


void MainWindow::addMessage(const QString & str) {
//  str.size();
//  ui->messages->append(
//        QDateTime::currentDateTime().toString("dd/MM/yyyy_hh:mm:ss.zzz") +
//        " " + str);
}


static QString lastPathComponent(const QString & pth) {
  QString lastComponent = pth;
  if ( lastComponent.endsWith("\\") ) {
    lastComponent.chop(1);
    lastComponent.replace("\\", "/");
  } else if ( lastComponent.endsWith("/") ) // can be win or lin path delimiter
    lastComponent.chop(1);
  lastComponent = QFileInfo(lastComponent).fileName();
  return lastComponent;
}


void MainWindow::updateUi_expPath() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_expPath());
    connect( ui->expPath, SIGNAL(textChanged(QString)), thisSlot );
    connect( ui->detPathSync, SIGNAL(toggled(bool)), thisSlot );
    connect( det, SIGNAL(connectionChanged(bool)), thisSlot);
  }

  const QString pth = ui->expPath->text();

  bool isOK;
  if (QDir::isAbsolutePath(pth)) {
    QFileInfo fi(pth);
    isOK = fi.isDir() && fi.isWritable();
  } else
    isOK = false;

  if (isOK) {
    QDir::setCurrent(pth);
    if ( ui->detPathSync->isChecked() && det->isConnected() ) {
      QString lastComponent = lastPathComponent(pth);
      QString detDir = det->path(uiImageFormat());
      if ( detDir.endsWith("/") || detDir.endsWith("\\") ) // can be win or lin path delimiter
        detDir.chop(1);
      const int delidx = detDir.lastIndexOf( QRegExp("[/\\\\]") );
      if ( delidx >=0 )
        detDir.truncate(delidx+1);
      det->setPath(detDir + lastComponent);
    }
  }

  check(ui->expPath, isOK);

}


void MainWindow::updateUi_pathSync() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_pathSync());
    connect( ui->expPath, SIGNAL(textChanged(QString)), thisSlot );
    connect( ui->detPathSync, SIGNAL(toggled(bool)), thisSlot );
    connect( det, SIGNAL(parameterChanged()), thisSlot);
  }
  check( ui->detPathSync,
         ! ui->detPathSync->isChecked()  ||
         ! ui->detSelection->currentIndex() ||
         lastPathComponent(det->path(uiImageFormat())) == lastPathComponent(ui->expPath->text()));
}


void MainWindow::updateUi_aqMode() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_aqMode());
    connect( tct, SIGNAL(connectionChanged(bool)), thisSlot);
    connect( tct, SIGNAL(runningChanged(bool)), thisSlot);
    connect( ui->aqMode, SIGNAL(currentIndexChanged(int)), thisSlot);
  }

  const AqMode aqmd = (AqMode) ui->aqMode->currentIndex();

  if ( sender() == ui->aqMode ) {
    if ( aqmd == FLYHARD2B ) {
      tct->setPrefix("SR08ID01ZEB02");
      ui->checkExtTrig->setVisible(false);
    } else if ( aqmd == FLYHARD3B ) {
      tct->setPrefix("SR08ID01ZEB01");
      ui->checkExtTrig->setVisible(false);
    } else {
      tct->setPrefix("");
      ui->checkExtTrig->setVisible(true);
    }
  }

  bool isOK = tct->prefix().isEmpty() ||
      ( tct->isConnected() && ! tct->isRunning() );
  check(ui->aqMode, isOK);

  const bool sasmd = aqmd == STEPNSHOT;

  ui->ffOnEachScan->setVisible(!sasmd);
  ui->speedsLine->setVisible(!sasmd);
  ui->speedsLabel->setVisible(!sasmd);
  ui->timesLabel->setVisible(!sasmd);
  ui->normalSpeed->setVisible(!sasmd);
  ui->normalSpeedLabel->setVisible(!sasmd);
  ui->stepTime->setVisible(!sasmd);
  ui->stepTimeLabel->setVisible(!sasmd);
  ui->flyRatio->setVisible(!sasmd);
  ui->rotSpeedLabel->setVisible(!sasmd);
  ui->aqsSpeed->setVisible(!sasmd);
  ui->expOverStepLabel->setVisible(!sasmd);
  ui->maximumSpeed->setVisible(!sasmd);
  ui->maximumSpeedLabel->setVisible(!sasmd);
  ui->exposureInfo->setVisible(!sasmd);
  ui->exposureInfoLabel->setVisible(!sasmd);
  ui->checkDyno->setVisible(sasmd);
  ui->checkMulti->setVisible(sasmd);

  if ( ! sasmd ) {
    ui->checkDyno->setChecked(false);
    ui->checkMulti->setChecked(false);
  }

}


void MainWindow::updateUi_hdf() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_hdf());
    connect(ui->aqMode, SIGNAL(currentIndexChanged(int)), thisSlot);
    connect(ui->imageFormatGroup, SIGNAL(buttonClicked(int)), thisSlot);
    connect(ui->detSelection, SIGNAL(currentIndexChanged(int)), thisSlot);
    connect(det, SIGNAL(connectionChanged(bool)), thisSlot);
    connect(det, SIGNAL(parameterChanged()), thisSlot);
  }
  const bool flymode = (AqMode) ui->aqMode->currentIndex() != STEPNSHOT;
  ui->hdfFormat->setEnabled( det->hdfReady() && flymode );
  check( ui->hdfFormat, ! ui->hdfFormat->isChecked() ||
         ( flymode && ( det->hdfReady() || ! det->isConnected() ) ) );
}


void MainWindow::updateUi_serials() {

  if ( ! sender() ) { // called from the constructor;

    const char* thisSlot = SLOT(updateUi_serials());
    connect(ui->endConditionButtonGroup, SIGNAL(buttonClicked(int)), thisSlot);
    connect(ui->serial2D, SIGNAL(toggled(bool)), thisSlot);
    connect(innearList, SIGNAL(amOKchanged(bool)), thisSlot);
    connect(outerList, SIGNAL(amOKchanged(bool)), thisSlot);

    connect(ui->serial2D, SIGNAL(toggled(bool)), ui->innearListPlace, SLOT(setVisible(bool)));
    connect(ui->serial2D, SIGNAL(toggled(bool)), ui->pre2DScript, SLOT(setVisible(bool)));
    connect(ui->serial2D, SIGNAL(toggled(bool)), ui->pre2DSlabel, SLOT(setVisible(bool)));
    connect(ui->serial2D, SIGNAL(toggled(bool)), ui->post2DScript, SLOT(setVisible(bool)));
    connect(ui->serial2D, SIGNAL(toggled(bool)), ui->post2DSlabel, SLOT(setVisible(bool)));
    ui->serial2D->toggle();
    ui->serial2D->toggle(); // twice to keep initial state

    connect(ui->endNumber, SIGNAL(toggled(bool)), ui->outerListPlace, SLOT(setVisible(bool)));
    connect(ui->endTime,   SIGNAL(toggled(bool)), ui->acquisitionTimeWdg, SLOT(setVisible(bool)));
    connect(ui->endTime,   SIGNAL(toggled(bool)), ui->acquisitionTimeLabel, SLOT(setVisible(bool)));
    connect(ui->endCondition, SIGNAL(toggled(bool)), ui->conditionScript, SLOT(setVisible(bool)));
    connect(ui->endCondition, SIGNAL(toggled(bool)), ui->conditionScriptLabel, SLOT(setVisible(bool)));
    ui->endTime->toggle();
    ui->endCondition->toggle();
    ui->endNumber->toggle();

  }


  const bool do2D = ui->serial2D->isChecked();
  ui->serial2D->setVisible(ui->endNumber->isChecked());
  ui->swapSerialLists->setVisible( do2D && ui->endNumber->isChecked() );
  ui->serialTabSpacer->setHidden( do2D || ui->endNumber->isChecked() );
  innearList->putLabel("innear\nloop\n\n[Z]");
  outerList->putLabel( do2D ? "outer\nloop\n\n[Y]" : "[Y]");

  check(ui->serial2D, ! do2D || innearList->amOK() );
  check(ui->endNumber, ! ui->endNumber->isChecked() || outerList->amOK() );

}


void MainWindow::onSwapSerial() {

  PositionList * ol = outerList;
  PositionList * il = innearList;
  QString tTi, tTo;
  tTo = ol->property(configProp).toString();
  tTi = il->property(configProp).toString();
  ol->setProperty(configProp, tTi);
  il->setProperty(configProp, tTo);

  #define swapTT(elm) \
    tTi = il->elm->toolTip(); \
    tTo = ol->elm->toolTip(); \
    ol->elm->setToolTip(tTi); \
    il->elm->setToolTip(tTo); \

  swapTT(ui->nof);
  swapTT(ui->step);
  swapTT(ui->irregular);
  swapTT(motui->setupButton());
  swapTT(ui->list->horizontalHeaderItem(0));
  swapTT(ui->list->horizontalHeaderItem(3));
  #undef swapTT

  ui->innearListPlace->layout()->addWidget(ol);
  ui->outerListPlace->layout()->addWidget(il);
  updateUi_serials();
  storeCurrentState();

}


void MainWindow::updateUi_ffOnEachScan() {
    if ( ! sender() ) // called from the constructor;
        connect( ui->ongoingSeries, SIGNAL(toggled(bool)), SLOT(updateUi_ffOnEachScan()));
    ui->ffOnEachScan->setEnabled( ! ui->ongoingSeries->isChecked() );
    if ( ui->ongoingSeries->isChecked() )
        ui->ffOnEachScan->setChecked(false);
}


void MainWindow::updateUi_aqsPP() {
  if ( ! sender() ) {
    const char* thisSlot = SLOT(updateUi_aqsPP());
    connect( ui->aqMode, SIGNAL(currentIndexChanged(int)), thisSlot);
    connect( ui->checkDyno, SIGNAL(toggled(bool)), thisSlot);
  }

  bool vis = ui->aqMode->currentIndex() == STEPNSHOT &&  ! ui->checkDyno->isChecked();
  ui->aqsPP->setVisible(vis);
  ui->aqsPPLabel->setVisible(vis);

}


void MainWindow::updateUi_scanRange() {
  QCaMotor * mot = thetaMotor->motor();
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_scanRange());
    connect( ui->scanRange, SIGNAL(valueChanged(double)), thisSlot);
    connect( mot, SIGNAL(changedConnected(bool)), thisSlot);
    connect( mot, SIGNAL(changedUserPosition(double)), thisSlot);
    connect( mot, SIGNAL(changedPrecision(int)), thisSlot);
    connect( mot, SIGNAL(changedUnits(QString)), thisSlot);
    connect( mot, SIGNAL(changedUserLoLimit(double)), thisSlot);
    connect( mot, SIGNAL(changedUserHiLimit(double)), thisSlot);
    connect(ui->checkSerial, SIGNAL(toggled(bool)), thisSlot);
    connect(ui->swapSerialLists, SIGNAL(clicked(bool)), thisSlot); //make sure connected after onSwapSerials()
    connect(ui->ongoingSeries, SIGNAL(toggled(bool)), thisSlot);
    connect(ui->endNumber, SIGNAL(toggled(bool)), thisSlot);
    connect(ui->serial2D, SIGNAL(toggled(bool)), thisSlot);
    connect(innearList, SIGNAL(parameterChanged()), thisSlot);
    connect(outerList, SIGNAL(parameterChanged()), thisSlot);
  }

  if ( mot->isConnected() ) {
    ui->scanRange->setSuffix(mot->getUnits());
    ui->scanRange->setDecimals(mot->getPrecision());
  }

  double endpos = mot->getUserPosition();
  if ( ui->checkSerial->isChecked() &&
       ui->ongoingSeries->isChecked() &&
       ui->innearListPlace->isVisible() )
    endpos +=  ( innearList->ui->nof->value() - 1 ) * ui->scanRange->value();
  else
    endpos +=  ui->scanRange->value();

  check(ui->scanRange,
        ui->scanRange->value() != 0.0 &&
      endpos > mot->getUserLoLimit() &&
      endpos < mot->getUserHiLimit() );

}


void MainWindow::updateUi_scanStep() {
  QCaMotor * mot = thetaMotor->motor();
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_scanStep());
    connect( ui->scanRange, SIGNAL(valueChanged(double)), thisSlot);
    connect( ui->scanProjections, SIGNAL(valueChanged(int)), thisSlot);
    connect( ui->scanStep, SIGNAL(valueEdited(double)), thisSlot);
    connect( mot, SIGNAL(changedConnected(bool)), thisSlot);
    connect( mot, SIGNAL(changedPrecision(int)), thisSlot);
    connect( mot, SIGNAL(changedUnits(QString)), thisSlot);
  }

  if ( mot->isConnected() ) {
    ui->scanStep->setSuffix(mot->getUnits());
    ui->scanStep->setDecimals(mot->getPrecision());
  }
  float absRange = abs(ui->scanRange->value());
  if (sender() == ui->scanStep) {
    int projections = round( absRange / ui->scanStep->value());
    ui->scanProjections->setValue( round( absRange / ui->scanStep->value()) );
    ui->scanStep->QDoubleSpinBox::setValue(absRange/projections);
  }
  else
    ui->scanStep->setValue( absRange / ui->scanProjections->value() );

}


QMDoubleSpinBox * MainWindow::selectPRS(QObject *prso) {

  QMDoubleSpinBox * prs;
  prs = qobject_cast<QMDoubleSpinBox*>(prso);
  if ( ! prs )
    prs = qobject_cast<QMDoubleSpinBox*>(sender());
  if ( ! prs || ! prsSelection.contains(prs) )
    prs=selectedPRS();
  if ( ! prs )
    prs=prsSelection.at(0);

  foreach (QMDoubleSpinBox* _prs, prsSelection) {
    _prs->setFrame(_prs==prs);
    _prs->setButtonSymbols(
          _prs == prs ? QAbstractSpinBox::UpDownArrows : QAbstractSpinBox::NoButtons );
  }

  return prs;

}


QMDoubleSpinBox * MainWindow::selectedPRS() const {
  QMDoubleSpinBox * prs=0;
  int aprs=0;
  foreach (QMDoubleSpinBox* _prs, prsSelection)
    if (_prs->hasFrame()) {
      prs = _prs;
      aprs++;
    }
  return  (aprs == 1)  ?  prs  :  0 ;
}


void MainWindow::updateUi_expOverStep() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_expOverStep());
    foreach (QMDoubleSpinBox* _prs, prsSelection)
      connect( _prs, SIGNAL(valueChanged(double)), thisSlot);
    connect( ui->aqMode, SIGNAL(currentIndexChanged(int)), thisSlot);
    connect( ui->scanRange, SIGNAL(valueChanged(double)), thisSlot);
    connect( ui->scanProjections, SIGNAL(valueChanged(int)), thisSlot);
    connect( det, SIGNAL(parameterChanged()), thisSlot);
    connect( thetaMotor->motor(), SIGNAL(changedMaximumSpeed(double)), thisSlot);
  }

  const float stepSize = ui->scanRange->value() / ui->scanProjections->value();
  const float exposure = det->exposure();
  float stepTime = ui->stepTime->value();
  float aqsSpeed = ui->aqsSpeed->value();
  float flyRatio = ui->flyRatio->value();
  const float maxSpeed = thetaMotor->motor()->getMaximumSpeed();

  bool isOK =  true;
  const QMDoubleSpinBox * prs=selectedPRS();
  if ( prs == ui->stepTime ) {
    flyRatio = exposure / stepTime;
    ui->flyRatio->setValue(flyRatio);
    aqsSpeed = stepSize / stepTime;
    ui->aqsSpeed->setValue(aqsSpeed);
  } else if ( prs == ui->flyRatio ) {
    stepTime = exposure / flyRatio ;
    ui->stepTime->setValue(stepTime);
    aqsSpeed = stepSize * flyRatio / exposure ;
    ui->aqsSpeed->setValue(aqsSpeed);
  } else if ( prs == ui->aqsSpeed ) {
    stepTime = stepSize / aqsSpeed;
    ui->stepTime->setValue(stepTime);
    flyRatio = aqsSpeed * exposure / stepSize;
    ui->flyRatio->setValue(flyRatio);
  } else {
    qDebug() << "ERROR! PRS selection is buggy. Report to developper. Must never happen.";
    isOK=false;
  }

  ui->stepTime->setMinimum(exposure);
  ui->aqsSpeed->setMaximum(maxSpeed);

  isOK &=  flyRatio <= 1  &&  aqsSpeed <= maxSpeed  &&  stepTime >= exposure;
  isOK |= ui->aqMode->currentIndex() == STEPNSHOT;
  foreach (QMDoubleSpinBox* _prs, prsSelection)
    check( _prs, _prs != prs || isOK );

}


void MainWindow::updateUi_thetaMotor() {
  QCaMotor * mot = thetaMotor->motor();
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_thetaMotor());
    connect( mot, SIGNAL(changedConnected(bool)), thisSlot);
    connect( mot, SIGNAL(changedPrecision(int)), thisSlot);
    connect( mot, SIGNAL(changedUnits(QString)), thisSlot);
    connect( mot, SIGNAL(changedMoving(bool)), thisSlot);
    connect( mot, SIGNAL(changedNormalSpeed(double)), thisSlot);
    connect( mot, SIGNAL(changedMaximumSpeed(double)), thisSlot);
    connect( mot, SIGNAL(changedLoLimitStatus(bool)), thisSlot);
    connect( mot, SIGNAL(changedHiLimitStatus(bool)), thisSlot);
  }

  setenv("ROTATIONMOTORPV", mot->getPv().toLatin1() , 1);

  check(thetaMotor->setupButton(),
        mot->isConnected() && ! mot->isMoving() && ! mot->getLimitStatus());

  if (!mot->isConnected())
    return;

  QString units = mot->getUnits();
  units += "/s";
  ui->normalSpeed->setSuffix(units);
  ui->maximumSpeed->setSuffix(units);
  ui->aqsSpeed->setSuffix(units);

  const int prec = mot->getPrecision();
  ui->normalSpeed->setDecimals(prec);
  ui->maximumSpeed->setDecimals(prec);
  ui->aqsSpeed->setDecimals(mot->getPrecision());

  ui->normalSpeed->setValue(mot->getNormalSpeed());
  ui->maximumSpeed->setValue(mot->getMaximumSpeed());

}


void MainWindow::updateUi_bgInterval() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_bgInterval());
    connect( ui->scanProjections, SIGNAL(valueChanged(int)), thisSlot);
    connect( ui->aqMode, SIGNAL(currentIndexChanged(int)), thisSlot);
    connect( ui->nofBGs, SIGNAL(valueChanged(int)), thisSlot);
    connect( ui->bgIntervalSAS, SIGNAL(valueChanged(int)), thisSlot);
    connect( ui->bgIntervalAfter, SIGNAL(toggled(bool)), thisSlot);
    connect( ui->bgIntervalBefore, SIGNAL(toggled(bool)), thisSlot);
  }

  const bool sasMode = ui->aqMode->currentIndex() == STEPNSHOT;

  ui->bgInterval->setEnabled(ui->nofBGs->value());
  ui->bgIntervalSAS->setVisible(sasMode);
  ui->bgIntervalContinious->setVisible(!sasMode);

  bool itemOK;

  itemOK =
      ! sasMode ||
      ! ui->nofBGs->value() ||
      ui->bgIntervalSAS->value() <= ui->scanProjections->value();
  check(ui->bgIntervalSAS, itemOK);

  itemOK =
      sasMode ||
      ! ui->nofBGs->value() ||
      ( ui->bgIntervalAfter->isChecked() || ui->bgIntervalBefore->isChecked() );
  check(ui->bgIntervalAfter, itemOK );
  check(ui->bgIntervalBefore, itemOK );

}


void MainWindow::updateUi_dfInterval() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_dfInterval());
    connect( ui->scanProjections, SIGNAL(valueChanged(int)), thisSlot);
    connect( ui->aqMode, SIGNAL(currentIndexChanged(int)), thisSlot);
    connect( ui->nofDFs, SIGNAL(valueChanged(int)), thisSlot);
    connect( ui->dfIntervalSAS, SIGNAL(valueChanged(int)), thisSlot);
    connect( ui->dfIntervalAfter, SIGNAL(toggled(bool)), thisSlot);
    connect( ui->dfIntervalBefore, SIGNAL(toggled(bool)), thisSlot);
  }

  const bool sasMode = ui->aqMode->currentIndex() == STEPNSHOT;
  ui->dfInterval->setEnabled(ui->nofDFs->value());
  ui->dfIntervalSAS->setVisible(sasMode);
  ui->dfIntervalContinious->setVisible(!sasMode);

  bool itemOK;

  itemOK =
      ! sasMode ||
      ! ui->nofDFs->value() ||
      ui->nofDFs->value() <= ui->scanProjections->value();
  check(ui->dfIntervalSAS, itemOK);

  itemOK =
      sasMode ||
      ! ui->nofDFs->value() ||
      ( ui->dfIntervalAfter->isChecked() || ui->dfIntervalBefore->isChecked() );
  check(ui->dfIntervalAfter, itemOK );
  check(ui->dfIntervalBefore, itemOK );


}


void MainWindow::updateUi_bgTravel() {
  QCaMotor * mot = bgMotor->motor();
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_bgTravel());
    connect( ui->scanProjections, SIGNAL(valueChanged(int)), thisSlot);
    connect( ui->nofBGs, SIGNAL(valueChanged(int)), thisSlot);
    connect( ui->bgTravel, SIGNAL(valueChanged(double)), thisSlot);
    connect( mot, SIGNAL(changedConnected(bool)), thisSlot);
    connect( mot, SIGNAL(changedUserLoLimit(double)), thisSlot);
    connect( mot, SIGNAL(changedUserHiLimit(double)), thisSlot);
    connect( mot, SIGNAL(changedPrecision(int)), thisSlot);
    connect( mot, SIGNAL(changedUnits(QString)), thisSlot);
    connect( mot, SIGNAL(stopped()), thisSlot);
  }


  if (mot->isConnected()) {
    ui->bgTravel->setSuffix(mot->getUnits());
    ui->bgTravel->setDecimals(mot->getPrecision());
  }

  if (  bgEnter == bgAcquire  &&  ! mot->isMoving() && ! inRun(ui->testFF) && ! inRun(ui->startStop) ) {
    bgOrigin = mot->getUserPosition();
    bgAcquire = bgOrigin + ui->bgTravel->value();
    bgEnter = bgAcquire;
  }

  const int nofbgs = ui->nofBGs->value();
  ui->bgTravel->setEnabled(nofbgs);
  //const double endpos = mot->getUserPosition() + ui->bgTravel->value();
  const bool itemOK =
      ! nofbgs ||
      ( ui->bgTravel->value() != 0.0  &&
      bgAcquire > mot->getUserLoLimit() && bgAcquire < mot->getUserHiLimit() ) ;
  check(ui->bgTravel, itemOK);

}


void MainWindow::updateUi_bgMotor() {
  QCaMotor * mot = bgMotor->motor();
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_bgMotor());
    connect( ui->nofBGs, SIGNAL(valueChanged(int)), thisSlot);
    connect( ui->checkFF, SIGNAL(toggled(bool)), thisSlot);
    connect( mot, SIGNAL(changedConnected(bool)), thisSlot);
    connect( mot, SIGNAL(changedMoving(bool)), thisSlot);
    connect( mot, SIGNAL(changedLoLimitStatus(bool)), thisSlot);
    connect( mot, SIGNAL(changedHiLimitStatus(bool)), thisSlot);
  }

  bgMotor->setupButton()->setEnabled(ui->nofBGs->value());
  check(bgMotor->setupButton(),
        ! ui->checkFF->isChecked() || ! ui->nofBGs->value() ||
        ( mot->isConnected()  &&  ! mot->isMoving()  &&  ! mot->getLimitStatus() ) );

}


void MainWindow::updateUi_loops() {
  if ( ! sender() ) { // called from the constructor;

    const char* thisSlot = SLOT(updateUi_loops());
    connect(ui->checkMulti, SIGNAL(toggled(bool)), thisSlot);
    connect(ui->subLoop, SIGNAL(toggled(bool)), thisSlot);
    connect(loopList, SIGNAL(amOKchanged(bool)), thisSlot);
    connect(sloopList, SIGNAL(amOKchanged(bool)), thisSlot);

    connect(ui->subLoop, SIGNAL(toggled(bool)), ui->sloopListPlace, SLOT(setVisible(bool)));
    connect(ui->subLoop, SIGNAL(toggled(bool)), ui->preSubLoopScript, SLOT(setVisible(bool)));
    connect(ui->subLoop, SIGNAL(toggled(bool)), ui->preSubLoopScriptLabel, SLOT(setVisible(bool)));
    connect(ui->subLoop, SIGNAL(toggled(bool)), ui->postSubLoopScript, SLOT(setVisible(bool)));
    connect(ui->subLoop, SIGNAL(toggled(bool)), ui->postSubLoopScriptLabel, SLOT(setVisible(bool)));
    connect(ui->subLoop, SIGNAL(toggled(bool)), ui->swapLoopLists, SLOT(setVisible(bool)));
    ui->subLoop->toggle(); ui->subLoop->toggle(); // twice to keep original

  }

  loopList->putLabel("loop\n\n[L]");
  sloopList->putLabel("sub\nloop\n\n[S]");

  const bool loopisOk = ! ui->checkMulti->isChecked()  ||  ! ui->subLoop->isChecked()  || sloopList->amOK();
  check(ui->subLoop, loopisOk );
  check(sloopList->ui->label, loopisOk );
  check(loopList->ui->label, ! ui->checkMulti->isChecked()  ||  loopList->amOK() );

}


void MainWindow::onSwapLoops() {

  PositionList * ll = loopList;
  PositionList * sl = sloopList;
  QString tTl, tTs;
  tTs = sl->property(configProp).toString();
  tTl = ll->property(configProp).toString();
  sl->setProperty(configProp, tTl);
  ll->setProperty(configProp, tTs);
  #define swapTT(elm) \
    tTl = ll->elm->toolTip(); \
    tTs = sl->elm->toolTip(); \
    sl->elm->setToolTip(tTl); \
    ll->elm->setToolTip(tTs); \

  swapTT(ui->nof);
  swapTT(ui->step);
  swapTT(ui->irregular);
  swapTT(motui->setupButton());
  swapTT(ui->list->horizontalHeaderItem(0));
  swapTT(ui->list->horizontalHeaderItem(3));
  #undef swapTT

  ui->loopListPlace->layout()->addWidget(sl);
  ui->sloopListPlace->layout()->addWidget(ll);
  updateUi_loops();
  storeCurrentState();

}


void MainWindow::updateUi_dynoSpeed() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_dynoSpeed());
    connect( ui->dynoSpeed , SIGNAL(editingFinished()), thisSlot);
    connect( dynoMotor->motor(), SIGNAL(changedConnected(bool)), thisSlot);
    connect( dynoMotor->motor(), SIGNAL(changedPrecision(int)), thisSlot);
    connect( dynoMotor->motor(), SIGNAL(changedUnits(QString)), thisSlot);
    connect( dynoMotor->motor(), SIGNAL(changedNormalSpeed(double)), thisSlot);
    connect( dynoMotor->motor(), SIGNAL(changedMaximumSpeed(double)), thisSlot);
    connect( ui->dynoSpeedLock, SIGNAL(toggled(bool)), thisSlot);
    connect( dyno2Motor->motor(), SIGNAL(changedMaximumSpeed(double)), thisSlot);
    connect( dyno2Motor->motor(), SIGNAL(changedConnected(bool)), thisSlot);
    connect( ui->dyno2, SIGNAL(toggled(bool)), thisSlot);
  }

  if ( ! dynoMotor->motor()->isConnected() )
    return;

  if ( ui->dynoSpeed->value()==0.0 )
    ui->dynoSpeed->setValue(dynoMotor->motor()->getNormalSpeed());
  ui->dynoSpeed->setSuffix(dynoMotor->motor()->getUnits()+"/s");
  ui->dynoSpeed->setDecimals(dynoMotor->motor()->getPrecision());


  double maximum = dynoMotor->motor()->getMaximumSpeed();
  if (ui->dyno2->isChecked() && ui->dynoSpeedLock->isChecked() &&
      dyno2Motor->motor()->isConnected())
    maximum = qMax(maximum, dynoMotor->motor()->getMaximumSpeed());
  check( ui->dynoSpeed, ui->dyno2Speed->value() <= maximum );

}


void MainWindow::updateUi_dynoMotor() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_dynoMotor());
    connect( ui->checkDyno, SIGNAL(toggled(bool)), thisSlot);
    connect( dynoMotor->motor(), SIGNAL(changedConnected(bool)), thisSlot);
    connect( dynoMotor->motor(), SIGNAL(changedPrecision(int)), thisSlot);
    connect( dynoMotor->motor(), SIGNAL(changedUnits(QString)), thisSlot);
    connect( dynoMotor->motor(), SIGNAL(changedMoving(bool)), thisSlot);
    connect( dynoMotor->motor(), SIGNAL(changedLoLimitStatus(bool)), thisSlot);
    connect( dynoMotor->motor(), SIGNAL(changedHiLimitStatus(bool)), thisSlot);

    connect(dynoMotor->motor(), SIGNAL(changedNormalSpeed(double)),
            ui->dynoNormalSpeed, SLOT(setValue(double)));

  }

  check(dynoMotor->setupButton(),
        ! ui->checkDyno->isChecked() ||
        ( dynoMotor->motor()->isConnected() && ! dynoMotor->motor()->isMoving() && ! dynoMotor->motor()->getLimitStatus()) );

  if ( dynoMotor->motor()->isConnected() ) {
    ui->dynoNormalSpeed->setDecimals(dynoMotor->motor()->getPrecision());
    ui->dynoNormalSpeed->setSuffix(dynoMotor->motor()->getUnits()+"/s");
  }


}


void MainWindow::updateUi_dyno2Speed() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_dyno2Speed());
    connect( ui->dyno2Speed , SIGNAL(editingFinished()), thisSlot);
    connect( dyno2Motor->motor(), SIGNAL(changedConnected(bool)), thisSlot);
    connect( dyno2Motor->motor(), SIGNAL(changedPrecision(int)), thisSlot);
    connect( dyno2Motor->motor(), SIGNAL(changedUnits(QString)), thisSlot);
    connect( dyno2Motor->motor(), SIGNAL(changedNormalSpeed(double)), thisSlot);
    connect( dyno2Motor->motor(), SIGNAL(changedMaximumSpeed(double)), thisSlot);
    connect( ui->dynoSpeedLock, SIGNAL(toggled(bool)), thisSlot);
  }

  if ( ! dyno2Motor->motor()->isConnected() )
    return;

  if ( ui->dyno2Speed->value()==0.0 )
    ui->dyno2Speed->setValue(dyno2Motor->motor()->getNormalSpeed());
  ui->dyno2Speed->setSuffix(dyno2Motor->motor()->getUnits()+"/s");
  ui->dyno2Speed->setDecimals(dyno2Motor->motor()->getPrecision());
  ui->dyno2Speed->setDisabled(ui->dynoSpeedLock->isChecked());

  check(ui->dyno2Speed,
        ui->dyno2Speed->value() <= dyno2Motor->motor()->getMaximumSpeed() );

}


void MainWindow::updateUi_dyno2Motor() {
  if ( ! sender() ) { // called from the constructor;
    const char* thisSlot = SLOT(updateUi_dyno2Motor());
    connect( ui->checkDyno, SIGNAL(toggled(bool)), thisSlot);
    connect( ui->dyno2, SIGNAL(toggled(bool)), thisSlot);
    connect( dyno2Motor->motor(), SIGNAL(changedConnected(bool)), thisSlot);
    connect( dyno2Motor->motor(), SIGNAL(changedPrecision(int)), thisSlot);
    connect( dyno2Motor->motor(), SIGNAL(changedUnits(QString)), thisSlot);
    connect( dyno2Motor->motor(), SIGNAL(changedMoving(bool)), thisSlot);
    connect( dyno2Motor->motor(), SIGNAL(changedLoLimitStatus(bool)), thisSlot);
    connect( dyno2Motor->motor(), SIGNAL(changedHiLimitStatus(bool)), thisSlot);

    connect(dyno2Motor->motor(), SIGNAL(changedNormalSpeed(double)),
            ui->dyno2NormalSpeed, SLOT(setValue(double)));

  }

  check(dyno2Motor->setupButton(),
        ! ui->checkDyno->isChecked() || ! ui->dyno2->isChecked() ||
        ( dyno2Motor->motor()->isConnected() && ! dyno2Motor->motor()->isMoving() && ! dyno2Motor->motor()->getLimitStatus()) );

  if ( dyno2Motor->motor()->isConnected() ) {
    ui->dyno2NormalSpeed->setDecimals(dyno2Motor->motor()->getPrecision());
    ui->dyno2NormalSpeed->setSuffix(dyno2Motor->motor()->getUnits()+"/s");
  }


}


void MainWindow::updateUi_detector() {
  if ( ! sender() ) {
    const char* thisSlot = SLOT(updateUi_detector());
    connect(ui->detSelection, SIGNAL(currentIndexChanged(int)), SLOT(onDetectorSelection()));
    connect(ui->detSelection, SIGNAL(currentIndexChanged(int)), thisSlot);
    connect(ui->vidFrames, SIGNAL(valueChanged(int)), thisSlot);
    connect(ui->vidTime, SIGNAL(timeChanged(QTime)), thisSlot);
    connect(det, SIGNAL(connectionChanged(bool)), thisSlot);
    connect(det, SIGNAL(parameterChanged()), thisSlot);
    connect(det, SIGNAL(counterChanged(int)), ui->detProgress, SLOT(setValue(int)));
    connect(det, SIGNAL(counterChanged(int)), ui->detImageCounter, SLOT(setValue(int)));
    connect(det, SIGNAL(lastNameChanged(QString)), ui->detFileLastName, SLOT(setText(QString)));
  }

  if ( ! ui->detSelection->currentIndex() )
    ui->detStatus->setText("always ready");
  else if ( ! det->isConnected() )
    ui->detStatus->setText("no link");
  else if ( det->isAcquiring() )
    ui->detStatus->setText("acquiring");
  else if ( det->isWriting() )
    ui->detStatus->setText("writing");
  else
    ui->detStatus->setText("idle");

  if ( det->camera() != Detector::NONE && det->isConnected() ) {

    bool enabme = det->imageFormat(Detector::TIF);
    ui->tiffEnabled->setText( enabme ? "enabled" : "disabled" );
    ui->detFileNameTiff->setEnabled(enabme);
    ui->detFileTemplateTiff->setEnabled(enabme);
    ui->detPathTiff->setEnabled(enabme);

    enabme = det->imageFormat(Detector::HDF);
    ui->hdfEnabled->setText( enabme ? "enabled" : "disabled" );
    ui->detFileNameHdf->setEnabled(enabme);
    ui->detFileTemplateHdf->setEnabled(enabme);
    ui->detPathHdf->setEnabled(enabme);


    ui->detProgress->setMaximum( det->imageMode() == 2 ? det->toCapture() : det->number() );
    ui->detProgress->setValue( det->imageMode() == 2 ? det->captured() : det->counter() );
    ui->detProgress->setVisible( det->imageMode() == 2 ? det->isCapturing() : det->isAcquiring() );
    ui->detTotalImages->setValue(det->number());
    ui->exposureInfo->setValue(det->exposure());
    ui->detExposure->setValue(det->exposure());
    ui->detPeriod->setValue(det->period());
    ui->detTriggerMode->setText(det->triggerModeString());
    ui->detImageMode->setText(det->imageModeString());
    ui->detTotalImages->setValue(det->number());

    ui->detFileNameTiff->setText(det->name(Detector::TIF));
    ui->detFileNameHdf->setText(det->name(Detector::HDF));
    ui->detFileTemplateTiff->setText(det->nameTemplate(Detector::TIF));
    ui->detFileTemplateHdf->setText(det->nameTemplate(Detector::HDF));
    ui->detPathTiff->setText(det->path(Detector::TIF));
    ui->detPathHdf->setText(det->path(Detector::HDF));

    if (!preVideoState)
      ui->vidStartStop->setText("Start");
    ui->vidReady->setEnabled( preVideoState || ! det->isWriting() );
    const float realPeriod = 1000 * std::max(det->exposure(), det->period()); // s -> ms
    if (realPeriod>0) {
      const QTime zeroTime(0,0);
      ui->vidTime->setMinimumTime(QTime(zeroTime).addMSecs(realPeriod));
      if (sender() == ui->vidFrames) {
        ui->vidTime->blockSignals(true);
        ui->vidTime->setTime( QTime(zeroTime).addMSecs(realPeriod * ui->vidFrames->value()) );
        ui->vidTime->blockSignals(false);
      } else {
        ui->vidFrames->blockSignals(true);
        ui->vidFrames->setValue(zeroTime.msecsTo(ui->vidTime->time())/realPeriod);
        ui->vidFrames->blockSignals(false);
      }
    }

  } else {
    ui->vidStartStop->setText("no link");
    ui->videoWidget ->setDisabled(true);
    ui->testDetector->setDisabled(true);
  }

  check (ui->detStatus, ! ui->detSelection->currentIndex() || ( det->isConnected() &&
         ( inRun(ui->startStop) || ( ! det->isAcquiring() && ! det->isWriting() ) ) ) );
  check (ui->detPathTiff, uiImageFormat() != Detector::TIF  ||  det->pathExists(Detector::TIF) || ! ui->detSelection->currentIndex() );
  check (ui->detPathHdf, uiImageFormat() != Detector::HDF  ||  det->pathExists(Detector::HDF) || ! ui->detSelection->currentIndex());


}


void MainWindow::onWorkingDirBrowse() {
  QDir startView( QDir::current() );
  startView.cdUp();
  const QString newdir = QFileDialog::getExistingDirectory(0, "Working directory", startView.path() );
  if ( ! newdir.isEmpty() )
    ui->expPath->setText(newdir);
}


void MainWindow::onSerialCheck() {
  ui->control->setTabVisible(ui->tabSerial, ui->checkSerial->isChecked());
  check( ui->tabSerial, true );
}


void MainWindow::onFFcheck() {
  ui->control->setTabVisible(ui->tabFF, ui->checkFF->isChecked());
  ui->ffOnEachScan->setVisible(ui->checkFF->isChecked());
  check( ui->tabFF, true );
}


void MainWindow::onDynoCheck() {
  ui->control->setTabVisible(ui->tabDyno, ui->checkDyno->isChecked());
  check( ui->tabDyno, true );
}


void MainWindow::onMultiCheck() {
  ui->control->setTabVisible(ui->tabMulti, ui->checkMulti->isChecked());
  check( ui->tabMulti, true );
}


void MainWindow::onDyno2() {
  const bool dod2 = ui->dyno2->isChecked();
  ui->placeDyno2Motor->setVisible(dod2);
  ui->dynoDirectionLock->setVisible(dod2);
  ui->dynoSpeedLock->setVisible(dod2);
  ui->placeDyno2Current->setVisible(dod2);
  ui->dyno2Neg->setVisible(dod2);
  ui->dyno2NormalSpeed->setVisible(dod2);
  ui->dyno2Pos->setVisible(dod2);
  ui->dyno2Speed->setVisible(dod2);
  ui->dyno2NormalSpeedLabel->setVisible(dod2);
  ui->dyno2SpeedLabel->setVisible(dod2);
  ui->dyno2DirectionLabel->setVisible(dod2);
}


void MainWindow::onDynoSpeedLock() {
  ui->dyno2Speed->setDisabled(ui->dynoSpeedLock->isChecked());
  if (ui->dynoSpeedLock->isChecked()) {
    connect(ui->dynoSpeed, SIGNAL(valueChanged(double)),
            ui->dyno2Speed, SLOT(setValue(double)));
    ui->dyno2Speed->setValue(ui->dynoSpeed->value());
  } else
    disconnect(ui->dynoSpeed, SIGNAL(valueChanged(double)),
               ui->dyno2Speed, SLOT(setValue(double)));
}


void MainWindow::onDynoDirectionLock() {
  const bool lockDir = ui->dynoDirectionLock->isChecked();
  ui->dyno2Pos->setDisabled(lockDir);
  ui->dyno2Neg->setDisabled(lockDir);
  if (lockDir) {
    connect(ui->dynoPos, SIGNAL(toggled(bool)),
            ui->dyno2Pos, SLOT(setChecked(bool)));
    connect(ui->dynoNeg, SIGNAL(toggled(bool)),
            ui->dyno2Neg, SLOT(setChecked(bool)));
    ui->dyno2Pos->setChecked(ui->dynoPos->isChecked());
    ui->dyno2Neg->setChecked(ui->dynoNeg->isChecked());
  } else {
    disconnect(ui->dynoPos, SIGNAL(toggled(bool)),
               ui->dyno2Pos, SLOT(setChecked(bool)));
    disconnect(ui->dynoNeg, SIGNAL(toggled(bool)),
               ui->dyno2Neg, SLOT(setChecked(bool)));
  }
}


void MainWindow::onDetectorSelection() {
  const QString currentText = ui->detSelection->currentText();
  Detector::Camera cam = Detector::NONE;
  foreach (Detector::Camera ccam , Detector::knownCameras)
    if (currentText==Detector::cameraName(ccam))
      cam=ccam;
  det->setCamera(cam);
  setenv("DETECTORPV", det->pv().toLatin1(), 1);
  updateUi_detector();
}


void MainWindow::check(QWidget * obj, bool status) {

  if ( ! obj )
    return;

  obj->setStyleSheet( status  ?  ""  :  warnStyle );

  QWidget * tab = 0;
  if ( ui->control->tabs().contains(obj) ) {

    tab = obj;

  } else if ( ! preReq.contains(obj) ) {

    if (obj == ui->startStop)
      tab = ui->control;
    else
      foreach( QWidget * wdg, ui->control->tabs() )
        if ( wdg->findChildren< const QWidget* >().contains(obj) ) {
          tab = wdg;
          break;
        }

    if (tab)
      preReq[obj] = qMakePair(status, (const QWidget*) tab);

  } else {

    tab = (QWidget*) preReq[obj].second;
    preReq[obj].first = status;

  }

  bool tabOK=status;
  if ( status ) {
    foreach ( ReqP tabel, preReq)
      if ( tabel.second == tab )
        tabOK &= tabel.first;
  }

  const bool anyInRun = inRun(ui->startStop)
      || inRun(ui->testDetector)
      || inRun(ui->vidStartStop)
      || inRun(ui->testDyno)
      || inRun(ui->testFF)
      || inRun(ui->testMulti)
      || inRun(ui->testSerial)
      || inRun(ui->testScan);

  if (tab) {

    tabOK |= ui->control->indexOf(tab) == -1;
    preReq[tab] = qMakePair( tabOK, (const QWidget*) 0 );
    ui->control->setTabTextColor(tab, tabOK ? QColor() : QColor(Qt::red));

    ui->testDetector->setEnabled (  inRun(ui->testDetector)
                                 || ( ! anyInRun && det->camera() && preReq[ui->tabDetector].first ) );
    ui->videoWidget->setEnabled ( inRun(ui->vidStartStop) || preVideoState
                                  || ( det->isConnected() && ! det->isWriting() ) );
    ui->testDyno->setEnabled ( inRun(ui->testDyno) || ( ! anyInRun &&
                                               preReq[ui->tabDetector].first &&
                                               preReq[ui->tabDyno].first ) );
    ui->testMulti->setEnabled ( inRun(ui->testMulti) || ( ! anyInRun &&
                                                 preReq[ui->tabDetector].first &&
                                                 preReq[ui->tabDyno].first &&
                                                 preReq[ui->tabMulti].first) );
    ui->testFF->setEnabled ( inRun(ui->testFF) ||  ( ! anyInRun &&
                               preReq[ui->tabDetector].first &&
                               preReq[ui->tabDyno].first &&
                               preReq[ui->tabMulti].first &&
                               preReq[ui->tabFF].first) );
    ui->testSerial->setEnabled( inRun(ui->testSerial) || ( ! anyInRun &&
                                    preReq[ui->tabDetector].first &&
                                    preReq[ui->tabSerial].first  ) );
    ui->testScan->setEnabled( inRun(ui->testScan) || ( ! anyInRun &&
                                    preReq[ui->tabDetector].first &&
                                    preReq[ui->tabScan].first  ) );

  }

  bool readyToStartCT=status;
  if ( status ) {
    foreach ( ReqP tabel, preReq)
      if ( ! tabel.second )
        readyToStartCT &= tabel.first;
  }
  ui->startStop->setEnabled( readyToStartCT || anyInRun );
  ui->startStop->setStyleSheet( anyInRun  ?  warnStyle  :  "" );
  ui->startStop->setText( anyInRun  ?  "Stop"  :  ssText );

}


// Marks the btn to indicate the process running or not.
// Used *Test functions
QString MainWindow::mkRun(QAbstractButton * btn, bool inr, const QString & txt) {
  if (inr)
    ui->statusBar->clearMessage();
  if ( ! btn )
    return "";
  check(btn, ! inr);
  const QString ret = btn->text();
  if ( ! txt.isEmpty() )
    btn->setText(txt);
  return ret;
}


// Returns true if the marked btn indicates the process is running.
// Used *Test functions
bool MainWindow::inRun(const QAbstractButton * btn) {
  return btn && preReq.contains(btn) && ! preReq[btn].first;
}


void MainWindow::onSerialTest() {

  if (inRun(ui->testSerial)) {
    stopAll();
    return;
  }

  stopMe=false;
  const QString butText = mkRun(ui->testSerial, true, "Stop");
  det->waitWritten();
  HWstate hw(det, shutterSec, shutterPri);
  ui->ssWidget->setEnabled(false);

  shutterPri->open();
  shutterSec->open();
  engineRun();

  hw.restore();
  ui->ssWidget->setEnabled(true);
  mkRun(ui->testSerial, false, butText);

}


void MainWindow::onScanTest() {

  if (inRun(ui->testScan)) {
    stopAll();
    return;
  }

  stopMe=false;
  const QString butText = mkRun(ui->testScan, true, "Stop");
  ui->scanWidget->setEnabled(false);
  det->waitWritten();
  HWstate hw(det, shutterSec, shutterPri);

  shutterPri->open();
  shutterSec->open();
  engineRun();

  hw.restore();
  ui->scanWidget->setEnabled(true);
  mkRun(ui->testScan, false, butText);


}


void MainWindow::onFFtest() {

  if (inRun(ui->testFF)) {
    stopAll();
    return;
  }

  stopMe=false;
  const QString butText = mkRun(ui->testFF, true, "Stop");
  ui->ffWidget->setEnabled(false);
  det->waitWritten();
  HWstate hw(det, shutterSec, shutterPri);

  moveToBG();
  acquireDF("", Shutter::CLOSED);
  det->waitWritten();
  acquireBG("");
  det->waitWritten();
  shutterPri->close();

  det->setAutoSave(false);
  hw.restore();
  ui->ffWidget->setEnabled(true);
  mkRun(ui->testFF, false, butText);

  QTimer::singleShot(0, this, SLOT(updateUi_bgMotor()));

}


void MainWindow::onLoopTest() {

  if (inRun(ui->testMulti)) {
    stopAll();
    return;
  }

  stopMe=false;
  const QString butText = mkRun(ui->testMulti, true, "Stop");
  ui->multiWidget->setEnabled(false);
  det->waitWritten();
  HWstate hw(det, shutterSec, shutterPri);

  shutterPri->open();
  shutterSec->open();
  acquireMulti("",  ( ui->aqMode->currentIndex() == STEPNSHOT  &&  ! ui->checkDyno->isChecked() )  ?
                      ui->aqsPP->value() : 1);
  shutterSec->close();
  shutterPri->close();
  det->waitWritten();

  det->setAutoSave(false);
  hw.restore();
  ui->multiWidget->setEnabled(true);
  mkRun(ui->testMulti, false, butText);

}


void MainWindow::onDynoTest() {

  if (inRun(ui->testDyno)) {
    stopAll();
    return;
  }

  stopMe=false;
  const QString butText = mkRun(ui->testDyno, true, "Stop");
  ui->dynoWidget->setEnabled(false);
  det->waitWritten();
  HWstate hw(det, shutterSec, shutterPri);

  shutterPri->open();
  shutterSec->open();
  acquireDyno("");
  shutterSec->close();
  shutterPri->close();
  det->waitWritten();

  det->setAutoSave(false);
  hw.restore();
  ui->dynoWidget->setEnabled(true);
  mkRun(ui->testDyno, false, butText);
  QTimer::singleShot(0, this, SLOT(updateUi_dynoMotor()));
  QTimer::singleShot(0, this, SLOT(updateUi_dyno2Motor()));

}


void MainWindow::onDetectorTest() {

  if ( ! det->isConnected()  ||  inRun(ui->testDetector) ) {
    stopAll();
    return;
  }

  stopMe=false;
  const QString butText = mkRun(ui->testDetector, true, "Stop");
  ui->detectorWidget->setEnabled(false);
  ui->videoWidget->setEnabled(false);
  det->waitWritten();
  HWstate hw(det, shutterSec, shutterPri);

  const int nofFrames = [&](){
    if ( ui->aqMode->currentIndex() != STEPNSHOT )
      return ui->nofBGs->value();
    else if ( ui->checkDyno->isChecked() )
      return 1;
    else
      return ui->aqsPP->value();
  } () ;

  shutterPri->open();
  shutterSec->open();
  acquireDetector(det->name(uiImageFormat()) + "_SAMPLE", nofFrames);
  shutterSec->close();
  shutterPri->close();
  det->waitWritten();

  det->setAutoSave(false);
  hw.restore();
  ui->detectorWidget->setEnabled(true);
  ui->videoWidget->setEnabled(true);
  mkRun(ui->testDetector, false, butText);
  updateUi_detector();

}


static void restorePreVid() {
  if (!preVideoState)
    return;
  preVideoState->restore();
  HWstate * tmpVST = preVideoState;
  preVideoState = 0;
  delete tmpVST;
}

bool MainWindow::onVideoGetReady() {
  if (preVideoState) { // was prepared before
    if ( sender() == ui->vidReady ) { // release
      ui->vidReady->setChecked(false);
      ui->vidReady->setCheckable(false);
      restorePreVid();
    }
    return true;
  }

  if ( ! det->isConnected() )
    return false;
  preVideoState = new HWstate(det, shutterSec, shutterPri);
  const Detector::ImageFormat fmt = uiImageFormat();
  det->waitWritten();
  if (  ! det->setName(fmt, det->name(fmt) + "_VIDEO")
     || ! det->prepareForVid(fmt, ui->vidFixed->isChecked() ? ui->vidFrames->value() : 0 )
     || ! ( ui->checkExtTrig->isChecked() ? det->setHardwareTriggering(true) : true ) )
  {
    restorePreVid();
    return false;
  }
  ui->preAqScript->script->execute();
  shutterPri->open();
  shutterSec->open();
  const bool toRet = det->start();
  ui->vidReady->setCheckable(toRet);
  ui->vidReady->setChecked(toRet);
  if (!toRet) {
    shutterPri->close();
    shutterSec->close();
    restorePreVid();
  }
  return toRet;

}


void MainWindow::onVideoRecord() {

  if ( ! ui->vidStartStop->isEnabled() )
    return;
  if ( ! det->isConnected()  ||  inRun(ui->vidStartStop) ) {
    det->stopCapture();
    //restorePreVid();
    //ui->vidReady->setChecked(false);
    //ui->vidReady->setCheckable(false);
    return;
  }
  const QString butText = mkRun(ui->vidStartStop, true, "Stop");
  ui->detectorWidget->setEnabled(false);
  if (onVideoGetReady()) {
    det->startCapture();
    qtWait(250);
    det->waitCaptured();
    shutterSec->close();
    shutterPri->close();
    det->waitWritten();
  }
  ui->detectorWidget->setEnabled(true);
  ui->vidReady->setChecked(false);
  ui->vidReady->setChecked(false);
  restorePreVid();
  mkRun(ui->vidStartStop, false, butText);
  updateUi_detector();

}


void MainWindow::onStartStop() {
  if ( ! ui->startStop->isEnabled() )
    return;
  if ( inRun(ui->startStop) || ui->startStop->styleSheet() == warnStyle ) {
    stopAll();
    return;
  }
  stopMe=false;
  const QString butText = mkRun(ui->startStop, true, "Stop");
  engineRun();
  mkRun(ui->startStop, false, butText);
}


void MainWindow::stopAll() {
  stopMe=true;
  emit requestToStopAcquisition();
  det->stop();
  innearList->motui->motor()->stop();
  outerList->motui->motor()->stop();
  thetaMotor->motor()->stop();
  bgMotor->motor()->stop();
  loopList->motui->motor()->stop();
  sloopList->motui->motor()->stop();
  dynoMotor->motor()->stop();
  dyno2Motor->motor()->stop();
  foreach( Script * script, findChildren<Script*>() )
    script->stop();
}


static QString combineNames(const QString & str1, const QString & str2) {
  return ( ! str1.isEmpty()  &&  ! str2.isEmpty() )
      ?  str1 + "_" + str2  :  str1 + str2 ;
}


bool MainWindow::prepareDetector(const QString & filetemplate, int count) {
  const Detector::ImageFormat fmt = uiImageFormat();
  QString fileT = "%s%s";
  if (fmt == Detector::TIF) {
    if (count>1)
      fileT += "%0" + QString::number(QString::number(count).length()) + "d";
    fileT+= ".tif";
  } else if (fmt == Detector::HDF) {
    if (    inRun(ui->testDetector)
         || inRun(ui->vidStartStop)
         || inRun(ui->testDyno)
         || inRun(ui->testFF)
         || inRun(ui->testMulti)
         || inRun(ui->testSerial)
         || inRun(ui->testScan)
        )
      fileT+="_%4.4d";
    fileT+= ".hdf";
  }

  return
      det->isConnected() &&
      det->setNameTemplate(fmt, fileT) &&
      det->setNumber(count) &&
      det->setName(fmt, filetemplate) &&
      det->prepareForAcq(fmt, count) &&
      (    ! ui->checkExtTrig->isVisible()
        || ! ui->checkExtTrig->isChecked()
        || det->setHardwareTriggering(true) ) ;

}


int MainWindow::acquireDetector() {

  int execStatus = -1;
  execStatus = ui->preAqScript->script->execute();
  if ( execStatus || stopMe )
    goto acquireDetectorExit;
  det->acquire();
  if ( ! stopMe )
    execStatus = ui->postAqScript->script->execute();

acquireDetectorExit:
  return execStatus;
}


int MainWindow::acquireDetector(const QString & filetemplate, int count) {
  if ( ! prepareDetector(filetemplate, count) || stopMe )
    return -1;
  return acquireDetector();
}


static void setMotorSpeed(QCaMotor* mot, double speed) {
  if ( ! mot )
    return;
  if ( speed >= 0.0 &&  mot->getNormalSpeed() != speed ) {
    mot->setNormalSpeed(speed);
    mot->waitUpdated<double>
        (".VELO", speed, &QCaMotor::getNormalSpeed, 500);
  }
}


static void setMotorSpeed(QCaMotorGUI* mot, double speed) {
  setMotorSpeed(mot->motor(), speed);
}


int MainWindow::acquireDyno(const QString & filetemplate, int count) {

  int ret=-1;

  if ( ! ui->checkDyno->isChecked()  || count < 1)
    return ret;

  dynoMotor->motor()->wait_stop();
  if (stopMe) return ret;
  if ( ui->dyno2->isChecked() ) {
    dyno2Motor->motor()->wait_stop();
    if (stopMe) return ret;
  }

  ui->dynoWidget->setEnabled(false);

  const double
      dStart = dynoMotor->motor()->getUserPosition(),
      d2Start = dyno2Motor->motor()->getUserPosition(),
      dnSpeed = dynoMotor->motor()->getNormalSpeed(),
      d2nSpeed = dyno2Motor->motor()->getNormalSpeed(),
      daSpeed = ui->dynoSpeed->value(),
      d2aSpeed = ui->dyno2Speed->value();
  const QString origname = det->name(uiImageFormat());

  if (count>1) {
    ui->dynoProgress->setMaximum(count);
    ui->dynoProgress->setValue(0);
    ui->dynoProgress->setVisible(true);
  }

 QString ftemplate;

  for (int curr=0 ; curr < count ; curr++) {

    if ( dynoMotor->motor()->getUserGoal() != dStart ) {
      setMotorSpeed(dynoMotor, dnSpeed);
      dynoMotor->motor()->goUserPosition(dStart, QCaMotor::STARTED);
      if (stopMe) goto acquireDynoExit;
    }
    if ( ui->dyno2->isChecked() &&
         dyno2Motor->motor()->getUserGoal() != d2Start ) {
      setMotorSpeed(dyno2Motor, d2nSpeed);
      dyno2Motor->motor()->goUserPosition(d2Start, QCaMotor::STARTED);
      if (stopMe) goto acquireDynoExit;
    }
    dynoMotor->motor()->wait_stop();
    if (stopMe) goto acquireDynoExit;
    if ( ui->dyno2->isChecked() ) {
      dyno2Motor->motor()->wait_stop();
      if (stopMe) goto acquireDynoExit;
    }

    ftemplate = filetemplate.isEmpty() ? origname + "_" : filetemplate;
    if (count>1)
      ftemplate += QString("%1").arg(curr, QString::number(count).length(), 10, QChar('0'));
    // two stopMes below are reqiuired for the case it changes while the detector is being prepared
    if ( stopMe || ! prepareDetector(ftemplate, 1) || stopMe )
      goto acquireDynoExit;

    setMotorSpeed(dynoMotor, daSpeed);
    if ( ui->dyno2->isChecked()  )
      setMotorSpeed(dyno2Motor, d2aSpeed);
    if (stopMe) goto acquireDynoExit;

    dynoMotor->motor()->goLimit( ui->dynoPos->isChecked() ? QCaMotor::POSITIVE : QCaMotor::NEGATIVE );
    if ( ui->dyno2->isChecked() )
      dyno2Motor->motor()->goLimit( ui->dyno2Pos->isChecked() ? QCaMotor::POSITIVE : QCaMotor::NEGATIVE );
    dynoMotor->motor()->wait_start();
    if ( ui->dyno2->isChecked() )
      dyno2Motor->motor()->wait_start();
    if (stopMe) goto acquireDynoExit;

    ret = acquireDetector();

    if (count>1)
      ui->dynoProgress->setValue(curr+1);

    if ( dynoMotor->motor()->getLimitStatus() ||
         ( ui->dyno2->isChecked() && dyno2Motor->motor()->getLimitStatus() ) )
      ret = 1;
    if (stopMe) goto acquireDynoExit;

  }

acquireDynoExit:

  det->setName(uiImageFormat(), origname) ;

  setMotorSpeed(dynoMotor, dnSpeed);
  if ( dynoMotor->motor()->getUserGoal() != dStart )
    dynoMotor->motor()->goUserPosition(dStart, QCaMotor::STARTED);
  if ( ui->dyno2->isChecked() ) {
    setMotorSpeed(dyno2Motor, d2nSpeed);
    if ( dyno2Motor->motor()->getUserGoal() != d2Start )
      dyno2Motor->motor()->goUserPosition(d2Start, QCaMotor::STARTED);
  }

  ui->dynoProgress->setVisible(false);
  ui->dynoWidget->setEnabled(true);

  return ret;

}


int MainWindow::acquireMulti(const QString & filetemplate, int count) {

  if ( ! ui->checkMulti->isChecked() )
    return -1;


  QCaMotor * const lMotor = loopList->motui->motor();
  QCaMotor * const sMotor = sloopList->motui->motor();

  const bool
      moveLoop = lMotor->isConnected(),
      moveSubLoop = ui->subLoop->isChecked() && sMotor->isConnected();
  const QString ftemplate = filetemplate.isEmpty() ? det->name(uiImageFormat()) : filetemplate;
  const QString origname = det->name(uiImageFormat());

  const double lStart = lMotor->getUserPosition();
  const double sStart = sMotor->getUserPosition();

  const int
      totalLoops = loopList->ui->nof->value(),
      totalSubLoops = ui->subLoop->isChecked() ? sloopList->ui->nof->value() : 1,
      loopDigs = QString::number(totalLoops-1).size(),
      subLoopDigs = QString::number(totalSubLoops-1).size();

  const QString progressFormat = QString("Multishot progress: %p% ; %v of %m") +
      ( ui->subLoop->isChecked() ? QString(" (%1,%2 of %3,%4)").arg(1).arg(+1).arg(totalLoops).arg(totalSubLoops)
                                 : "" );
  ui->multiProgress->setFormat(progressFormat);
  ui->multiProgress->setMaximum(totalLoops*totalSubLoops);
  ui->multiProgress->setVisible(true);

  int currentLoop=0;

  if (moveLoop)
    lMotor->goUserPosition( loopList->position(0), QCaMotor::STARTED);
  if (stopMe) goto acquireMultiExit;

  if (moveSubLoop)
    sMotor->goUserPosition( sloopList->position(0), QCaMotor::STARTED);
  if (stopMe) goto acquireMultiExit;

  do { // loop

    setenv("CURRENTLOOP", QString::number(currentLoop).toLatin1(), 1);
    loopList->emphasizeRow(currentLoop);

    const QString filenameL = combineNames(ftemplate, QString("L%1").arg(currentLoop, loopDigs, 10, QChar('0')));

    if (moveLoop)
      lMotor->wait_stop();
    if (stopMe) goto acquireMultiExit;

    int currentSubLoop=0;
    do { // subloop

      setenv("CURRENTSUBLOOP", QString::number(currentSubLoop).toLatin1(), 1);
      sloopList->emphasizeRow(currentSubLoop);

      QString filename = filenameL;
      if ( totalSubLoops > 1 )
        filename = combineNames(filename, QString("S%1").arg(currentSubLoop, subLoopDigs, 10, QChar('0') ) );

      if (moveSubLoop)
        sMotor->wait_stop();
      if (stopMe) goto acquireMultiExit;

      if ( inRun(ui->startStop) )
        ui->preSubLoopScript ->script->execute();
      if (stopMe) goto acquireMultiExit;


      if ( ui->checkDyno->isChecked() )
        acquireDyno(filename, count) ;
      else
        acquireDetector(filename, count);
      if (stopMe) goto acquireMultiExit;


      currentSubLoop++;
      if (moveSubLoop && currentSubLoop < totalSubLoops)
        sMotor->goUserPosition(sloopList->position(currentSubLoop), QCaMotor::STARTED);
      if (stopMe) goto acquireMultiExit;

    } while (currentSubLoop < totalSubLoops) ;

    currentLoop++;
    if (currentLoop < totalLoops) {
      if (moveLoop)
        lMotor->goUserPosition(loopList->position(currentLoop), QCaMotor::STARTED);
      if (stopMe) goto acquireMultiExit;
      if (moveSubLoop)
        sMotor->goUserPosition(sloopList->position(0), QCaMotor::STARTED);
      if (stopMe) goto acquireMultiExit;
    }

  } while (currentLoop < totalLoops);


acquireMultiExit:


  ui->multiProgress->setVisible(false);

  det->setName(uiImageFormat(), origname) ;

  if (moveLoop) {
    lMotor->goUserPosition(lStart, QCaMotor::STARTED);
    if (!stopMe)
      lMotor->wait_stop();
  }
  if (moveSubLoop) {
    sMotor->goUserPosition(sStart, QCaMotor::STARTED);
    if (!stopMe)
      sMotor->wait_stop();
  }


  loopList->emphasizeRow();
  sloopList->emphasizeRow();

  return ! stopMe;

}


int MainWindow::moveToBG() {
  if ( ! bgMotor->motor()->isConnected() || bgMotor->motor()->isMoving() ||  ui->nofBGs->value() <1 ||  bgOrigin == bgAcquire )
    return -1;
  bgEnter = bgMotor->motor()->getUserPosition();
  bgMotor->motor()->goUserPosition( bgAcquire , QCaMotor::STARTED );
  return 1;
}


int MainWindow::acquireBG(const QString &filetemplate) {

  int ret = -1;
  const int bgs = ui->nofBGs->value();
  const double originalExposure = det->exposure();
  const QString origname = det->name(uiImageFormat());

  if ( ! bgMotor->motor()->isConnected() || bgs <1 || bgOrigin == bgAcquire )
    return ret;

  QString ftemplate = filetemplate.isEmpty()
      ?  origname + "_BG"  :  "BG_" + filetemplate;
  if ( uiImageFormat() != Detector::HDF && bgs > 1)
    ftemplate += "_";

  bgMotor->motor()->wait_stop();
  if (bgEnter == bgAcquire)
    bgEnter = bgMotor->motor()->getUserPosition();
  bgMotor->motor()->goUserPosition( bgAcquire, QCaMotor::STOPPED );
  if (stopMe) goto onBgExit;

  det->waitWritten();
  if (stopMe) goto onBgExit;

  setenv("CONTRASTTYPE", "BG", 1);

  if ( ui->bgExposure->value() != ui->bgExposure->minimum() )
    det->setExposure( ui->bgExposure->value() ) ;

  shutterSec->open();
  shutterPri->open();
  if (stopMe) goto onBgExit;

  det->setPeriod(det->exposure());
  if (ui->checkMulti->isChecked() && ! ui->singleBg->isChecked() )
    ret = acquireMulti(ftemplate, bgs);
  else if (ui->checkDyno->isChecked())
    ret = acquireDyno(ftemplate, bgs);
  else
    ret = acquireDetector(ftemplate, bgs);

onBgExit:

  if (!stopMe)
    det->waitWritten();
  det->setName(uiImageFormat(), origname) ;

  if ( ui->bgExposure->value() != ui->bgExposure->minimum() )
    det->setExposure( originalExposure ) ;

  shutterSec->close();
  bgMotor->motor()->goUserPosition ( bgEnter, QCaMotor::STARTED );
  bgEnter = bgAcquire;
  // if (!stopMe)
  //  bgMotor->motor()->wait_stop();

  return ret;

}


int MainWindow::acquireDF(const QString &filetemplate, Shutter::State stateToGo) {

  int ret = -1;
  const int dfs = ui->nofDFs->value();
  const QString origname=det->name(uiImageFormat());
  QString ftemplate;

  if ( dfs<1 )
    return 0;

  shutterSec->close();
  if (stopMe) goto onDfExit;
  det->waitWritten();
  if (stopMe) goto onDfExit;

  ftemplate = filetemplate.isEmpty()
      ?  origname + "_DF"  :  "DF_" + filetemplate;
  if ( uiImageFormat() != Detector::HDF && dfs > 1)
    ftemplate += "_";

  setenv("CONTRASTTYPE", "DF", 1);

  det->setPeriod(det->exposure());

  ret = acquireDetector(ftemplate, dfs);
  if (stopMe) goto onDfExit;

  if (stateToGo == Shutter::OPEN)
    shutterSec->open();
  else if (stateToGo == Shutter::CLOSED)
    shutterSec->close();
  if ( ! stopMe && shutterSec->state() != stateToGo)
    qtWait(shutterSec, SIGNAL(stateUpdated(State)), 500); /**/


onDfExit:

  if (!stopMe)
    det->waitWritten();
  det->setName(uiImageFormat(), origname) ;

  return ret;

}












/*/////////////////////////// Engine ///////////////////////////*/

void MainWindow::engineRun () {


  QCaMotor * const inSMotor = innearList->motui->motor();
  QCaMotor * const outSMotor = outerList->motui->motor();

  const QString origname = det->name(uiImageFormat());
  HWstate hw(det, shutterSec, shutterPri);

  bgOrigin = bgMotor->motor()->getUserPosition();
  bgAcquire = bgOrigin + ui->bgTravel->value();
  bgEnter = bgAcquire;

  const double
      inSerialStart = inSMotor->getUserPosition(),
      outSerialStart = outSMotor->getUserPosition(),
      thetaStart = thetaMotor->motor()->getUserPosition(),
      thetaRange = ui->scanRange->value(),
      thetaSpeed = thetaMotor->motor()->getNormalSpeed();

  const bool
      doSerial1D = ui->checkSerial->isChecked(),
      doSerial2D = doSerial1D && ui->serial2D->isChecked(),
      doFF = inRun(ui->startStop) && ! inRun(ui->testScan) && ui->checkFF->isChecked(),
      doBG = doFF && ui->nofBGs->value(),
      doDF = doFF && ui->nofDFs->value(),
      sasMode = inRun(ui->startStop)  &&  ui->aqMode->currentIndex() == STEPNSHOT,
      ongoingSeries = doSerial1D && ui->ongoingSeries->isChecked(),
      doTriggCT = ( inRun(ui->testScan) || inRun(ui->startStop) ) && ! tct->prefix().isEmpty(),
      splitData = uiImageFormat() != Detector::HDF;

  const int
      bgInterval = ui->bgIntervalSAS->value(),
      dfInterval = ui->dfIntervalSAS->value(),
      scanDelay = QTime(0, 0, 0, 0).msecsTo( ui->scanDelay->time() ),
      scanTime = QTime(0, 0, 0, 0).msecsTo( ui->acquisitionTime->time() ),
      totalProjections = ui->scanProjections->value(),
      projectionDigs = QString::number(totalProjections).size(),
      doAdd = ui->scanAdd->isChecked() ? 1 : 0,
      totalScans1D = doSerial1D ?
        ( ui->endNumber->isChecked() ? outerList->ui->nof->value() : 0 ) : 1 ,
      totalScans2D = doSerial2D ? innearList->ui->nof->value() : 1 ,
      series1Digs = QString::number(totalScans1D-1).size(),
      series2Digs = QString::number(totalScans2D-1).size();

  int
      currentScan1D = 0,
      currentScan2D = 0,
      currentScan = 0,
      beforeBG = 0,
      beforeDF = 0;


  foreach(PositionList * lst, findChildren<PositionList*>())
    lst->freezList(true);
  if ( inRun(ui->startStop) ) {
    foreach(QWidget * tab, ui->control->tabs())
      tab->setEnabled(false);
  }
  QElapsedTimer inCTtime;
  inCTtime.start();
  bool timeToStop=false;

  if ( totalScans1D * totalScans2D > 1 ) {
    ui->serialProgress->setMaximum(totalScans1D * totalScans2D);
    ui->serialProgress->setFormat("Series progress. %p% (scans complete: %v of %m)");
    ui->serialProgress->setValue(currentScan);
  } else if ( ui->endTime->isChecked() ) {
    ui->serialProgress->setMaximum(QTime(0, 0, 0, 0).msecsTo( ui->acquisitionTime->time() ));
    QString format = "Series progress: %p% "
        + (QTime(0, 0, 0, 0).addMSecs(inCTtime.elapsed())).toString() + " of " + ui->acquisitionTime->time().toString()
        + " (scans complete: " + QString::number(currentScan) + ")";
    ui->serialProgress->setFormat(format);
    ui->serialProgress->setValue(inCTtime.elapsed());
  } else {
    ui->serialProgress->setMaximum(0);
    ui->serialProgress->setFormat("Series progress. Scans complete: %v.");
    ui->serialProgress->setValue(currentScan);
  }
  ui->serialProgress->setVisible(true);


  QProcess logProc(this);
  QTemporaryFile logExec(this);
  if ( inRun(ui->startStop) ) {

    QString cfgName;
    QString logName;
    int attempt=0;
    do {
      cfgName = "acquisition." + QString::number(attempt) + ".configuration";
      logName = "acquisition." + QString::number(attempt) + ".log";
      attempt++;
    } while ( QFile::exists(cfgName) || QFile::exists(logName) ) ;

    if ( attempt > 1  &&  QMessageBox::No == QMessageBox::question
         (this, "Overwrite warning"
          , "Current directory seems to contain earlier scans: configuration and/or log files are present.\n\n"
            " Existing data may be overwritten.\n"
            " Do you want to proceed?\n"
          , QMessageBox::Yes | QMessageBox::No, QMessageBox::No)  )
      goto onEngineExit;

    saveConfiguration(cfgName);

    logProc.setWorkingDirectory(QDir::currentPath());
    if ( ! logExec.open() )
      qDebug() << "ERROR! Unable to open temporary file for log proccess.";
    logExec.write( (  QCoreApplication::applicationDirPath().remove( QRegExp("bin/*$") )
                      + "/libexec/ctgui.log.sh"
                      + " " + det->pv()
                      + " " + thetaMotor->motor()->getPv()
                      + " >> " + logName ).toLatin1() );
    logExec.flush();
    logProc.start( "/bin/sh", QStringList() << logExec.fileName() );

  }

  if (   ui->endNumber->isChecked()
     && (  ( doSerial1D  && !  outerList->doAll() )
        || ( doSerial2D  && ! innearList->doAll() ) )
     && QMessageBox::No == QMessageBox::question
         (this, "Skippins scans",
          "Some scans in the series are marked to be skipped.\n\n"
          " Do you want to proceed?\n"
          , QMessageBox::Yes | QMessageBox::No, QMessageBox::No)  )
    goto onEngineExit;

  if ( doSerial1D  &&  ui->endNumber->isChecked() ) {
    currentScan1D = outerList->nextToDo();
    if ( currentScan1D < 0 ) {
      qDebug() << "No position selected in innear list.";
      goto onEngineExit;
    }
    if ( outSMotor->isConnected() )
      outSMotor->goUserPosition( outerList->position(currentScan1D), QCaMotor::STARTED);
  }
  if (stopMe) goto onEngineExit;

  if ( doSerial2D ) {
    if (ui->endNumber->isChecked()) {
      currentScan2D = innearList->nextToDo();
      if ( currentScan2D < 0 ) {
        qDebug() << "No position selected in innear list.";
        goto onEngineExit;
      }
    }
    if ( inSMotor->isConnected() )
      inSMotor->goUserPosition( innearList->position(currentScan2D), QCaMotor::STARTED);
  }
  if (stopMe) goto onEngineExit;

  if (doTriggCT) {
    const double stepSize = thetaRange / ui->scanProjections->value();
    tct->setStartPosition(thetaStart, true);
    tct->setStep(stepSize, true);
    tct->setRange( thetaRange + doAdd*stepSize , true);
    qtWait(500); // otherwise NofTrigs are set, but step is not recalculated ...((    int digs=0;
    tct->setNofTrigs(totalProjections + doAdd, true);
    if (det->camera() == Detector::Camera::Eiger)
      tct->setExposure(det->exposure());
    else
      tct->setExposure();
    if (stopMe) goto onEngineExit;
  }


  if ( inRun(ui->startStop) ) {
    setenv("INEXPERIMENT", "YES", 1);
    ui->preRunScript->script->execute();
  }
  if (stopMe) goto onEngineExit;





  do { // serial scanning 1D

    setenv("CURRENTOSCAN", QString::number(currentScan1D).toLatin1(), 1);
    outerList->emphasizeRow(currentScan1D);

    QString sn1, sn2;
    if ( ! totalScans1D )
      sn1 = "Y" + QString::number(currentScan1D);
    else if (totalScans1D > 1)
      sn1 = QString("Y%1").arg(currentScan1D, series1Digs, 10, QChar('0') );
    else
      sn1 = QString();

    if (doSerial1D  && outSMotor->isConnected())
      outSMotor->wait_stop();
    if (stopMe) goto onEngineExit;

    currentScan2D=0;
    if ( doSerial2D ) {
      if (ui->endNumber->isChecked()) {
        currentScan2D = innearList->nextToDo();
        if ( currentScan2D < 0 ) {
          qDebug() << "No position selected in innear list.";
          goto onEngineExit;
        }
      }
      if ( inSMotor->isConnected() )
        inSMotor->goUserPosition( innearList->position(currentScan2D), QCaMotor::STARTED);
      if (stopMe) goto onEngineExit;
    }

    if(doSerial2D)
      ui->pre2DScript->script->execute();
    if (stopMe) goto onEngineExit;





    do { // serial scanning 2D

      sn2 = (totalScans2D <= 1)  ?
            sn1  :  combineNames(sn1, QString("Z%1").arg(currentScan2D, series2Digs, 10, QChar('0') ) );

      QString sampleName;
      if ( inRun(ui->testScan) )
        sampleName = origname + "_SAMPLE";
      else if ( inRun(ui->testSerial) )
        sampleName = combineNames( origname + "_SAMPLE", sn2);
      else
        sampleName = combineNames("SAMPLE", sn2);

      setenv("CURRENTISCAN", QString::number(currentScan2D).toLatin1(), 1);
      setenv("CURRENTSCAN", QString::number(currentScan).toLatin1(), 1);
      innearList->emphasizeRow(currentScan2D);

      if (doSerial2D  && inSMotor->isConnected())
        inSMotor->wait_stop();
      if (stopMe) goto onEngineExit;

      if ( inRun(ui->startStop) )
        ui->preScanScript->script->execute();
      if (stopMe) goto onEngineExit;





      if ( inRun(ui->testSerial) ) { // test series




        const QString projectionName = sampleName + ( splitData
                                 ? QString("_T%1").arg(0, projectionDigs, 10, QChar('0') )
                                 :  QString() );
        acquireDetector(projectionName, 1);
        if (stopMe) goto onEngineExit;





      } else if ( sasMode ) { // STEP-AND-SHOT





        int currentProjection = 0;
        if ( ! ongoingSeries ) {
          beforeBG=0;
          beforeDF=0;
        }

        ui->scanProgress->setMaximum(totalProjections + ( ui->scanAdd->isChecked() ? 1 : 0 ));
        ui->scanProgress->setValue(currentProjection);
        ui->scanProgress->setVisible(true);


        do {

          setenv("CURRENTPROJECTION", QString::number(currentProjection).toLatin1(), 1);

          QString projectionName = sampleName + QString("T%1").arg(currentProjection, projectionDigs, 10, QChar('0') );

          QString bgdfN = combineNames(sn2, "BEFORE");
          if (doBG && ! beforeBG)
            moveToBG();
          if (doDF && ! beforeDF) {
            acquireDF(bgdfN, Shutter::CLOSED);
            beforeDF = dfInterval;
            if (stopMe) goto onEngineExit;
          }
          beforeDF--;
          if (doBG && ! beforeBG) {
            acquireBG(bgdfN);
            beforeBG = bgInterval;
            if (stopMe) goto onEngineExit;
          }
          beforeBG--;

          thetaMotor->motor()->wait_stop();
          if (stopMe) goto onEngineExit;
          bgMotor->motor()->wait_stop();
          if (stopMe) goto onEngineExit;

          det->waitWritten();
          setenv("CONTRASTTYPE", "SAMPLE", 1);
          if (stopMe) goto onEngineExit;

          shutterPri->open();
          shutterSec->open();
          if (stopMe) goto onEngineExit;

          if (ui->checkMulti->isChecked())
            acquireMulti(projectionName, ui->aqsPP->value());
          else if (ui->checkDyno->isChecked())
            acquireDyno(projectionName);
          else
            acquireDetector(projectionName, ui->aqsPP->value());
          if (stopMe) goto onEngineExit;

          currentProjection++;

          double motorTarget = thetaStart + thetaRange * currentProjection / totalProjections;
          if (currentProjection < totalProjections + doAdd)
            thetaMotor->motor()->goUserPosition( motorTarget + ( ongoingSeries ? thetaRange * currentScan : 0) , QCaMotor::STARTED );
          if (stopMe) goto onEngineExit;

          ui->scanProgress->setValue(currentProjection);


        } while ( currentProjection < totalProjections + doAdd );



        QString bgdfN = combineNames(sn2, "AFTER");
        if (doBG && ! beforeBG)
          moveToBG();
        if ( doDF && ! beforeDF ) {
          acquireDF(bgdfN, Shutter::CLOSED);
          if (stopMe) goto onEngineExit;
          beforeDF = dfInterval;
        }
        if (doBG && ! beforeBG) {
          acquireBG(bgdfN);
          if (stopMe) goto onEngineExit;
          beforeBG = bgInterval;
        }



      } else { // CONTINIOUS




        QString bgdfN = combineNames(sn2, "BEFORE");
        if ( doBG  && ui->bgIntervalBefore->isChecked() && (ui->ffOnEachScan->isChecked() || ! currentScan ) )
          moveToBG();
        if ( doDF && ui->dfIntervalBefore->isChecked() && (ui->ffOnEachScan->isChecked() || ! currentScan ) ) {
          acquireDF(bgdfN, Shutter::CLOSED);
          if (stopMe) goto onEngineExit;
        }
        if ( doBG  && ui->bgIntervalBefore->isChecked() && (ui->ffOnEachScan->isChecked() || ! currentScan ) ) {
          acquireBG(bgdfN);
          if (stopMe) goto onEngineExit;
        }

        const double
            speed = thetaRange * ui->flyRatio->value() / ( det->exposure() * totalProjections ),
            accTime = thetaMotor->motor()->getAcceleration(),
            accTravel = speed * accTime / 2,
            rotDir = copysign(1,thetaRange),
            addTravel = 2*rotDir*accTravel + (doTriggCT ? 2.0 : 0.0);
        // coefficient 2 in the above string is required to guarantee that the
        // motor has accelerated to the normal speed before the acquisition starts.


        det->waitWritten();
        if (stopMe) goto onEngineExit;

        prepareDetector(sampleName + (splitData ? "_T" : ""), totalProjections + doAdd);
        if (stopMe) goto onEngineExit;

        if (doTriggCT || ui->checkExtTrig->isChecked() ) {
          det->setHardwareTriggering(true);
          det->setPeriod(det->exposure());
        } else {
          det->setPeriod( qAbs(thetaRange) / (totalProjections * speed)  ) ;
        }

        if ( ! ongoingSeries || ! currentScan || doTriggCT ) {

          bgMotor->motor()->wait_stop();
          thetaMotor->motor()->wait_stop();
          if (doTriggCT)
            tct->stop(false);
          if (stopMe) goto onEngineExit;

          thetaMotor->motor()->goRelative( -addTravel, QCaMotor::STOPPED);
          if (stopMe) goto onEngineExit;

          setMotorSpeed(thetaMotor, speed);
          if (stopMe) goto onEngineExit;

        }

        setenv("CONTRASTTYPE", "SAMPLE", 1);
        shutterPri->open();
        shutterSec->open();
        if (stopMe) goto onEngineExit;
        ui->preAqScript->script->execute();
        if (stopMe) goto onEngineExit;

        if ( ! doTriggCT && ( currentScan || ! ongoingSeries ) ) {
            thetaMotor->motor()-> /* goRelative( ( thetaRange + addTravel ) * 1.05 ) */
                goLimit( thetaRange > 0 ? QCaMotor::POSITIVE : QCaMotor::NEGATIVE );
            if (stopMe) goto onEngineExit;
            // accTravel/speed in the below string is required to compensate
            // for the coefficient 2 in addTravel
            // qtWait(1000*(accTime + accTravel/speed));
            usleep(1000000*(accTime + accTravel/speed));
            if (stopMe) goto onEngineExit;
        }


        QList<ObjSig> stopSignals;
        stopSignals << ObjSig(det, SIGNAL(done()));
        stopSignals << ObjSig(thetaMotor->motor(), SIGNAL(stopped()));
        if (doTriggCT) {
          tct->start(true);
          stopSignals << ObjSig(tct, SIGNAL(stopped()));
          thetaMotor->motor()->goLimit( thetaRange > 0 ? QCaMotor::POSITIVE : QCaMotor::NEGATIVE );
        }
        if (stopMe) goto onEngineExit;

        det->start();

        qtWait( stopSignals );
        if (   det->number() != totalProjections + doAdd
            || (doTriggCT && tct->isRunning())
            || ! thetaMotor->motor()->isMoving()  ) {
          ui->statusBar->showMessage("Unexpected condition during last experiment was observed. Check data.");
          //goto onEngineExit;
        }

        if (stopMe) goto onEngineExit;
        if (det->isAcquiring()) {
          qtWait( det, SIGNAL(done()), 1000 );
          det->stop();
        }
        if (stopMe) goto onEngineExit;
        ui->postAqScript->script->execute();
        if (stopMe) goto onEngineExit;
        shutterSec->close();
        if (stopMe) goto onEngineExit;
        if (doTriggCT)
          tct->stop(false);
        if (stopMe) goto onEngineExit;



        if ( ! ongoingSeries ) {
          bgdfN = combineNames(sn2, "AFTER");
          if ( doBG  && ui->bgIntervalAfter->isChecked() && ui->ffOnEachScan->isChecked() )
            moveToBG();
          if ( doDF && ui->dfIntervalAfter->isChecked() && ui->ffOnEachScan->isChecked() )
            acquireDF(bgdfN, Shutter::CLOSED);
          if (stopMe) goto onEngineExit;
          if ( doBG  && ui->bgIntervalAfter->isChecked() && ui->ffOnEachScan->isChecked() )
            acquireBG(bgdfN);
          if (stopMe) goto onEngineExit;
        }




      } // Cont / SaS / Test





      if ( inRun(ui->startStop) )
        ui->postScanScript->script->execute();
      if (stopMe) goto onEngineExit;

      if ( ! ongoingSeries ) {
        thetaMotor->motor()->stop(QCaMotor::STOPPED);
        if (stopMe) goto onEngineExit;
        setMotorSpeed(thetaMotor, thetaSpeed);
        thetaMotor->motor()->goUserPosition( thetaStart, QCaMotor::STARTED );
        if (stopMe) goto onEngineExit;
      }

      if (doSerial2D) {
        innearList->done(currentScan2D);
        if (ui->endNumber->isChecked())
          currentScan2D = innearList->nextToDo();
        else
          currentScan2D++;
      }
      currentScan++;

      if ( ui->endTime->isChecked() )
        ui->serialProgress->setValue(inCTtime.elapsed());
      else
        ui->serialProgress->setValue(currentScan);

      timeToStop = inRun(ui->testScan)
          || ! doSerial2D
          || currentScan2D >= totalScans2D
          || ( ui->endNumber->isChecked() && currentScan2D < 0 )
          || ( ui->endTime->isChecked() &&  inCTtime.elapsed() + scanDelay >= scanTime )
          || ( ui->endCondition->isChecked()  &&  ui->conditionScript->script->execute() );

      if ( ! timeToStop ) {
        if (doSerial2D  && inSMotor->isConnected())
          inSMotor->goUserPosition( innearList->position(currentScan2D), QCaMotor::STARTED);
        if (stopMe) goto onEngineExit;
        qtWait(this, SIGNAL(requestToStopAcquisition()), scanDelay);
        if (stopMe) goto onEngineExit;
      } else {
        if (doSerial2D)
          innearList->todo();
      }


    } while ( ! timeToStop ); // innear loop





    if(doSerial2D)
      ui->post2DScript->script->execute();
    if (stopMe) goto onEngineExit;
    if (doSerial1D) {
      outerList->done(currentScan1D);
      if (ui->endNumber->isChecked())
        currentScan1D = outerList->nextToDo();
      else
        currentScan1D++;
    }

    timeToStop = inRun(ui->testScan)
        || ! doSerial1D
        || ( ui->endNumber->isChecked()  &&  currentScan1D < 0 )
        || ( ui->endTime->isChecked()  &&  inCTtime.elapsed() + scanDelay >= scanTime )
        || ( ui->endCondition->isChecked()  &&  ui->conditionScript->script->execute() );

    if ( ! timeToStop ) {
      if ( doSerial1D  &&  ui->endNumber->isChecked()  &&  outSMotor->isConnected() )
        outSMotor->goUserPosition(outerList->position(currentScan1D), QCaMotor::STARTED);
      if ( doSerial2D  &&  inSMotor->isConnected() )
        inSMotor->goUserPosition( innearList->position(0), QCaMotor::STARTED);
      if (stopMe) goto onEngineExit;
      qtWait(this, SIGNAL(requestToStopAcquisition()), scanDelay);
      if (stopMe) goto onEngineExit;
    } else {
      if (doSerial1D)
        outerList->todo();
    }

    if ( timeToStop && ! sasMode && ! ui->ffOnEachScan->isChecked() ) {
      QString bgdfN = combineNames(sn2, "AFTER");
      if ( doBG && ui->bgIntervalAfter->isChecked() )
        moveToBG();
      if ( doDF && ui->dfIntervalAfter->isChecked() ) {
        acquireDF(bgdfN, Shutter::CLOSED);
        if (stopMe) goto onEngineExit;
      }
      if ( doBG && ui->bgIntervalAfter->isChecked() ) {
        acquireBG(bgdfN);
        if (stopMe) goto onEngineExit;
      }
    }




  } while ( ! timeToStop );
  if (stopMe) goto onEngineExit;


  if ( inRun(ui->startStop) )
    ui->postRunScript->script->execute();


onEngineExit:

  setenv("INEXPERIMENT", "NO", 1);
  ui->scanProgress->setVisible(false);
  ui->serialProgress->setVisible(false);

  shutterPri->close();
  shutterSec->close();

  thetaMotor->motor()->stop(QCaMotor::STOPPED);
  setMotorSpeed(thetaMotor, thetaSpeed);
  thetaMotor->motor()->goUserPosition(thetaStart, QCaMotor::STARTED);

  if ( doBG ) {
    bgMotor->motor()->stop(QCaMotor::STOPPED);
    bgMotor->motor()->goUserPosition(bgOrigin, QCaMotor::STARTED);
  }
  if (doSerial1D && outSMotor->isConnected()) {
    outSMotor->stop(QCaMotor::STOPPED);
    outSMotor->goUserPosition(outSerialStart, QCaMotor::STARTED);
  }
  if (doSerial2D && inSMotor->isConnected()) {
    inSMotor->stop(QCaMotor::STOPPED);
    inSMotor->goUserPosition(inSerialStart, QCaMotor::STARTED);
  }

  det->stop();
  det->waitWritten();
  det->setAutoSave(false);
  if (doTriggCT)
    tct->stop(false);
  hw.restore();

  if ( inRun(ui->startStop) ) {
    Script::executeOnce("killall -9 camonitor");
    //Script::executeOnce("kill -9 $(ps --no-headers --ppid $$ | grep camonitor | sed 's/^ *//g' | cut -d' ' -f 1) 2> /dev/null");
    //Script::executeOnce("kill -9 $(pstree -a  -p " + QString::number(logProc.processId())
    //                    + " | grep -v '{' | grep camonitor | sed 's .*camonitor,\\([0-9]*\\).* \\1 g')");
    logProc.terminate();
    logExec.close();
  }

  QTimer::singleShot(0, this, SLOT(updateUi_thetaMotor()));
  QTimer::singleShot(0, this, SLOT(updateUi_bgMotor()));
  QTimer::singleShot(0, this, SLOT(updateUi_dynoMotor()));
  QTimer::singleShot(0, this, SLOT(updateUi_dyno2Motor()));
  QTimer::singleShot(0, this, SLOT(updateUi_detector()));
  QTimer::singleShot(0, this, SLOT(updateUi_serials()));
//  QTimer::singleShot(0, this, SLOT(updateUi_shutterStatus()));
//  QTimer::singleShot(0, this, SLOT(updateUi_expPath()));

  foreach(PositionList * lst, findChildren<PositionList*>())
    lst->freezList(false);
  foreach(QWidget * tab, ui->control->tabs())
        tab->setEnabled(true);
  outerList->emphasizeRow();
  innearList->emphasizeRow();

}



