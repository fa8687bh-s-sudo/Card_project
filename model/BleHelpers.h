#pragma once
#include <ArduinoBLE.h>
#include <Arduino.h>
#include "NeuralNetwork.h"


// This file contains helper methods for Bluetooth Low Energy (BLE) communication.
// It provides setup functions for central and peripheral roles, helpers to
// connect to a peripheral and retrieve a characteristic, and a function to
// send weight data from the central to the peripheral


// UUID of the BLE service that the central device will look for on the peripheral.
const char* deviceServiceUuid = "19b10000-e8f2-537e-4f6c-d104768a1214";

// UUID of the characteristic used for transferring weights between central and peripheral.
const char* weightCharUuid    = "19b10001-e8f2-537e-4f6c-d104768a1214"; // central -> peripheral (WRITE)
const char* notifyCharUuid    = "19b10003-e8f2-537e-4f6c-d104768a1214"; // peripheral -> central (NOTIFY)

extern float weightsAndBiases[];

BLECharacteristic weightChar(weightCharUuid, BLEWrite,  sizeof(float));   // c->p
BLECharacteristic notifyChar(notifyCharUuid, BLEIndicate, sizeof(float));   // p->c

void bleCentralSetUp();
void blePeripheralSetUp();
BLEDevice connectToPeripheral();
BLECharacteristic getWeightCharacteristic(BLEDevice peripheral);
BLECharacteristic getNotifyCharacteristic(BLEDevice peripheral);
void sendUpdatedWeightsToPeripheral(BLECharacteristic& chr);
int peripheralReadEachWeight(BLEDevice central);
BLEService weightService(deviceServiceUuid);

/**
 * @brief Set up the device as a BLE central.
 */
void bleCentralSetUp() {
  if (!BLE.begin()) {
    Serial.println("* Starting Bluetooth® Low Energy module failed!");
    while (1);
  }
  BLE.setLocalName("Card Central");
  Serial.println("Central BLE ready.");
}


/**
 * @brief Set up the device as a BLE peripheral.
 */
void blePeripheralSetUp() {
  if (!BLE.begin()) {
    Serial.println("BLE start fail");
    while (1);
  }

  BLE.setLocalName("Card Peripheral");
  BLE.setAdvertisedService(weightService);
  weightService.addCharacteristic(weightChar);
  weightService.addCharacteristic(notifyChar);
  BLE.addService(weightService);
  BLE.advertise();

  Serial.println("Peripheral ready");
}

/**
 * @brief Scan for and return a peripheral that exposes the target service UUID.
 */
BLEDevice connectToPeripheral() {
  Serial.println("- Scanning for peripheral with target service...");

  BLE.scanForUuid(deviceServiceUuid);

  BLEDevice peripheral;

  // Wait until a peripheral with the target service appears
  while (!peripheral) {
    peripheral = BLE.available();
  }

  BLE.stopScan();

  Serial.println("* Peripheral found!");
  Serial.print("* MAC: ");
  Serial.println(peripheral.address());
  Serial.print("* Name: ");
  Serial.println(peripheral.localName());

  return peripheral;
}

/**
 * @brief Connect to a peripheral and retrieve the weight characteristic.
 */
BLECharacteristic getWeightCharacteristic(BLEDevice peripheral) {
  if (!peripheral.connect()) {
    Serial.println("Failed to connect.");
    return BLECharacteristic(); //empty
  }

  Serial.println("Connected!");

  // Retry discoverAttributes 5 times
  bool ok = false;
  for (int attempt = 0; attempt < 20; attempt++) {
    if (peripheral.discoverAttributes()) {
      ok = true;
      break;
    }
    Serial.print("discoverAttributes failed, retry ");
    Serial.println(attempt + 1);
    delay(300);
    BLE.poll();
  }

  if (!ok) {
    Serial.println("Failed to discover attributes (giving up).");
    peripheral.disconnect();
    return BLECharacteristic();
  }

  BLECharacteristic chr = peripheral.characteristic(weightCharUuid);
  if (!chr) Serial.println("Characteristic not found!");
  return chr;

  Serial.println("Got weight characteristic from peripheral device");
}

/**
 * @brief Retrieve the notify characteristic from a connected peripheral.
 */
