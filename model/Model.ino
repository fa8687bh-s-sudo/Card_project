#include <Arduino_OV767X.h>
#include <ArduinoBLE.h>
#include "Data.h"
#include "TestData.h"
#include "Cropper.h"
#include "NeuralNetwork.h"
#include "BleHelpers.h"

constexpr int BLE_DEVICE = 0; // 0 - central, 1 - peripheral
constexpr int NBR_EPOCHS = 30;
constexpr int BIG_WIDTH = IMAGE_SIZE;
constexpr int BIG_HEIGHT = IMAGE_SIZE;
constexpr int CAMERA_WIDTH = 176;
constexpr int CAMERA_HEIGHT = 144;
constexpr uint8_t GRAYSCALE_THRESHOLD = 150;
float weightsAndBiases[TOTAL_PARAMS];
static uint8_t cameraBuffer[CAMERA_WIDTH * CAMERA_HEIGHT * 2];
static uint8_t fullImage[IMAGE_SIZE * IMAGE_SIZE / 8];
static uint8_t croppedImage[PATCH_SIZE * PATCH_SIZE];

static void printImage(uint8_t* image) {
  for (int j = 0; j < PATCH_SIZE; j++) {
    for (int k = 0; k < PATCH_SIZE; k++) {
      Serial.print(image[j * PATCH_SIZE + k]);
      Serial.print(" ");
    }
    Serial.println();
  }
}

static void printGrayPreview() {
  for (int y = 0; y < CAMERA_HEIGHT; y += 1) {
    for (int x = 0; x < CAMERA_WIDTH; x += 1) {
      int i = (y * CAMERA_WIDTH + x) * 2;
      uint16_t rgb565 = (uint16_t)cameraBuffer[i] | ((uint16_t)cameraBuffer[i + 1] << 8);

      uint8_t r = ((rgb565 >> 11) & 0x1F) << 3;
      uint8_t g = ((rgb565 >>  5) & 0x3F) << 2;
      uint8_t b = ((rgb565 >>  0) & 0x1F) << 3;
      uint8_t gray = (30u * r + 59u * g + 11u * b) / 100u;

      const char ramp[] = "@&#*+=-:. ";
      Serial.print(ramp[ gray / 26 ]);
    }
    Serial.println();
  }
}

static void train() {
  static uint8_t rotated[PATCH_SIZE * PATCH_SIZE];
  size_t nbrImages = (BLE_DEVICE == 0) ? NBR_CENTRAL_TRAIN_IMAGES : NBR_PERIPHERAL_TRAIN_IMAGES;
  for (int i = 0; i < nbrImages; i++) {
    for (int j = 0; j < IMAGE_SIZE * IMAGE_SIZE / 8; j++) {
      fullImage[j] = (BLE_DEVICE == 0) ? pgm_read_byte(&centralTrainImages[i][j]) : pgm_read_byte(&peripheralTrainImages[i][j]);
    }
    uint8_t label = (BLE_DEVICE == 0) ? pgm_read_byte(&centralTrainLabels[i]): pgm_read_byte(&peripheralTrainLabels[i]);

    cropImage(fullImage, croppedImage);
    trainModel(croppedImage, label);

    for (int j = 0; j < PATCH_SIZE * PATCH_SIZE; j++) {
      rotated[PATCH_SIZE * PATCH_SIZE - 1 - j] = croppedImage[j];
    }
    trainModel(rotated, label);
    delay(1);
  }
}

static void validate() {
  size_t correct = 0;
  size_t total = 0;
  size_t nbrImages = (BLE_DEVICE == 0) ? NBR_CENTRAL_VAL_IMAGES : NBR_PERIPHERAL_VAL_IMAGES;
  for (int i = 0; i < nbrImages; i++) {
    //Serial.print("Validating with image ");
    //Serial.println(i);
    for (int j = 0; j < IMAGE_SIZE * IMAGE_SIZE / 8; j++) {
      fullImage[j] = (BLE_DEVICE == 0) ? pgm_read_byte(&centralValImages[i][j]) : pgm_read_byte(&peripheralValImages[i][j]);
    }
    uint8_t label = (BLE_DEVICE == 0) ? pgm_read_byte(&centralValLabels[i]): pgm_read_byte(&peripheralValLabels[i]);
    cropImage(fullImage, croppedImage);

    updateAccuracy(croppedImage, label, correct, total);

    //printImage(croppedImage);    

    delay(1);
  }
  float accuracy = (total > 0) ? (float) correct / total : 0.0f;
  Serial.print("Validation accuracy: ");
  Serial.println(accuracy, 4);
  Serial.println();
}

void trainAndValidate() {
  for (int i = 0; i < NBR_EPOCHS; i++) {
    Serial.print("EPOCH ");
    Serial.println(i + 1);
    train();
    validate();
  } 
}

