#include "ESP32SPI.h"
#include "MiBand3.h"
#include <QDateTime>
#include <QProcess>
#include <QStringList>
#include <QtCore>

int main(int argc, char *argv[]) {
  QCoreApplication a(argc, argv);

  QProcess::execute("sudo hciconfig", QStringList{"hci0", "reset"});

  MiBand3 *miBand3 = new MiBand3(&a);
  QObject::connect(miBand3, SIGNAL(finished()), &a, SLOT(quit()));
  QTimer::singleShot(0, miBand3, SLOT(startSearch()));

  ESP32SPI *esp32 = new ESP32SPI(&a);
  QObject::connect(esp32, SIGNAL(timeReceived(QDateTime)), miBand3, SLOT(setTime(QDateTime)));
  QObject::connect(miBand3, SIGNAL(dataChanged(uint8_t, uint16_t)), esp32, SLOT(sendData(uint8_t, uint16_t)));

  //  QTimer *timer = new QTimer(&a);
  //  timer->start(5000);
  //  QObject::connect(timer, &QTimer::timeout, esp32, &ESP32SPI::receiveTime);

  return a.exec();
}
