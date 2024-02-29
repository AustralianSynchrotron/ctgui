#include <QApplication>
#include "ctgui_mainwindow.h"
#include <poptmx.h>
#include <QTimeEdit>
#include <QButtonGroup>
#include <QDebug>
#include <memory>

using namespace std;


//QString QString(const string & str) {
//  return QString::fromStdString(str);
//}

std::string type_desc (QString*) {return "QString";};
int _conversion (QString* _val, const string & in) {
  * _val = QString::fromStdString(in);
  return 1;
}

std::string type_desc (QVariant* _val) {
  return _val->typeName();
};
int _conversion (QVariant* _val, const string & in) {
  * _val = QString::fromStdString(in);
  return 1;
}

std::string type_desc (QObject* obj) {
  if (!obj) {
    qDebug() << "Zero QObject";
    return "";
  } else if (qobject_cast<const QLineEdit*>(obj))
    return "string";
  else if (qobject_cast<const QPlainTextEdit*>(obj))
    return "string";
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
    return "PV";
  else if (qobject_cast<const QButtonGroup*>(obj))
    return "enum";
  else if (qobject_cast<const QTableWidgetOtem*>(obj))
    return "list";
  else
    qDebug() << "Cannot add value of object" << obj
             << "into cli arguments: unsupported type" << obj->metaObject()->className();
  return obj->metaObject()->className();
};



int _conversion (QObject* obj, const string & in) {
  const QString sval = QString::fromStdString(in);
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
    if (list.size() != rows)
      qDebug() << "Warning. List size" << list.size() << "produced from string" << sval
               << "is not same as table size" << rows;
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

QString value (QObject* obj) {
  auto quoteNonEmpty = [](const QString & str){ return str.isEmpty() ? "" : "'" + str + "'"; };
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
    return QString::number(cobj->value())
        + ( cobj->specialValueText() == cobj->text() ? " (" + cobj->text() + ")" : "" );
  else if (QDoubleSpinBox* cobj = qobject_cast<QDoubleSpinBox*>(obj))
    return QString::number(cobj->value())
        + ( cobj->specialValueText() == cobj->text() ? " (" + cobj->text() + ")" : "" );
  else if (QTimeEdit* cobj = qobject_cast<QTimeEdit*>(obj))
    return cobj->time().toString()
        + ( cobj->specialValueText() == cobj->text() ? " (" + cobj->text() + ")" : "" );
  else if (QLabel* cobj = qobject_cast<QLabel*>(obj))
    return quoteNonEmpty(cobj->text());
  else if (UScript* cobj = qobject_cast<UScript*>(obj))
    return quoteNonEmpty(cobj->script->path());
  else if (QCaMotorGUI* cobj = qobject_cast<QCaMotorGUI*>(obj))
    return cobj->motor()->getPv();
  else if (QButtonGroup* cobj = qobject_cast<QButtonGroup*>(obj))
    return cobj->checkedButton() ? "'" + cobj->checkedButton()->text() +  "'" : "";
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
    return "["+list.join(',')+"]";
  } else
    qDebug() << "Cannot get default value of object" << obj
             << ": unsupported type" << obj->metaObject()->className();
  return "";
}


