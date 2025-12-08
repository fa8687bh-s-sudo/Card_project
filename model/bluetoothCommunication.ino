

// This file contains helper methods for bluetooth communication


const char* deviceServiceUuid = "19b10000-e8f2-537e-4f6c-d104768a1214"; // denna ska anpassas
const char* weightCharUuid    = "19b10001-e8f2-537e-4f6c-d104768a1214";


void bleCentralSetUp() {
  if (!BLE.begin()) {
    Serial.println("* Starting Bluetooth® Low Energy module failed!");
    while (1);
  }
  BLE.setLocalName("Card Central");
  Serial.println("Central BLE ready.");
}

void blePeriperalSetUp() {
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
  Serial.println("Peripheral redo, väntar på central...");
}


BLEDevice connectToPeripheral() {
  Serial.println("- Scanning for peripheral with target service...");

  BLE.scanForUuid(deviceServiceUuid);

  BLEDevice peripheral;

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


void sendWeightsFromCentralToPeripheral(BLECharacteristic& weightChar) {
  bool ok = weightChar.writeValue(weights, NUM_WEIGHTS);
  if (ok) {
    Serial.print("Skickade vikter: ");
    for (int i = 0; i < NUM_WEIGHTS; i++) {
      Serial.print(weights[i]);
      Serial.print(' ');
    }
    Serial.println();
  } else {
    Serial.println("Kunde inte skriva vikter");
  }
}
