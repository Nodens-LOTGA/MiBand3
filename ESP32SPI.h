#pragma once
#include <QDateTime>
#include <QObject>

class ESP32SPI : public QObject {
  Q_OBJECT
public:
  ESP32SPI(QObject *parent = nullptr);
  ~ESP32SPI();
public slots:
  void sendData(uint8_t hr, uint16_t steps);
signals:
  void timeReceived(QDateTime time);
private slots:
  void openSpiPort();
  void closeSpiPort();
  size_t writeAndRead(char *tx, char *rx, size_t len);
  size_t write(char *tx, size_t len);
  size_t read(char *rx, size_t len);
  
private:
  int m_spiHandle{};
  unsigned char m_spiMode{};
  unsigned char m_spiBitsPerWord{8};
  unsigned int m_spiSpeed{1'000'000};
};