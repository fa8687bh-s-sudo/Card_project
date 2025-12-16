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

const int MAX_WEIGHTS = 24000;
float weightsAndBias[MAX_PARAMS]; 

BLECharacteristic weightChar(
  weightCharUuid,
  BLERead | BLEWrite,
  MAX_PARAMS * sizeof(float) 
);

uint8_t weights[MAX_WEIGHTS];
int numWeightsUsed = 0;


// ----- To print to other device ----- 
const char* logCharUuid       = "19b10002-e8f2-537e-4f6c-d104768a1214";

BLECharacteristic logChar(
  logCharUuid,
  BLERead | BLENotify,
  100
);


// ==================== FUNCTION DECLARATIONS ====================

void bleCentralSetUp();
void blePeripheralSetUp();
BLEDevice connectToPeripheral();
BLECharacteristic getWeightCharacteristic(BLEDevice peripheral);
void writeWeightsToCharacteristic(BLECharacteristic& chr);
int readWeightsFromCharacteristic(BLECharacteristic& chr);
void blePrint(const char* text);

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
  Serial.println("numParams är: ");
  Serial.print(numParams);
  if (numParams <= 0 || numParams > MAX_PARAMS) {
    Serial.println("Invalid numParams, cannot send weights.");
    return;
  }

  int numBytes = numParams * sizeof(float);
  Serial.print("numBytes: ");
  Serial.println(numBytes);

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
  Serial.print("maxBytes: ");
  Serial.println(maxBytes);

  int len = chr.readValue((uint8_t*)weightsAndBias, maxBytes);
  Serial.print("length / len: " );
  Serial.println(len);

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

void blePrint(const char* text) {
  if (BLE.connected()) {
    logChar.writeValue((const uint8_t*)text, strlen(text));
  }
}


// ==================== CHUNKED SEND / RECEIVE ====================
// Skicka vikter i små bitar (chunks) eftersom BLE bara klarar ~244 bytes per write

void writeWeightsChunked(BLECharacteristic& chr) {
  if (numParams <= 0) {
    Serial.println("writeWeightsChunked: no params");
    return;
  }

  int totalBytes = numParams * sizeof(float);
  uint8_t* bytes = (uint8_t*)weightsAndBias;

  const int CHUNK_BYTES = 200;     // något under max (~244)

  Serial.print("Chunked sending ");
  Serial.print(totalBytes);
  Serial.println(" bytes...");

  // Skriv först hur många bytes det totalt gäller (header)
  chr.writeValue((uint8_t*)&totalBytes, sizeof(int));
  delay(20);

  for (int offset = 0; offset < totalBytes; offset += CHUNK_BYTES) {
    int thisLen = min(CHUNK_BYTES, totalBytes - offset);

    bool ok = chr.writeValue(bytes + offset, thisLen);
    if (!ok) {
      Serial.print("Chunk write failed at offset ");
      Serial.println(offset);
      return;
    }

    delay(10); // ge BLE-stack tid
  }

  Serial.println("Chunked sending done!");
}


int readWeightsChunked(BLECharacteristic& chr) {
  // 1) Läs hur många bytes vi ska ta emot
  int totalBytes = 0;
  int len = chr.readValue((uint8_t*)&totalBytes, sizeof(int));
  if (len != sizeof(int)) {
    Serial.println("readWeightsChunked: failed to read header.");
    return 0;
  }

  Serial.print("Expecting ");
  Serial.print(totalBytes);
  Serial.println(" bytes total.");

  int expectedParams = totalBytes / sizeof(float);
  if (expectedParams > MAX_PARAMS) {
    Serial.println("Too many params in chunked read!");
    return 0;
  }

  uint8_t* bytes = (uint8_t*)weightsAndBias;

  const int CHUNK_BYTES = 200;

  int received = 0;

  while (received < totalBytes) {
    int toRead = min(CHUNK_BYTES, totalBytes - received);

    int got = chr.readValue(bytes + received, toRead);
    if (got <= 0) {
      Serial.println("readWeightsChunked: failed mid-way");
      return 0;
    }

    received += got;
    delay(10);
  }

  numParams = expectedParams;

  Serial.print("Chunked read complete. Received params = ");
  Serial.println(numParams);

  return numParams;
}



