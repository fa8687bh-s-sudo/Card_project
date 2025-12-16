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

#define SMALL_W 128
#define SMALL_H 128
#define SMALL_SIZE (SMALL_W * SMALL_H)

// Den nedskalade, normaliserade bilden (24x24 => 576 float-värden)
float smallImage[SMALL_SIZE];

#define EPOCH 10 // max number of epochs
int epoch_count = 0; // tracks the current epoch

void downsampleToSmallBilinearNorm(const uint8_t* src, float* dst) {
  float x_ratio = (float)(CAM_W - 1) / (SMALL_W - 1);
  float y_ratio = (float)(CAM_H - 1) / (SMALL_H - 1);

  for (int y = 0; y < SMALL_H; y++) {
    float srcY = y * y_ratio;
    int y0 = (int)srcY;
    int y1 = min(y0 + 1, CAM_H - 1);
    float y_lerp = srcY - y0;

    for (int x = 0; x < SMALL_W; x++) {
      float srcX = x * x_ratio;
      int x0 = (int)srcX;
      int x1 = min(x0 + 1, CAM_W - 1);
      float x_lerp = srcX - x0;

      float p00 = src[y0 * CAM_W + x0];
      float p10 = src[y0 * CAM_W + x1];
      float p01 = src[y1 * CAM_W + x0];
      float p11 = src[y1 * CAM_W + x1];

      float top    = p00 + x_lerp * (p10 - p00);
      float bottom = p01 + x_lerp * (p11 - p01);
      float value  = top + y_lerp * (bottom - top);

      dst[y * SMALL_W + x] = value / 255.0f;
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
    //readWeightsFromCharacteristic(peripheral_characteristic);
    readWeightsChunked(peripheral_characteristic);
    Serial.println("Read weights from peripheral device");

    //beräkna nya vikter utifrån peripheral
    Serial.println("Calculating new weights");
    averageWeights();

    // Skriv tillbaka globala vikter till peripheral
    Serial.println("Sending updated global weights back to peripheral...");
    //writeWeightsToCharacteristic(peripheral_characteristic);
    writeWeightsChunked(peripheral_characteristic);
    blePrint("Peripheral has recieved new calculated weights from central");

    Serial.println("Central done.");

   

  } else {
    //Peripheral set up
    Serial.println("Peripheral Device");
    blePeripheralSetUp();
    Serial.println("Peripheral device set up completets");

    //Packar vikter
    Serial.println("Packing weights ...");
    packWeights();
    BLE.advertise();

    //skickar vikter till central
    //writeWeightsToCharacteristic(weightChar);
    writeWeightsChunked(weightChar);
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

  downsampleToSmallBilinearNorm(frame, smallImage);
  float* prediction = inference(smallImage);
  // TODO: Gör något här med resultatet
  delete[] prediction;

  // Skriv ut de första 20 normaliserade pixlarna så vi ser att något händer
  //Serial.print("Normalized pixles: ");
    for (int y = 0; y < SMALL_H; y++) {
        for (int x = 0; x < SMALL_W; x++) {
            int binaryPixel = (smallImage[y * SMALL_W + x] >= 0.5f) ? 1 : 0;
            Serial.print(binaryPixel);
        }
        Serial.println(); // new line for each row
    }
    Serial.println(); // extra line to separate frames
    delay(10000);
 }



