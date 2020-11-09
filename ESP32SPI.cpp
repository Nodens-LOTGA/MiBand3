#include "ESP32SPI.h"
#include "fcntl.h"
#include <QDataStream>
#include <QDebug>
#include <QThread>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>

ESP32SPI::ESP32SPI(QObject *parent) : QObject(parent) { openSpiPort(); }

ESP32SPI::~ESP32SPI() { closeSpiPort(); }

void ESP32SPI::sendData(uint8_t hr, uint16_t steps) {
  char data[32] = {0};
  char time[32] = {0};
  auto len = sprintf(data, "hr=%hhu;steps=%hu;", hr, steps);
  qDebug() << "Send Data to SPI:" << data;
  writeAndRead(data, time, 32);
  qDebug() << "Read Time from SPI:" << time;
  QDateTime t = QDateTime::fromString(time, Qt::ISODate);
  if (t.isValid())
    timeReceived(t);
}

void ESP32SPI::openSpiPort() {
  m_spiHandle = open("/dev/spidev1.2", O_RDWR);
  if (m_spiHandle < 0) {
    perror("Could not open SPI device");
    return;
  }
  if (ioctl(m_spiHandle, SPI_IOC_WR_MODE, &m_spiMode) < 0) {
    perror("Could not set SPIMode (WR)...ioctl fail");
    return;
  }
  if (ioctl(m_spiHandle, SPI_IOC_RD_MODE, &m_spiMode) < 0) {
    perror("Could not set SPIMode (RD)...ioctl fail");
    return;
  }
  if (ioctl(m_spiHandle, SPI_IOC_WR_BITS_PER_WORD, &m_spiBitsPerWord) < 0) {
    perror("Could not set SPI bitsPerWord (WR)...ioctl fail");
    return;
  }
  if (ioctl(m_spiHandle, SPI_IOC_RD_BITS_PER_WORD, &m_spiBitsPerWord) < 0) {
    perror("Could not set SPI bitsPerWord (RD)...ioctl fail");
    return;
  }
  if (ioctl(m_spiHandle, SPI_IOC_WR_MAX_SPEED_HZ, &m_spiSpeed) < 0) {
    perror("Could not set SPI speed (WR)...ioctl fail");
    return;
  }
  if (ioctl(m_spiHandle, SPI_IOC_RD_MAX_SPEED_HZ, &m_spiSpeed) < 0) {
    perror("Could not set SPI speed (RD)...ioctl fail");
    return;
  }
}

void ESP32SPI::closeSpiPort() {
  if (close(m_spiHandle) < 0) {
    perror("Error - Could not close SPI device");
  }
}

size_t ESP32SPI::writeAndRead(char *tx, char *rx, size_t len) {
  struct spi_ioc_transfer spi {};
  int retVal = -1;

  spi.tx_buf = reinterpret_cast<unsigned long>(tx); // transmit from "data"
  spi.rx_buf = reinterpret_cast<unsigned long>(rx); // receive into "data"
  spi.len = len;
  spi.speed_hz = m_spiSpeed;
  spi.bits_per_word = m_spiBitsPerWord;

  retVal = ioctl(m_spiHandle, SPI_IOC_MESSAGE(1), &spi);

  if (retVal < 0) {
    perror("Error - Problem transmitting spi data..ioctl");
  }
  return retVal;
}

size_t ESP32SPI::write(char *tx, size_t len) {
  struct spi_ioc_transfer spi {};
  int retVal = -1;

  spi.tx_buf = reinterpret_cast<unsigned long>(tx); // transmit from "data"
  spi.len = len;
  spi.speed_hz = m_spiSpeed;
  spi.bits_per_word = m_spiBitsPerWord;

  retVal = ioctl(m_spiHandle, SPI_IOC_MESSAGE(1), &spi);

  if (retVal < 0) {
    perror("Error - Problem transmitting spi data..ioctl");
  }
  return retVal;
}

size_t ESP32SPI::read(char *rx, size_t len) {
  struct spi_ioc_transfer spi {};
  int retVal = -1;

  spi.rx_buf = reinterpret_cast<unsigned long>(rx); // receive into "data"
  spi.len = len;
  spi.speed_hz = m_spiSpeed;
  spi.bits_per_word = m_spiBitsPerWord;

  retVal = ioctl(m_spiHandle, SPI_IOC_MESSAGE(1), &spi);

  if (retVal < 0) {
    perror("Error - Problem transmitting spi data..ioctl");
  }
  return retVal;
}