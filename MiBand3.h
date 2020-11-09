#pragma once

#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QBluetoothUuid>
#include <QDateTime>
#include <QLowEnergyController>
#include <QTimer>
#include <QDateTime>

class MiBand3 : public QObject {
  Q_OBJECT
public:
  static constexpr char ServiceMiBand0Uuid[] = "0000fee0-0000-1000-8000-00805f9b34fb";
  static constexpr char ServiceMiBand1Uuid[] = "0000fee1-0000-1000-8000-00805f9b34fb";
  static constexpr char CharAuthUuid[] = "00000009-0000-3512-2118-0009af100700";
  static constexpr char CharStepsUuid[] = "00000007-0000-3512-2118-0009af100700";
  static constexpr char CharSensorUuid[] = "00000001-0000-3512-2118-0009af100700";
  MiBand3(QObject *parent = nullptr);
public slots:
  void startSearch();
  void setTime(QDateTime time);

signals:
  void finished();
  void authenticated();
  void dataChanged(uint8_t hr, uint16_t steps);

private slots:
  void addDevice(const QBluetoothDeviceInfo &device);
  void scanError(QBluetoothDeviceDiscoveryAgent::Error error);
  void scanFinished();
  void connectToDevice();

  void serviceDiscovered(const QBluetoothUuid &gatt);
  void serviceScanDone();
  void deviceDisconnected();

  void authenticate(const QByteArray &value);

  void hrStateChanged(QLowEnergyService::ServiceState s);
  void miBand0StateChanged(QLowEnergyService::ServiceState s);
  void miBand1StateChanged(QLowEnergyService::ServiceState s);
  void updateCharacteristicValue(const QLowEnergyCharacteristic &c, const QByteArray &value);
  void confirmedHRDescriptorWrite(const QLowEnergyDescriptor &d, const QByteArray &value);
  void confirmedMiBand0DescriptorWrite(const QLowEnergyDescriptor &d, const QByteArray &value);
  void confirmedMiBand1DescriptorWrite(const QLowEnergyDescriptor &d, const QByteArray &value);
  void readCharacteristicValue(const QLowEnergyCharacteristic &c, const QByteArray &value);

  void startServicesDiscover();
  void startMeasure();
  void keepHRAlive();

private:
  QBluetoothDeviceDiscoveryAgent *m_deviceDiscoveryAgent = nullptr;
  QBluetoothDeviceInfo m_device;
  QLowEnergyController *m_control = nullptr;
  bool m_foundHRService = false;
  bool m_foundMiBand0Service = false;
  bool m_foundMiBand1Service = false;
  QLowEnergyService *m_hrService = nullptr;
  QLowEnergyService *m_miBand0Service = nullptr;
  QLowEnergyService *m_miBand1Service = nullptr;
  QLowEnergyDescriptor m_authNotifDesc, m_hrmNotifDesc;
  bool m_authenticated = false;
  bool m_canBeAuthenticated = false;
  QByteArray m_authKey;
  QTimer m_measureTimer;
  QDateTime m_dateTime;
  uint16_t m_steps;
  uint8_t m_hr;
};