int main(int argc, char *argv[]) {

  QApplication a(argc, argv);
  MainWindow w;

  QString configFile = MainWindow::storedState;
  QStringList names = w.configNames.values();
  names.sort();
  int moveTo = 0;
  for (int idx=0 ; idx < names.count() ; idx++ )
    if ( ! names.at(idx).contains("/") )
      names.move(idx,moveTo++);

  { // parse and load configuration file
    poptmx::OptionTable otable("this is fake option table", "just to get config file");
    otable.add(poptmx::ARGUMENT, &configFile, "configuration", "Configuration file.", "");
    string fakeval;
    foreach (QString name,  names ) {
      QObject * obj = 0;
      foreach (QObject * mobj, w.configNames.keys())
        if ( w.configNames[mobj] == name )
          obj = mobj;
      const string sname = (name.contains('/') ? "" : "general/") + name.toStdString();
      if (dynamic_cast<const PositionList*>(obj))
        otable
            .add(poptmx::OPTION, &fakeval, 0, sname+"/motor", "shortDesc", "")
            .add(poptmx::OPTION, &fakeval, 0, sname+"/nofsteps", "shortDesc", "")
            .add(poptmx::OPTION, &fakeval, 0, sname+"/step", "shortDesc", "")
            .add(poptmx::OPTION, &fakeval, 0, sname+"/irregular", "shortDesc", "")
            .add(poptmx::OPTION, &fakeval, 0, sname+"/positions", "shortDesc", "")
            .add(poptmx::OPTION, &fakeval, 0, sname+"/todos", "shortDesc", "");
      else
        otable.add(poptmx::OPTION, &fakeval, 0, sname, "shortDesc", "");
    }
    bool abool; // to emulate standard options
    otable.add(poptmx::OPTION, &abool, 'h', "help", "short", "long", "def");
    otable.add(poptmx::OPTION, &abool, 'u', "usage", "short", "long", "def");
    otable.add(poptmx::OPTION, &abool, 'v', "verbose", "short", "long", "def");
    otable.parse(argc,argv);
    w.loadConfiguration(configFile);
  }

  // Only after loading configFile, I prepare and parse argv properly.
  bool beverbose=false;
  poptmx::OptionTable otable("CT acquisition", "Executes CT experiment.");
  otable
      .add(poptmx::NOTE, "ARGUMENTS:")
      .add(poptmx::ARGUMENT, &configFile, "configuration", "Configuration file.",
           "Ini file to be loaded on start.", configFile.toStdString())
      .add(poptmx::NOTE, "OPTIONS:");
  foreach (QString name,  names ) {

    const string sname = (name.contains('/') ? "" : "general/") + name.toStdString();
    QObject * obj = 0;
    foreach (QObject * mobj, w.configNames.keys())
      if ( w.configNames[mobj] == name )
        obj = mobj;

    auto addOpt = [&otable,&sname]
        (QObject * sobj, const string & postName="", const string & preShort="", const string & preLong="" )
    {
      string shortDesc;
      string longDesc;
      // adding option name to toolTip and getting descriptions
      QString ttAdd = QString::fromStdString( "\n\n--" + sname + postName );
      if (const QButtonGroup* cobj = dynamic_cast<const QButtonGroup*>(sobj)) {
        QStringList knownFields;
        foreach (QAbstractButton * but, cobj->buttons()) {
          but->setToolTip(but->toolTip() + ttAdd + " \"" + but->text() + "\"");
          knownFields << "'" + but->text() + "'";
        }
        longDesc = "Possible values: " + knownFields.join(", ").toStdString() + "." ;
        if ( sobj->property("toolTip").isValid() )
          longDesc += "\n" + sobj->property("toolTip").toString().toStdString();
        if ( sobj->property("whatsThis").isValid() )
          shortDesc = sobj->property("whatsThis").toString().toStdString();
      } else {

        if (QComboBox* cobj = qobject_cast<QComboBox*>(sobj)) {
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

        variant<QWidget*, QTableWidgetOtem*> ctrlWdg = (QWidget*) nullptr;
        if (QCaMotorGUI* cobj = dynamic_cast<QCaMotorGUI*>(sobj))
          ctrlWdg = cobj->setupButton();
        else if (Shutter* cobj = dynamic_cast<Shutter*>(sobj))
          ctrlWdg = cobj->ui->selection;
        else if (QWidget * wdg = dynamic_cast<QWidget*>(sobj))
          ctrlWdg = wdg;
        else if (QTableWidgetOtem * item = dynamic_cast<QTableWidgetOtem*>(sobj))
          ctrlWdg = item;
        shortDesc = visit( [](auto &x) { return x->whatsThis(); }, ctrlWdg).toStdString();
        longDesc += visit( [](auto &x) { return x->toolTip(); }, ctrlWdg).toStdString();
        if (shortDesc.empty()) {
          shortDesc = longDesc;
          longDesc.clear();
        }
        ttAdd += " " + QString::fromStdString(type_desc(sobj));
        visit( [&ttAdd](auto &x) { x->setToolTip( x->toolTip() + ttAdd ); }, ctrlWdg);
      }
      // adding option to parse table
      string svalue = value(sobj).toStdString();
      otable.add( poptmx::OPTION, sobj, 0, sname+postName
                , preShort + shortDesc, preLong + longDesc, svalue.empty() ? "<none>" : svalue);
    };

    if (!obj)
      qDebug() << "Can't find QObject associated with config name" <<  name << "report to developper.";
    else if (PositionList* cobj = qobject_cast<PositionList*>(obj)) {
      const string listName = name.split('/').last().toStdString() + ": ";
      addOpt(cobj->motui, "/motor", listName + "Motor PV", listName + "EPICS PV of the motor record.");
      addOpt(cobj->ui->nof, "/nofsteps", listName, listName);
      addOpt(cobj->ui->step, "/step", listName, listName);
      addOpt(cobj->ui->irregular, "/irregular", listName, listName);
      addOpt( dynamic_cast<QTableWidgetOtem*>(cobj->ui->list->horizontalHeaderItem(0))
            , "/positions", listName, listName);
      addOpt( dynamic_cast<QTableWidgetOtem*>(cobj->ui->list->horizontalHeaderItem(3))
            , "/todos", listName, listName);
    } else if (Shutter* cobj = qobject_cast<Shutter*>(obj))
      addOpt(cobj->ui->selection);
    else
      addOpt(obj);

  }
  otable.add_standard_options(&beverbose);
  if ( ! otable.parse(argc,argv) )
    exit(0);

  w.show();
  return a.exec();

}