float calculateTestAccuracy() {
  size_t correct = 0;
  size_t total = 0;
  for (int i = 0; i < NBR_TEST_IMAGES; i++) {
    for (int j = 0; j < IMAGE_SIZE * IMAGE_SIZE / 8; j++) {
      fullImage[j] = pgm_read_byte(&testImages[i][j]);
    }
    uint8_t label = pgm_read_byte(&testLabels[i]);
    cropImage(fullImage, croppedImage);
    updateAccuracy(croppedImage, label, correct, total);
    delay(1);
  }
  float accuracy = (total > 0) ? (float) correct / total : 0.0f;
  return accuracy;
}

bool captureFromCamera() {
  constexpr int outputPixelCount = BIG_WIDTH * BIG_HEIGHT;
  constexpr int outputByteCount = outputPixelCount / 8;

  for (int byteIndex = 0; byteIndex < outputByteCount; byteIndex++) {
    fullImage[byteIndex] = 0;
  }

  Camera.readFrame(cameraBuffer);
  printGrayPreview();

  for (int outY = 0; outY < BIG_HEIGHT; outY++) {
    const int srcY = (outY * CAMERA_HEIGHT) / BIG_HEIGHT;

    for (int outX = 0; outX < BIG_WIDTH; outX++) {
      const int srcX = (outX * CAMERA_WIDTH) / BIG_WIDTH;
      const int srcPixelIndex = (srcY * CAMERA_WIDTH + srcX);
      const int srcByteIndex = srcPixelIndex * 2;

      const uint16_t rgb565 = (uint16_t)cameraBuffer[srcByteIndex] | ((uint16_t)cameraBuffer[srcByteIndex + 1] << 8);

      const uint8_t red5   = (rgb565 >> 11) & 0x1F;
      const uint8_t green6 = (rgb565 >>  5) & 0x3F;
      const uint8_t blue5  = (rgb565 >>  0) & 0x1F;

      const uint8_t red8   = (uint8_t)(red5 << 3);
      const uint8_t green8 = (uint8_t)(green6 << 2);
      const uint8_t blue8  = (uint8_t)(blue5 << 3);

      const uint8_t grayscale = (uint8_t)((30u * red8 + 59u * green8 + 11u * blue8) / 100u);

      const bool isWhitePixel = (grayscale >= GRAYSCALE_THRESHOLD);

      const int outLinearIndex = outY * BIG_WIDTH + outX;
      const int outPackedByteIndex = outLinearIndex >> 3;
      const int outBitInByte = 7 - (outLinearIndex & 7);

      if (isWhitePixel) {
        fullImage[outPackedByteIndex] |= (uint8_t)(1u << outBitInByte);
      }
    }
  }

  return true;
}

void testCamera() {
  for (int i = 0; i < 5; i++) { // Take some pictures first to discard
    Camera.readFrame(cameraBuffer); 
    delay(50);
  }
  Serial.println("Capturing one frame...");
  captureFromCamera();
  Serial.println("Captured. Cropping...");
  cropImage(fullImage, croppedImage);
  printImage(croppedImage);
}

void bleCentralSetup() {
  Serial.println("This is the Central Device. The central device will first get weights from the peripheral device");
  bleCentralSetUp();

  // Hittar peripheral
  BLEDevice peripheral = connectToPeripheral();
  
  //Hämtar Characteristics från peripheral
  BLECharacteristic peripheral_characteristic = getWeightCharacteristic(peripheral);

  // Hämta vikt-karaktäristiken
  BLECharacteristic notifyChr = getNotifyCharacteristic(peripheral);
  delay(200);
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
  unpackWeights();

  // Skriv tillbaka globala vikter till peripheral
  Serial.println("Sending updated global weights back to peripheral...");
  sendUpdatedWeightsToPeripheral(peripheral_characteristic);

  Serial.println("Central done.");
}

void blePeripheralSetup() {
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

void setup() {
  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && millis() - start < 3000) { }
  delay(100); 
  Serial.println("Starting code!");
  Serial.flush();

  if(!Camera.begin(QCIF, RGB565, 1)) {
    Serial.println("Failed to start camera");
    while (1) {}
  }

  randomSeed(micros());
  createModel(weightsAndBiases);
  Serial.println("Model created, starting training...");
  trainAndValidate();
  Serial.print(NBR_EPOCHS);
  Serial.println(" epochs of training done.");
  Serial.println();

  float accuracyBefore = calculateTestAccuracy();
  Serial.print("Accuracy with test data (before FL): ");
  Serial.println(accuracyBefore);

  //testCamera();

  if (BLE_DEVICE == 0) {
    bleCentralSetup();
  } else {
    blePeripheralSetup();
  }

  float accuracyAfter = calculateTestAccuracy();
  Serial.print("Accuracy with test data (after FL): ");
  Serial.println(accuracyAfter);
}

void loop() {
  /*
  captureFromCamera();
  cropImage(fullImage, croppedImage);
  float prediction[NBR_CLASSES];
  inference(croppedImage, prediction);
  
  float max =  -1.0f;
  size_t maxIndex = 0;
  for(size_t neuron = 0; neuron < NBR_CLASSES; neuron++) {
      Serial.print("Probability of being ");
      Serial.print(neuron);
      Serial.print(": ");
      Serial.println(prediction[neuron]);
      if (prediction[neuron] > max) {
          max = prediction[neuron];
          maxIndex = neuron;
      }
  }
  */
}

