const int N_CLASSES = 4;


#include <Arduino.h>
#include <Arduino_OV767X.h>
#include <ArduinoBLE.h>
#include <Arduino_APDS9960.h>
#include "ble_helpers.h"
#include "NeuralNetwork.h"


const int BLEDEVICE = 0; // 0 om central, 1 om peripheral

// Vi använder QCIF (176x144) i gråskala – 1 byte per pixel
#define CAM_W 176
#define CAM_H 144

// Buffer där hela kamerabilden hamnar (176 * 144 bytes)
uint8_t frame[CAM_W * CAM_H];

#define SMALL_W resolution
#define SMALL_H resolution
#define SMALL_SIZE (SMALL_W * SMALL_H)

// Den nedskalade, normaliserade bilden (24x24 => 576 float-värden)
float smallImage[SMALL_SIZE];

#define EPOCH 50 // max number of epochs
int epoch_count = 0; // tracks the current epoch

void downsampleToSmall(const uint8_t* fullImage, float* smallImage) {
  for (int smallY = 0; smallY < SMALL_H; smallY++) {
    int fullY = (smallY * CAM_H) / SMALL_H; // motsvarande rad i stora bilden
    
    for (int smallX = 0; smallX < SMALL_W; smallX++) {
      int fullX = (smallX * CAM_W) / SMALL_W; // motsvarande kolumn i stora bilden
      // index i flat array
      int fullIndex  = fullY  * CAM_W  + fullX;
      int smallIndex = smallY * SMALL_W + smallX;
      // normalisera 0..255 → 0..1
      smallImage[smallIndex] = fullImage[fullIndex] / 255.0f;
    }
  }
}


void setup(){
  Serial.begin(115200);
  while (!Serial) {} 
  Serial.println("Create Model");
  createModel(weightsAndBias);
  for(int i = 0; i < EPOCH; i++){
    Serial.print("Training epoch ");
    Serial.println(epoch_count);
    trainModelAllImages();
    epoch_count++;
  }

  // Use validation data to get accuracy beofre federated learning
  float accBefore = calculateAccuracy();
  Serial.print("Val accuracy before FL: ");
  Serial.println(accBefore);


  if (BLEDEVICE == 0) {
    // Central Set up
    Serial.println("This is the Central Device. The central device will first get weights from the peripheral device");
    bleCentralSetUp();

    // Hittar peripheral
    BLEDevice peripheral = connectToPeripheral(); //oklart om detta är rätt
    Serial.println("Central device connected to peripheral device");

    // Hämta vikt-karaktäristiken
    BLECharacteristic peripheral_characteristic = getWeightCharacteristic(peripheral);
    Serial.println("Got weight characteristic from peripheral device");

    //Läser vikter från peripheral
    readWeightsFromCharacteristic(peripheral_characteristic);
    Serial.println("Read weights from peripheral device");

    //beräkna nya vikter utifrån peripheral
    Serial.println("Calculating new weights");
    averageWeights();

    // Skriv tillbaka globala vikter till peripheral
    Serial.println("Sending updated global weights back to peripheral...");
    writeWeightsToCharacteristic(peripheral_characteristic);;

    Serial.println("Central done.");

   

  } else {
    //Peripheral set up
    Serial.println("Peripheral Device");
    blePeripheralSetUp();
    Serial.println("Peripheral device set up completets");

    //Packar vikter
    Serial.println("Packing weights ...");
    packWeights();

    //skickar vikter till central
    writeWeightsToCharacteristic(weightChar);
    Serial.print("Weights have been sent from peripheral device to central device");    
  }

  // Use validation data to get accuracy after federated learning
  float accAfter = calculateAccuracy();
  Serial.print("Val accuracy after FL: ");
  Serial.println(accAfter);


  Serial.println("Starting camera...");

  // Starta kameran: QCIF-upplösning, GRAYSCALE, clock divisor 1
  if (!Camera.begin(QCIF, GRAYSCALE, 1)) {
    Serial.println("Could not start camera!");
    while (1) {
      delay(1000);
    }
  }

  Serial.print("Camera started. Upplösning: ");
  Serial.print(Camera.width());
  Serial.print(" x ");
  Serial.println(Camera.height()); // borde vara 176x144
}

 void loop(){
  BLE.poll();
  Camera.readFrame(frame);

  downsampleToSmall(frame, smallImage);
  float* prediction = inference(smallImage);
  // TODO: Gör något här med resultatet
  delete[] prediction;

  // Skriv ut de första 20 normaliserade pixlarna så vi ser att något händer
  Serial.print("Normalized pixles: ");
    for (int i = 0; i < 20; i++) {
      Serial.print(smallImage[i], 3);  // 3 decimaler
      Serial.print(' ');
    }
  Serial.println();
  delay(500); // lite paus
 }



