#include <QApplication>
#include "ctgui_mainwindow.h"
#include <QTimeEdit>
#include <QButtonGroup>
#include <QDebug>

using namespace std;

int main(int argc, char *argv[]) {

  QApplication a(argc, argv);
  MainWindow w(argc, argv);
  w.show();
  return a.exec();

}
