#include "MiBand3.h"
#include "aes.hpp"
#include <QDataStream>
#include <QDebug>
#include <QRandomGenerator>
#include <algorithm>

MiBand3::MiBand3(QObject *parent) : QObject(parent) {
  m_deviceDiscoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);
  m_deviceDiscoveryAgent->setLowEnergyDiscoveryTimeout(15000);

  connect(m_deviceDiscoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered, this, &MiBand3::addDevice);
  connect(m_deviceDiscoveryAgent,
          static_cast<void (QBluetoothDeviceDiscoveryAgent::*)(QBluetoothDeviceDiscoveryAgent::Error)>(&QBluetoothDeviceDiscoveryAgent::error), this,
          &MiBand3::scanError);

  connect(m_deviceDiscoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished, this, &MiBand3::scanFinished);
  connect(m_deviceDiscoveryAgent, &QBluetoothDeviceDiscoveryAgent::canceled, this, &MiBand3::scanFinished);

  connect(this, &MiBand3::authenticated, this, &MiBand3::startServicesDiscover);
  connect(&m_measureTimer, &QTimer::timeout, this, &MiBand3::keepHRAlive);
}

void MiBand3::startSearch() {
  m_device = QBluetoothDeviceInfo();

  m_deviceDiscoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
}

void MiBand3::setTime(QDateTime time) {
  if (m_authenticated) {
    const QLowEnergyCharacteristic timeChar = m_miBand0Service->characteristic(QBluetoothUuid::CurrentTime);
    if (!timeChar.isValid()) {
      qCritical() << "Time Data not found.";
      return;
    };
    if (!time.isValid() || time == m_dateTime) {
      qDebug() << "Time is not valid or is same.";
      return;
    }
    QByteArray buffer;
    buffer.resize(11);
    buffer[0] = time.date().year() % 256;
    buffer[1] = time.date().year() / 256;
    buffer[2] = time.date().month();
    buffer[3] = time.date().day();
    buffer[4] = time.time().hour();
    buffer[5] = time.time().minute();
    buffer[6] = time.time().second();
    buffer[7] = time.date().weekNumber();
    buffer[8] = 0x0;
    buffer[9] = 0x0;
    buffer[10] = 0x16;
    qDebug() << "Set time to:" << time << buffer.toHex(' ');
    m_dateTime = time;
    m_miBand0Service->writeCharacteristic(timeChar, buffer);
  }
}

void MiBand3::addDevice(const QBluetoothDeviceInfo &device) {
  if (device.coreConfigurations() & QBluetoothDeviceInfo::LowEnergyCoreConfiguration) {
    auto services = device.serviceUuids();
    if (services.contains(QBluetoothUuid(QString(ServiceMiBand0Uuid)))) {
      m_device = device;
    }
    qDebug() << "Low Energy device found" << device.name() << ". Scanning more...";
    qDebug() << "Services:" << device.serviceUuids();
  }
}

void MiBand3::scanError(QBluetoothDeviceDiscoveryAgent::Error error) {
  if (error == QBluetoothDeviceDiscoveryAgent::PoweredOffError)
    qCritical() << "The Bluetooth adaptor is powered off.";
  else if (error == QBluetoothDeviceDiscoveryAgent::InputOutputError)
    qCritical() << "Writing or reading from the device resulted in an error.";
  else
    qCritical() << "An unknown error has occurred.";
}

void MiBand3::scanFinished() {
  if (!m_device.isValid()) {
    qWarning() << "No Mi Band 3 devices found.";
    QTimer::singleShot(60000, this, &MiBand3::startSearch);
  } else {
    qDebug() << "Mi Band 3 found.";
    connectToDevice();
  }
}

void MiBand3::connectToDevice() {
  if (m_control) {
    m_control->disconnectFromDevice();
    delete m_control;
    m_control = nullptr;
  }

  if (m_device.isValid()) {

    m_control = QLowEnergyController::createCentral(m_device, this);
    m_control->setRemoteAddressType(QLowEnergyController::RandomAddress);

    connect(m_control, &QLowEnergyController::serviceDiscovered, this, &MiBand3::serviceDiscovered);
    connect(m_control, &QLowEnergyController::discoveryFinished, this, &MiBand3::serviceScanDone);

    connect(m_control, static_cast<void (QLowEnergyController::*)(QLowEnergyController::Error)>(&QLowEnergyController::error), this,
            [this](QLowEnergyController::Error error) {
              qCritical() << "Cannot connect to remote device. Error:" << error;
              startSearch();
            });
    connect(m_control, &QLowEnergyController::connected, this, [this]() {
      qDebug() << "Controller connected. Search services...";
      m_control->discoverServices();
    });
    connect(m_control, &QLowEnergyController::disconnected, this, &MiBand3::deviceDisconnected);

    m_control->connectToDevice();
  }
}