BLECharacteristic getNotifyCharacteristic(BLEDevice peripheral) {
  BLECharacteristic chr = peripheral.characteristic(notifyCharUuid);
  if (!chr){
    Serial.println("Notify characteristic not found!");
    return chr;
  }
  Serial.println("Got weight characteristic from peripheral device");
  return chr;
}

/**
 * @brief Write the current weights to the given BLE characteristic.
 */
void sendUpdatedWeightsToPeripheral(BLECharacteristic& chr) {
  packWeights();
  Serial.println("Packing weights");

  for (int i = 0; i < TOTAL_PARAMS; i++) {
    uint8_t b[sizeof(float)];
    memcpy(b, &weightsAndBiases[i], sizeof(float));
    chr.writeValue(b, sizeof(float));
    if (i % 200 == 0){
      Serial.print("Weight number ");
      Serial.print(i);
      Serial.print(" ");
      Serial.println(weightsAndBiases[i], 5);
    }
    BLE.poll();
    delay(10);
  }
}

/**
 * @brief Read incoming weight floats from the central to the peripheral.
 */
int countperipheralReadEachWeight = 0;
int peripheralReadEachWeight(BLEDevice central) {
  BLE.poll();

    if (weightChar.written()) {
      float v;
      int n = weightChar.readValue((uint8_t*)&v, sizeof(float));
      if (countperipheralReadEachWeight % 200 == 0){
        Serial.print("Weight number ");
        Serial.print(countperipheralReadEachWeight);
        Serial.print(": ");
        Serial.println(v, 5);
        }
      if (n != (int)sizeof(float)) {
        Serial.println("Failed to read float");
      }

      weightsAndBiases[countperipheralReadEachWeight++] = v;

      if (countperipheralReadEachWeight >= TOTAL_PARAMS) {
        numParams = TOTAL_PARAMS;
        Serial.println("All weights received!");
        return numParams;
      }
    }
  return countperipheralReadEachWeight; // number of floats recieved
}


void peripheralRecievingWeightsFromCentral(){
  BLEDevice central = BLE.central();
  Serial.println("inside peripheralRecievingWeightsFromCentral");
  if (central) {
    int got = 0;
    while (got < TOTAL_PARAMS){
      got = peripheralReadEachWeight(central);
      if (got >= TOTAL_PARAMS) {
        unpackWeights();
        return;
    }
    }
}
}

void peripheralSendWeightsToCentral() {
  packWeights();
  Serial.println("inside peripheralSendWeightsToCentral");
  delay(500);

  for (int i = 0; i < TOTAL_PARAMS; i++) {
    notifyChar.writeValue((uint8_t*)&weightsAndBiases[i], sizeof(float));
    if (i % 200 == 0){
      Serial.print("Weight number ");
      Serial.print(i);
      Serial.print(": ");
      Serial.println(weightsAndBiases[i], 5);
    }
    BLE.poll();
    delay(10);
  }
}

int countCentralRecieveWeights = 0;
int centralReceiveWeightsFromPeripheral(BLECharacteristic& notifyRemote) {
  while (countCentralRecieveWeights < TOTAL_PARAMS){
    BLE.poll();
    if (notifyRemote.valueUpdated()) {
      float v;
      int n = notifyRemote.readValue((uint8_t*)&v, sizeof(float));
      if (countCentralRecieveWeights % 200 == 0){
        Serial.print("Weight number ");
        Serial.print(countCentralRecieveWeights);
        Serial.print(" ");
        Serial.println(v, 5);
      }
      if (n == (int)sizeof(float)) {
        weightsAndBiases[countCentralRecieveWeights++] = v;
        }
      }
    }
  //numParams = TOTAL_PARAMS;
  return countCentralRecieveWeights;
  }

void waitForGo() {
  Serial.println("Waiting for GO from central...");

  while (true) {
    BLE.poll();
    BLEDevice central = BLE.central();

    if (central && weightChar.written()) {
      float go = 0.0f;
    if (weightChar.readValue((uint8_t*)&go, sizeof(float)) == sizeof(float) && go == 1.0f) {
      Serial.println("GO received!");
      return;
      }
    }
  }
}
