#pragma once
#include <ArduinoBLE.h>
#include <Arduino.h>
#include "NeuralNetwork.h"


// This file contains helper methods for Bluetooth Low Energy (BLE) communication.
// It provides setup functions for central and peripheral roles, helpers to
// connect to a peripheral and retrieve a characteristic, and a function to
// send weight data from the central to the peripheral


// UUID of the BLE service that the central device will look for on the peripheral.
const char* deviceServiceUuid = "19b10000-e8f2-537e-4f6c-d104768a1214"; // denna ska anpassas

// UUID of the characteristic used for transferring weights between central and peripheral.
const char* weightCharUuid    = "19b10001-e8f2-537e-4f6c-d104768a1214";


// ==================== WEIGHT DECLARATIONS ====================

// OBS denna ska ändras när vi vet vikterna!

const int MAX_WEIGHTS = 256;
float weightsAndBias[MAX_PARAMS]; 

BLECharacteristic weightChar(
  weightCharUuid,
  BLERead | BLEWrite,
  MAX_WEIGHTS
);

uint8_t weights[MAX_WEIGHTS];
int numWeightsUsed = 0;


// ==================== FUNCTION DECLARATIONS ====================

void bleCentralSetUp();
void blePeripheralSetUp();
BLEDevice connectToPeripheral();
BLECharacteristic getWeightCharacteristic(BLEDevice peripheral);
void writeWeightsToCharacteristic(BLECharacteristic& chr);
int readWeightsFromCharacteristic(BLECharacteristic& chr);

BLEService weightService(deviceServiceUuid); //denna är jag lite osäker på


// ==================== FUNCTION IMPLEMENTATIONS ====================

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
  BLE.addService(weightService);

  BLE.advertise();
  Serial.println("Peripheral ready, waiting for central...");
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
    return BLECharacteristic();  // tom
  }

  Serial.println("Connected!");

  if (!peripheral.discoverAttributes()) {
    Serial.println("Failed to discover attributes.");
    peripheral.disconnect();
    return BLECharacteristic();
  }

  BLECharacteristic chr = peripheral.characteristic(weightCharUuid);

  if (!chr) {
    Serial.println("Characteristic not found!");
  }

  return chr;
}

/**
 * @brief Write the current weights to the given BLE characteristic.
 */
void writeWeightsToCharacteristic(BLECharacteristic& chr) {
  if (numParams <= 0 || numParams > MAX_PARAMS) {
    Serial.println("Invalid numParams, cannot send weights.");
    return;
  }

  int numBytes = numParams * sizeof(float);

  bool ok = chr.writeValue((uint8_t*)weightsAndBias, numBytes);
  if (ok) {
    Serial.print("Sent ");
    Serial.print(numParams);
    Serial.println(" params via BLE.");
  } else {
    Serial.println("Could not send weights");
  }
}


/**
 * @brief Read weight data from a BLE characteristic
 */
int readWeightsFromCharacteristic(BLECharacteristic& chr) {
  int maxBytes = MAX_PARAMS * sizeof(float);

  int len = chr.readValue((uint8_t*)weightsAndBias, maxBytes);

  if (len <= 0) {
    Serial.println("Failed to read weights from characteristic.");
    numParams = 0;
    return 0;
  }

  numParams = len / sizeof(float);

  Serial.print("Read ");
  Serial.print(numParams);
  Serial.println(" params from BLE.");

  return numParams;
}