void MiBand3::serviceDiscovered(const QBluetoothUuid &gatt) {
  if (gatt == QBluetoothUuid(QBluetoothUuid::HeartRate)) {
    qDebug() << "Heart Rate service discovered. Waiting for service scan to be done...";
    m_foundHRService = true;
  } else if (gatt == QBluetoothUuid(QString(ServiceMiBand0Uuid))) {
    qDebug() << "MiBand0 service discovered. Waiting for service scan to be done...";
    m_foundMiBand0Service = true;
  } else if (gatt == QBluetoothUuid(QString(ServiceMiBand1Uuid))) {
    qDebug() << "MiBand1 service discovered. Waiting for service scan to be done...";
    m_foundMiBand1Service = true;
  }
}

void MiBand3::serviceScanDone() {
  qDebug() << "Service scan done.";

  // Delete old service if available
  if (m_hrService) {
    delete m_hrService;
    m_hrService = nullptr;
  }
  if (m_miBand0Service) {
    delete m_miBand0Service;
    m_miBand0Service = nullptr;
  }
  if (m_miBand1Service) {
    delete m_miBand1Service;
    m_miBand1Service = nullptr;
  }

  if (m_foundHRService && m_foundMiBand0Service && m_foundMiBand1Service) {
    m_hrService = m_control->createServiceObject(QBluetoothUuid(QBluetoothUuid::HeartRate), this);
    m_miBand0Service = m_control->createServiceObject(QBluetoothUuid(QString(ServiceMiBand0Uuid)), this);
    m_miBand1Service = m_control->createServiceObject(QBluetoothUuid(QString(ServiceMiBand1Uuid)), this);
  }

  if (m_hrService) {
    connect(m_hrService, &QLowEnergyService::stateChanged, this, &MiBand3::hrStateChanged);
    connect(m_hrService, &QLowEnergyService::characteristicChanged, this, &MiBand3::updateCharacteristicValue);
    connect(m_hrService, &QLowEnergyService::descriptorWritten, this, &MiBand3::confirmedHRDescriptorWrite);
    connect(m_hrService, &QLowEnergyService::characteristicRead, this, &MiBand3::readCharacteristicValue);
  } else {
    qCritical() << "Heart Rate Service not found.";
  }
  if (m_miBand0Service) {
    connect(m_miBand0Service, &QLowEnergyService::stateChanged, this, &MiBand3::miBand0StateChanged);
    connect(m_miBand0Service, &QLowEnergyService::characteristicChanged, this, &MiBand3::updateCharacteristicValue);
    connect(m_miBand0Service, &QLowEnergyService::descriptorWritten, this, &MiBand3::confirmedMiBand0DescriptorWrite);
    connect(m_miBand0Service, &QLowEnergyService::characteristicRead, this, &MiBand3::readCharacteristicValue);
  } else {
    qCritical() << "MiBand0 Service not found.";
  }
  if (m_miBand1Service) {
    connect(m_miBand1Service, &QLowEnergyService::stateChanged, this, &MiBand3::miBand1StateChanged);
    connect(m_miBand1Service, &QLowEnergyService::characteristicChanged, this, &MiBand3::updateCharacteristicValue);
    connect(m_miBand1Service, &QLowEnergyService::descriptorWritten, this, &MiBand3::confirmedMiBand1DescriptorWrite);
    m_miBand1Service->discoverDetails();
  } else {
    qCritical() << "MiBand1 Service not found.";
  }
}

