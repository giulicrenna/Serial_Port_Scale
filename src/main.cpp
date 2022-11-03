// https://github.com/LiveSparks/ESP32_BLE_Examples/blob/master/BLE_switch/BLE_Switch.ino
// https://github.com/choichangjun/ESP32_arduino/blob/master/ESP32_Arduino_paring_Key.ino
#include <Arduino.h>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <LittleFS.h>
#include <Arduino.h>
#include <vector>
#include <cstdlib>

#include "global.hpp" // WARNING! global.h must be included before BluethoothSerial.h, 'cause bluethooth pin is set from global.h
#include <HardwareSerial.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

/* define the UUID that our custom service will use */
#define SERVICE_UUID "0000FFE0-0000-1000-8000-00805F9B34FB"
#define CHARACTERISTIC_UUID "0000FFE1-0000-1000-8000-00805F9B34FB"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

HardwareSerial SerialPort(1);

int detRate(int RXD, int TXD, bool isRS232 = true);
void changeColour(int color[3]);

/*
RED
BLUE
GREEN
PINK
WHITE
*/
int color[5][3] = {{255, 0, 0}, {0, 0, 102}, {0, 204, 0}, {204, 0, 204}, {255, 255, 255}};

BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;

class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer *pServer)
  {
    deviceConnected = false;
  }
};

class MyCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    std::string rxValue = pCharacteristic->getValue();

    if (rxValue.length() > 0)
    {
      Serial.println("*********");
      Serial.print("Received Value: ");
      for (int i = 0; i < rxValue.length(); i++)
        Serial.print(rxValue[i]);

      Serial.println();
      Serial.println("*********");
    }
  }
};

// Security class
class MySecurity : public BLESecurityCallbacks
{

  uint32_t onPassKeyRequest()
  {

    ESP_LOGI(LOG_TAG, "PassKeyRequest");

    return pin;
  }

  void onPassKeyNotify(uint32_t pass_key)
  {

    ESP_LOGI(LOG_TAG, "The passkey Notify number:%d", pass_key);
  }

  bool onConfirmPIN(uint32_t pass_key)
  {

    ESP_LOGI(LOG_TAG, "The passkey YES/NO number:%d", pass_key);

    vTaskDelay(5000);

    return true;
  }

  bool onSecurityRequest()
  {

    ESP_LOGI(LOG_TAG, "SecurityRequest");

    return true;
  }

  void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl)
  {

    ESP_LOGI(LOG_TAG, "Starting BLE work!");
  }
};

void setup()
{
  pinMode(PIN_RED, OUTPUT);
  pinMode(PIN_GREEN, OUTPUT);
  pinMode(PIN_BLUE, OUTPUT);

  Serial.begin(9600);
  int baudrate = detRate(RXD_232, TXD_232);

  if (baudrate)
  {
#define RS232_
  }
  else
  {
    baudrate = detRate(RXD_485, TXD_485);
#define RS485_
    digitalWrite(TR, LOW); // Transmitter: LOW, Receiver: HIGH
  }

  // Create the BLE Device
  BLEDevice::init("Darkflow-Device");

  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);

  BLEDevice::setSecurityCallbacks(new MySecurity());

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID_TX,
      BLECharacteristic::PROPERTY_NOTIFY);

  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID_RX,
      BLECharacteristic::PROPERTY_WRITE);

  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client lpm...");

  // Security Stuff
  BLESecurity *pSecurity = new BLESecurity(); // pin

  uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;

  uint32_t passkey = pin; // PASS

  uint8_t auth_option = ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_DISABLE;

  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(uint32_t));

  pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);

  pSecurity->setCapability(ESP_IO_CAP_OUT);

  pSecurity->setKeySize(16);

  esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH, &auth_option, sizeof(uint8_t));

  pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

  // advertisement config

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("Characteristic defined! Now you can read it in your phone!");

#ifdef RS232_
  SerialPort.begin(baudrate, SERIAL_8N1, RXD_232, TXD_232);
#endif
#ifdef RS485_
  SerialPort.begin(baudrate, SERIAL_8N1, RXD_485, TXD_485);
#endif
}

void loop()
{
#ifdef RS232
  String msg = SerialPort.readString();

  if (deviceConnected)
  {

    char buffer[msg.length() + 1];
    // msg.toCharArray(buffer, msg.length() + 1);
    pTxCharacteristic->setValue(msg.c_str());
    pTxCharacteristic->notify();
    delay(10); // bluetooth stack will go into congestion, if too many packets are sent
  }
  // disconnecting
  if (!deviceConnected && oldDeviceConnected)
  {
    delay(500);                  // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
    Serial.println("start advertising");
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected)
  {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }

  Serial.println(msg);
#endif

#ifdef RS485
  String msg = SerialPort.readString();

  if (deviceConnected)
  {
    char buffer[msg.length() + 1];
    // msg.toCharArray(buffer, msg.length() + 1);
    pTxCharacteristic->setValue(msg.c_str());
    pTxCharacteristic->notify();
    delay(10); // bluetooth stack will go into congestion, if too many packets are sent
  }
  // disconnecting
  if (!deviceConnected && oldDeviceConnected)
  {
    delay(500);                  // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
    Serial.println("start advertising");
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected)
  {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }

  Serial.println(msg);
#endif
}

bool areAnyKnownCharacter(std::string str)
{
  int numbers[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  for (int num : numbers)
  {
    if (str.find("\r") != std::string::npos)
    {
      return true;
    }
    else
    {
      return false;
    }
  }
  return false;
}

// Auto Baudrate
int detRate(int RXD, int TXD, bool isRS232)
{
  int count = 0;
  int bauds[14] = {9600, 115200, 110, 300, 600, 1200, 57600, 2400, 4800, 14400, 19200, 38400, 128000, 256000};
  for (int baud : bauds)
  {
    // ITERATE OVER RS232
    if (isRS232)
    {
      changeColour(color[count]);
      count++;
      Serial.println("Testing: " + String(baud) + " bauds.");
      Serial1.begin(baud, SERIAL_8N1, RXD, TXD);
      String incomming = Serial1.readString();
      if (areAnyKnownCharacter(incomming.c_str()))
      {
        Serial.println("Correct configuration found!");
        Serial1.end();
        return baud;
      }
      if (count == 5)
      {
        count = 0;
      }
    }
    else
    {
    // ITERATE OVER RS485
      changeColour(color[count]);
      count++;
      Serial.println("Testing: " + String(baud) + " bauds.");
      Serial1.begin(baud, SERIAL_8N1, RXD, TXD);
      digitalWrite(TX, LOW);
      String incomming = Serial1.readString();
      if (areAnyKnownCharacter(incomming.c_str()))
      {
        Serial.println("Correct configuration found!");
        Serial1.end();
        return baud;
      }
      if (count == 5)
      {
        count = 0;
      }
    }
  }
   return false;
}

void changeColour(int color[3])
{
  analogWrite(PIN_RED, color[0]);
  analogWrite(PIN_GREEN, color[1]);
  analogWrite(PIN_BLUE, color[2]);
}