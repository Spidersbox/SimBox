#include <QApplication>
#include "SimboxMain.h"

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);
  app.setWindowIcon(QIcon(":/images/icon"));
  SimboxMain window(":/images/form");
  window.show();

  return app.exec();
}