void MiBand3::deviceDisconnected() {
  qWarning() << "LowEnergy controller disconnected";
  m_authenticated = false;
  m_canBeAuthenticated = false;
  m_authKey.clear();
  m_foundHRService = false;
  m_foundMiBand0Service = false;
  m_foundMiBand1Service = false;
  m_measureTimer.stop();
  if (m_hrService != nullptr) {
    delete m_hrService;
    m_hrService = nullptr;
  }
  if (m_miBand0Service != nullptr) {
    delete m_miBand0Service;
    m_miBand0Service = nullptr;
  }
  if (m_miBand1Service != nullptr) {
    delete m_miBand1Service;
    m_miBand1Service = nullptr;
  }
  startSearch();
  // emit finished();
}

void MiBand3::authenticate(const QByteArray &value) {
  const QLowEnergyCharacteristic authChar = m_miBand1Service->characteristic(QBluetoothUuid(QString(CharAuthUuid)));
  if (!authChar.isValid()) {
    qCritical() << "Auth Data not found.";
    return;
  }
  if (m_authenticated == true) {
    qDebug() << "Allready authenticated";
    return;
  }
  if (!m_canBeAuthenticated) {
    if (value.startsWith(QByteArray::fromHex("0101"))) {
      m_canBeAuthenticated = true;
      return;
    } else {
      qDebug() << "Can't authnticate with device (occupied?)";
    }
  } else {
    if (value.startsWith(QByteArray::fromHex("0100"))) {
      qDebug() << "Authentication: descriptor written.";

      QByteArray buffer = QByteArray::fromHex("0100");
      m_authKey.resize(16);
      std::generate(m_authKey.begin(), m_authKey.end(), []() { return static_cast<quint8>(QRandomGenerator::global()->generate()); });
      buffer.append(m_authKey);
      qDebug() << "Generated key message:" << buffer.toHex(' ');
      m_miBand1Service->writeCharacteristic(authChar, buffer, QLowEnergyService::WriteWithoutResponse);
    } else if (value.startsWith(QByteArray::fromHex("100101"))) {
      qDebug() << "Authentication: key received.";

      m_miBand1Service->writeCharacteristic(authChar, QByteArray::fromHex("0200"), QLowEnergyService::WriteWithoutResponse);
    } else if (value.startsWith(QByteArray::fromHex("100201"))) {
      qDebug() << "Authentication: data send.";

      QByteArray buffer = QByteArray::fromHex("0300");
      buffer.append(value.constData() + 3, 16);

      struct AES_ctx ctx;
      AES_init_ctx(&ctx, reinterpret_cast<const uint8_t *>(m_authKey.constData()));
      AES_ECB_encrypt(&ctx, reinterpret_cast<uint8_t *>(buffer.data() + 2));

      qDebug() << "Encrypted Data message:" << buffer.toHex(' ');
      m_miBand1Service->writeCharacteristic(authChar, buffer, QLowEnergyService::WriteWithoutResponse);
    } else if (value.startsWith(QByteArray::fromHex("100301"))) {
      qDebug() << "Authentication: success.";
      m_authenticated = true;
      emit authenticated();
    } else {
      qDebug() << "Authentication: failed.";
      m_authenticated = false;
      m_control->disconnectFromDevice();
    }
  }
}

void MiBand3::hrStateChanged(QLowEnergyService::ServiceState s) {
  switch (s) {
  case QLowEnergyService::DiscoveringServices:
    qDebug() << "Discovering Heart Rate services...";
    break;
  case QLowEnergyService::ServiceDiscovered: {
    qDebug() << "Heart Rate Service discovered.";
    const QLowEnergyCharacteristic hrmChar = m_hrService->characteristic(QBluetoothUuid::HeartRateMeasurement);
    if (!hrmChar.isValid()) {
      qCritical() << "HRM Data not found.";
      return;
    }
    m_hrmNotifDesc = hrmChar.descriptor(QBluetoothUuid::ClientCharacteristicConfiguration);
    startMeasure();
    break;
  }
  default:
    break;
  }
}

void MiBand3::miBand0StateChanged(QLowEnergyService::ServiceState s) {
  switch (s) {
  case QLowEnergyService::DiscoveringServices:
    qDebug() << "Discovering MiBand0 services...";
    break;
  case QLowEnergyService::ServiceDiscovered:
    qDebug() << "MiBand0 Service discovered.";
    break;
  default:
    break;
  }
}

