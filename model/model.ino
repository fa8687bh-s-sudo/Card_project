const int N_CLASSES = 4;

#include <Arduino.h>
#include <Arduino_OV767X.h>
#include <ArduinoBLE.h>
#include <Arduino_APDS9960.h>
#include "ble_helpers.h"
#include "NeuralNetwork.h"

const int BLEDEVICE = 1; // 0 om central, 1 om peripheral

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

void downsampleToSmallBilinear(const uint8_t* src, uint8_t* dst) {
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

      dst[y * SMALL_W + x] = (uint8_t)(value + 0.5f); // korrekt avrundning
    }
  }
}



void setup(){
  //Training model
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
    BLEDevice peripheral = connectToPeripheral();
    Serial.println("Central device connected to peripheral device");

    //Hämtar Characteristics från peripheral
    BLECharacteristic peripheral_characteristic = getWeightCharacteristic(peripheral);
    Serial.println("Got weight characteristic from peripheral device");

    // Hämta vikt-karaktäristiken
    BLECharacteristic notifyChr = getNotifyCharacteristic(peripheral);
    delay(200);
    Serial.println("Got weight characteristic from peripheral device");
    if (notifyChr.canSubscribe()) notifyChr.subscribe();
    Serial.println("has subscribed");
    delay(200);

    // Skickar GO till peripheral, att central är redo
    float go = 1.0f;
    peripheral_characteristic.writeValue((uint8_t*)&go, sizeof(float));
    Serial.println("Sent GO to peripheral");
    delay(50);
    BLE.poll();

    //Hämtar vikter från peripheral
    centralReceiveWeightsFromPeripheral(notifyChr);
    Serial.println("central recieved weights");

    //beräkna nya vikter utifrån peripheral
    Serial.println("Calculating new weights");
    averageWeights();

    // Skriv tillbaka globala vikter till peripheral
    Serial.println("Sending updated global weights back to peripheral...");
    writeWeightsToCharacteristic(peripheral_characteristic);

    Serial.println("Central done.");

  } else {
    //Peripheral set up
    Serial.println("Peripheral Device");
    blePeripheralSetUp();
    Serial.println("Peripheral device set up completets");
   
    //Ensuring connection to central
    static bool sent = false;
    BLEDevice central = BLE.central();
    while (!central){
      central = BLE.central();
    }

    //Sending weights from Peripheral to Central
    if (central && !sent) {
    waitForGo();                    
    peripheralSendWeightsToCentral();
    sent = true;
  }
    //Retrieving weights from Central
    if (central && sent) {
      peripheralRecievingWeightsFromCentral(); // ta emot uppdaterade vikter här
    }

    if (!central) sent = false;   

    Serial.println("Peripheral done"); 
  }

  // Use validation data to get accuracy after federated learning
  float accAfter = calculateAccuracy();
  Serial.print("Val accuracy after FL: ");
  Serial.println(accAfter);

  // Starting the camera: QCIF-resolution, GRAYSCALE, clock divisor 1
   Serial.println("Starting camera...");
  if (!Camera.begin(QCIF, GRAYSCALE, 1)) {
    Serial.println("Could not start camera!");
    while (1) {
      delay(1000);
    }
  }

  Serial.print("Camera started. Resolution: ");
  Serial.print(Camera.width());
  Serial.print(" x ");
  Serial.println(Camera.height());
}

 void loop(){
  /*
  BLE.poll();
  Camera.readFrame(frame);

  //downsampleToSmallBilinear(frame, smallImage);
  //float* prediction = inference(smallImage);
  // TODO: Gör något här med resultatet
  //delete[] prediction;

  // Skriv ut de första 20 normaliserade pixlarna så vi ser att något händer
  //Serial.print("Normalized pixles: ");
    for (int y = 0; y < SMALL_H; y++) {
        for (int x = 0; x < SMALL_W; x++) {
            int binaryPixel = (smallImage[y * SMALL_W + x] >= 128) ? 1 : 0;
            Serial.print(binaryPixel);
        }
        Serial.println(); // new line for each row
    }
    Serial.println(); // extra line to separate frames
    delay(10000);
    */
 }



