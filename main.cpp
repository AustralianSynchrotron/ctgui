#include <QApplication>
#include "ctgui_mainwindow.h"
#include <poptmx.h>
#include <QTimeEdit>
#include <QButtonGroup>
#include <QDebug>


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








int main(int argc, char *argv[]) {

  QApplication a(argc, argv);
  MainWindow w;

  bool beverbose=false;
  QString configFile = MainWindow::storedState;
  QStringList names = w.configNames.values();
  names.sort();
  int moveTo = 0;
  for (int idx=0 ; idx < names.count() ; idx++ )
    if ( ! names.at(idx).contains("/") )
      names.move(idx,moveTo++);
  QHash<QObject*, QVariant> configVals;

  auto optionsParser = [&] (bool updateToolTips=false) {

    configVals.clear();
    poptmx::OptionTable otable("CT acquisition", "Executes CT experiment.");
    otable
        .add(poptmx::NOTE, "ARGUMENTS:")
        .add(poptmx::ARGUMENT, &configFile, "configuration", "Configuration file.",
             "Ini file to be loaded on start.", configFile.toStdString())
        .add(poptmx::NOTE, "OPTIONS:");
    foreach (QString name,  names ) {

      QObject * obj = 0;
      foreach (QObject * mobj, w.configNames.keys())
        if ( w.configNames[mobj] == name )
          obj = mobj;
      QString shortDesc, longDesc;
      if (const QWidget* wdg = dynamic_cast<const QWidget*>(obj)) {
        shortDesc = wdg->whatsThis();
        longDesc = wdg->toolTip();
      }

      QVariant rval;
      if (!obj)
        qDebug() << "Can't find QObject associated with config name" <<  name << "report to developper.";
      else if (const QLineEdit* cobj = dynamic_cast<const QLineEdit*>(obj) )
        rval.setValue(cobj->text());
      else if (const QPlainTextEdit* cobj = dynamic_cast<const QPlainTextEdit*>(obj))
        rval.setValue(cobj->toPlainText());
      else if (const QAbstractButton* cobj = dynamic_cast<const QAbstractButton*>(obj))
        rval.setValue(cobj->isChecked());
      else if (const QComboBox* cobj = dynamic_cast<const QComboBox*>(obj)){
        rval.setValue(cobj->currentText());
        QString knownFields;
        for (int cidx=0 ; cidx < cobj->count() ; cidx++ )
          knownFields += "'" + cobj->itemText(cidx) + "' ";
        longDesc = "Possible values: " + knownFields + ". " + longDesc;
      } else if (const QSpinBox* cobj = dynamic_cast<const QSpinBox*>(obj))
        rval.setValue(cobj->value());
      else if (const QDoubleSpinBox* cobj = dynamic_cast<const QDoubleSpinBox*>(obj))
        rval.setValue(cobj->value());
      else if (const QTimeEdit* cobj = dynamic_cast<const QTimeEdit*>(obj))
        rval.setValue(cobj->time());
      else if (const QLabel* cobj = dynamic_cast<const QLabel*>(obj))
        rval.setValue(cobj->text());
      else if (const UScript* cobj = dynamic_cast<const UScript*>(obj))
        rval.setValue(cobj->script->path());
      else if (const QCaMotorGUI* cobj = dynamic_cast<const QCaMotorGUI*>(obj))
        rval.setValue(cobj->motor()->getPv());
      else if (const QButtonGroup* cobj = dynamic_cast<const QButtonGroup*>(obj)) {
        rval.setValue(cobj->checkedButton() ? cobj->checkedButton()->text() : QString("None") );
        if (cobj->property("whatsThis").isValid())
          shortDesc = cobj->property("whatsThis").toString();
        QString knownFields;
        foreach (QAbstractButton * but, cobj->buttons())
          knownFields += "'"+but->text()+"' ";
        longDesc = "Possible values: " + knownFields + ". " + longDesc;
      } else if (dynamic_cast<const PositionList*>(obj))
        rval.setValue(QString("Positions"));
      else if (const Shutter* cobj = dynamic_cast<const Shutter*>(obj)) {
        rval.setValue(cobj->ui->selection->currentText());
        QString knownFields;
        for (int cidx=0 ; cidx < cobj->ui->selection->count() ; cidx++ )
          knownFields += "'" + cobj->ui->selection->itemText(cidx) + "' ";
        longDesc = "Possible values: " + knownFields + ". " + longDesc;
      } else
        qDebug() << "Cannot add value of object" << obj << "into cli arguments: unsupported type.";

      if (rval.isValid()) {
        configVals[obj]=rval;
        QString cval = rval.toString();
        if (rval.type() == QVariant::Type::String)
          cval = "'" + cval + "'";
        if (const QAbstractSpinBox* cobj = dynamic_cast<const QAbstractSpinBox*>(obj))
          if ( cobj->specialValueText() == cobj->text() )
            cval += " (" + cobj->text() + ")";
        if (!name.contains('/'))
          name = "general/" + name;
        otable.add(poptmx::OPTION, &configVals[obj], 0, name.toStdString(),
                  "Def: " + shortDesc.toStdString(), longDesc.toStdString(), cval.toStdString());
        if (updateToolTips) {
          if (const QButtonGroup* cobj = dynamic_cast<const QButtonGroup*>(obj)) {
            foreach (QAbstractButton * but, cobj->buttons())
              but->setToolTip(but->toolTip() + "\n--" + name + " " + but->text());
          } else if (QCaMotorGUI* cobj = dynamic_cast<QCaMotorGUI*>(obj))
            cobj->setupButton()->setToolTip(cobj->setupButton()->toolTip() + "\n--" + name);
          else if (Shutter* cobj = dynamic_cast<Shutter*>(obj)) {
            cobj->ui->selection->setToolTip( cobj->ui->selection->toolTip() + "\n--" + name);
          } else if (QWidget * wdg = dynamic_cast<QWidget*>(obj))
            wdg->setToolTip( wdg->toolTip() + "\n--" + name );
        }

      }

    }

    return otable;

  };


  // first table is only to get configFile
  poptmx::OptionTable table = optionsParser() ;
  bool abool; // to emulate standard options
  table.add(poptmx::OPTION, &abool, 'h', "help", "short", "long", "def");
  table.add(poptmx::OPTION, &abool, 'u', "usage", "short", "long", "def");
  table.add(poptmx::OPTION, &abool, 'v', "verbose", "short", "long", "def");
  table.parse(argc,argv);
  w.loadConfiguration(configFile);

  // once configFile is loaded, recreate option parser, this time with correct
  // default values from the config.
  table = optionsParser(true) ;
  table.add_standard_options(&beverbose);
  if ( ! table.parse(argc,argv) )
    exit(0);
  const QString command = QString::fromStdString(table.name());

  // actually set fields in the UI
  foreach (QString name,  names ) {
    QObject * obj = 0;
    foreach (QObject * mobj, w.configNames.keys())
      if ( w.configNames[mobj] == name )
        obj = mobj;
    QVariant rval;
    if (obj && configVals.contains(obj) && table.count(&configVals[obj]) )
      rval = configVals[obj];
    const QString sval = rval.toString();
    if (!rval.isValid())
      continue;
    else if (QLineEdit* cobj = dynamic_cast<QLineEdit*>(obj) ) {
      //cobj->setText(rval.toString());
      qDebug() << cobj->metaObject()->className() << rval << "->:" << rval.toString();
    } else if (QPlainTextEdit* cobj = dynamic_cast<QPlainTextEdit*>(obj)) {
      //cobj->setPlainText(rval.toString());
      qDebug() << cobj->metaObject()->className() << rval << "->:" << rval.toString();
    } else if (QAbstractButton* cobj = dynamic_cast<QAbstractButton*>(obj)) {
      if ( rval.convert(QMetaType::Bool) )
        //cobj->setChecked(rval.toBool());
        qDebug() << cobj->metaObject()->className() << rval << "->:" << rval.toBool();
      else
        qDebug() << cobj->metaObject()->className() << sval << "can't convert to bool";
    } else if (QComboBox* cobj = dynamic_cast<QComboBox*>(obj)) {
      const QString val = rval.toString();
      const int idx = cobj->findText(val);
      if ( idx >= 0 )
        //cobj->setCurrentIndex(idx);
        qDebug() << cobj->metaObject()->className() << rval << "->:" << idx;
      else if ( cobj->isEditable() )
        //cobj->setEditText(val);
        qDebug() << cobj->metaObject()->className() << rval << "->:" << rval.toString();
      else {
        QStringList knownFields;
        for (int cidx=0 ; cidx < cobj->count() ; cidx++ )
          knownFields << cobj->itemText(cidx);
        qDebug() << cobj->metaObject()->className() << rval << "not in existing values:" << knownFields;
      }
    } else if (QSpinBox* cobj = dynamic_cast<QSpinBox*>(obj)) {
      if ( rval.convert(QMetaType::Int) )
        //cobj->setValue(rval.toInt());
        qDebug() << cobj->metaObject()->className() << rval << "->:" << rval.toInt();
      else
        qDebug() << cobj->metaObject()->className() << sval << "can't convert to integer";
    } else if (QDoubleSpinBox* cobj = dynamic_cast<QDoubleSpinBox*>(obj)) {
      if ( rval.convert(QMetaType::Double) )
        //cobj->setValue(rval.toDouble());
        qDebug() << cobj->metaObject()->className() << rval << "->:" << rval.toDouble();
      else
        qDebug() << cobj->metaObject()->className() << sval << "can't convert to double";
    } else if (QTimeEdit* cobj = dynamic_cast<QTimeEdit*>(obj)) {
      if ( rval.convert(QMetaType::QTime) )
        //cobj->setTime(rval.toTime());
        qDebug() << cobj->metaObject()->className() << rval << "->:" << rval.toTime();
      else
        qDebug() << cobj->metaObject()->className() << sval << "can't convert to time";
    //} else if (const QLabel* cobj = dynamic_cast<const QLabel*>(obj))
    //  rval.setValue(cobj->text());
    } else if (UScript* cobj = dynamic_cast<UScript*>(obj)) {
      //cobj->script->setPath(rval.toString());
      qDebug() << cobj->metaObject()->className() << rval << "->:" << rval.toString();
    } else if (QCaMotorGUI* cobj = dynamic_cast<QCaMotorGUI*>(obj)) {
      //cobj->motor()->setPv(rval.toString());
      qDebug() << cobj->metaObject()->className() << rval << "->:" << rval.toString();
    } else if (QButtonGroup* cobj = dynamic_cast<QButtonGroup*>(obj)) {
      qDebug() << cobj->metaObject()->className() << rval << "->:" << rval.toString();
      QAbstractButton * fbut=0;
      foreach (QAbstractButton * but, cobj->buttons())
        if (but->text() == rval.toString())
          fbut = but;
      if (fbut)
        //fbut->setChecked(true);
        qDebug() << cobj->metaObject()->className() << rval << "->: CheckMe" ;
      else {
        QStringList knownFields;
        foreach (QAbstractButton * but, cobj->buttons())
          knownFields << but->text();
        qDebug() << cobj->metaObject()->className() << rval << "not in existing values:" << knownFields;
      }
    } else if (dynamic_cast<const PositionList*>(obj))
      qDebug() << cobj->metaObject()->className() << rval << "->:" << rval.toString();
    else if (Shutter* cobj = dynamic_cast<Shutter*>(obj)) {
      const QString val = rval.toString();
      const int idx = cobj->ui->selection->findText(val);
      if ( idx >= 0 )
        //cobj->ui->selection->setCurrentIndex(idx);
        qDebug() << cobj->metaObject()->className() << rval << "->:" << rval.toString();
      else {
        QStringList knownFields;
        for (int cidx=0 ; cidx < cobj->ui->selection->count() ; cidx++ )
          knownFields << cobj->ui->selection->itemText(cidx);
        qDebug() << cobj->metaObject()->className() << rval << "not in existing values:" << knownFields;
      }
    } else
      qDebug() << "Cannot add value of object" << obj << "into cli arguments: unsupported type.";

  }

  w.show();

  return a.exec();

}