void MiBand3::miBand1StateChanged(QLowEnergyService::ServiceState s) {
  switch (s) {
  case QLowEnergyService::DiscoveringServices:
    qDebug() << "Discovering MiBand1 services...";
    break;
  case QLowEnergyService::ServiceDiscovered: {
    qDebug() << "MiBand1 Service discovered.";
    qDebug() << "Authentication: init.";
    const QLowEnergyCharacteristic authChar = m_miBand1Service->characteristic(QBluetoothUuid(QString(CharAuthUuid)));
    if (!authChar.isValid()) {
      qCritical() << "Auth Data not found.";
      return;
    }
    m_authNotifDesc = authChar.descriptor(QBluetoothUuid::ClientCharacteristicConfiguration);
    if (m_authNotifDesc.isValid())
      m_miBand1Service->writeDescriptor(m_authNotifDesc, QByteArray::fromHex("0100"));
    break;
  }
  default:
    break;
  }
}

void MiBand3::updateCharacteristicValue(const QLowEnergyCharacteristic &c, const QByteArray &value) {
  qDebug() << "Characteristic" << c.name() << ':' << c.uuid() << "changed to" << value.toHex(' ');
  if (c.isValid() && c.uuid() == QBluetoothUuid(QString(CharAuthUuid))) {
    authenticate(value);
  } else if (c.isValid() && c.uuid() == QBluetoothUuid::HeartRateMeasurement) {
    m_hr = value[1];
    emit dataChanged(m_hr, m_steps);
  }
}

void MiBand3::confirmedHRDescriptorWrite(const QLowEnergyDescriptor &d, const QByteArray &value) {
  if (d.isValid() && d == m_authNotifDesc && value == QByteArray::fromHex("0000")) {
    qDebug() << "Notifications disabled.";
    m_control->disconnectFromDevice();
  }
}

void MiBand3::confirmedMiBand0DescriptorWrite(const QLowEnergyDescriptor &d, const QByteArray &value) {
  if (d.isValid() && d == m_authNotifDesc && value == QByteArray::fromHex("0000")) {
    qDebug() << "Notifications disabled.";
    m_control->disconnectFromDevice();
  }
}

void MiBand3::confirmedMiBand1DescriptorWrite(const QLowEnergyDescriptor &d, const QByteArray &value) {
  if (d.isValid() && d == m_authNotifDesc) {
    if (value == QByteArray::fromHex("0000")) {
      qDebug() << "Notifications disabled.";
      m_control->disconnectFromDevice();
    } else {
      authenticate(value);
    }
  }
}

void MiBand3::readCharacteristicValue(const QLowEnergyCharacteristic &c, const QByteArray &value) {
  qDebug() << "Read characteristic" << c.name() << ':' << c.uuid() << "value" << value.toHex(' ');
  if (c.isValid() && c.uuid() == QBluetoothUuid(QString(CharStepsUuid))) {
    QDataStream ds(value);
    uint16_t steps{};
    ds >> steps;
    m_steps = steps;
  }
}

void MiBand3::startServicesDiscover() {
  m_miBand0Service->discoverDetails();
  m_hrService->discoverDetails();
}

void MiBand3::startMeasure() {
  const QLowEnergyCharacteristic hrcChar = m_hrService->characteristic(QBluetoothUuid::HeartRateControlPoint);
  if (!hrcChar.isValid()) {
    qCritical() << "HRC Data not found.";
    return;
  }

  m_hrService->writeCharacteristic(hrcChar, QByteArray::fromHex("150200"));
  m_hrService->writeCharacteristic(hrcChar, QByteArray::fromHex("150100"));
  m_hrService->writeDescriptor(m_hrmNotifDesc, QByteArray::fromHex("0100"));
  m_hrService->writeCharacteristic(hrcChar, QByteArray::fromHex("150101"));

  m_measureTimer.start(10000);
}

void MiBand3::keepHRAlive() {
  const QLowEnergyCharacteristic hrcChar = m_hrService->characteristic(QBluetoothUuid::HeartRateControlPoint);
  if (!hrcChar.isValid()) {
    qCritical() << "HRC Data not found.";
    return;
  };
  const QLowEnergyCharacteristic stepsChar = m_miBand0Service->characteristic(QBluetoothUuid(QString(CharStepsUuid)));
  if (!stepsChar.isValid()) {
    qCritical() << "Steps Data not found.";
    return;
  };

  m_hrService->writeCharacteristic(hrcChar, QByteArray::fromHex("16"));
  m_miBand0Service->readCharacteristic(stepsChar);
}