#ifndef POSITIONLIST_H
#define POSITIONLIST_H

#include <QWidget>
#include <QSettings>
#include <QStyledItemDelegate>
#include <qcamotorgui.h>
#include <QTableWidget>


// to be used for the double numbers in the list of positions
// in the irregular step serial scans.

class NTableDelegate : public QStyledItemDelegate {
  Q_OBJECT;
public:
  NTableDelegate(QObject* parent);
  QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;
  void setEditorData(QWidget *editor, const QModelIndex &index) const;
  void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const;
  void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const;
};

// QTableWidget with support for copy and paste added
// Here copy and paste can copy/paste the entire grid of cells
class QTableWidgetWithCopyPaste : public QTableWidget
{
public:
  QTableWidgetWithCopyPaste(int rows, int columns, QWidget *parent) :
      QTableWidget(rows, columns, parent)
  {};

  QTableWidgetWithCopyPaste(QWidget *parent) :
    QTableWidget(parent)
  {};

private:
  void copy();
  void paste();

protected:
  virtual void keyPressEvent(QKeyEvent * event);
};



#include "ui_positionlist.h"


class PositionList : public QWidget
{
  Q_OBJECT;

private:
  bool allOK;
  bool freezListUpdates;
  static const int doMeCol = 3;

public:

  Ui::PositionList * const ui; // do not change outside public only to store
  QCaMotorGUI * motui; // do not change outside public only to move and store

  explicit PositionList(QWidget *parent = 0);
  ~PositionList();

  void putMotor(QCaMotorGUI * motor);
  void putLabel(const QString & lab) {ui->label->setText(lab);}
  bool amOK() {return allOK;}
  void freezList(bool fz) {freezListUpdates=fz;}
  void emphasizeRow(int row=-1);
  double position(int row) const {return ui->list->item(row, 0)->text().toDouble();}
  bool doMe(int row) const { return row < ui->list->rowCount() &&
                                    ((QSCheckBox*)ui->list->cellWidget(row, doMeCol))->isChecked(); }
  void done(int row=-1);
  void todo(int row=-1);
  bool doAny() const;
  bool doAll() const;

private slots:

  void updateAmOK();
  void updateNoF();
  void moveMotorHere();
  void getMotorPosition();
  void updateToDo(int index);

signals:

  void amOKchanged(bool);
  void parameterChanged();

};

#endif // POSITIONLIST_H
