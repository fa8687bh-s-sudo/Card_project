#pragma once
#include <ArduinoBLE.h>
#include <Arduino.h>

// This file contains helper methods for Bluetooth® Low Energy (BLE) communication.
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
  Serial.begin(115200);
  while (!Serial) {}

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
 * @brief Write the current weights[] buffer to the given BLE characteristic.
 *
 * Can be used by:
 *  - the central (to send global weights to a peripheral)
 *  - the peripheral (to publish locally trained weights)
 */
void writeWeightsToCharacteristic(BLECharacteristic& chr) {
  if (numWeightsUsed <= 0 || numWeightsUsed > MAX_WEIGHTS) {
    Serial.println("Invalid numWeightsUsed, cannot send weights.");
    return;
  }

  bool ok = chr.writeValue(weights, numWeightsUsed);
  if (ok) {
    Serial.print("Sent weights: ");
    for (int i = 0; i < numWeightsUsed; i++) {
      Serial.print(weights[i]);
      Serial.print(' ');
    }
    Serial.println();
  } else {
    Serial.println("Could not send weights");
  }
}


/**
 * @brief Read weight data from a BLE characteristic into the global weights[] buffer.
 *
 * Typically used by the central device to fetch:
 *  - initial weights from a peripheral client, or
 *  - updated, locally trained weights from a peripheral.
 *
 * After reading:
 *  - The weights are stored in the global `weights[]`
 *  - The number of bytes read is stored in `numWeightsUsed`
 *
 * @param chr  Reference to the BLE characteristic on the remote device.
 * @return int Number of bytes read (same as numWeightsUsed). Returns 0 on failure.
 */
int readWeightsFromCharacteristic(BLECharacteristic& chr) {
  uint8_t buffer[MAX_WEIGHTS];

  // Try to read up to MAX_WEIGHTS bytes
  int len = chr.readValue(buffer, MAX_WEIGHTS);

  if (len <= 0) {
    Serial.println("Failed to read weights from characteristic.");
    numWeightsUsed = 0;
    return 0;
  }

  // Store values in global array
  numWeightsUsed = len;
  for (int i = 0; i < len; i++) {
    weights[i] = buffer[i];
  }

  // Debug print
  Serial.print("Read weights: ");
  for (int i = 0; i < numWeightsUsed; i++) {
    Serial.print(weights[i]);
    Serial.print(' ');
  }
  Serial.println();

  return len;
}

