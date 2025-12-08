const int N_CLASSES = 4;


#include <Arduino.h>
#include <Arduino_OV767X.h>

// Vi använder QCIF (176x144) i gråskala – 1 byte per pixel
#define CAM_W 176
#define CAM_H 144

// Buffer där hela kamerabilden hamnar (176 * 144 bytes)
uint8_t frame[CAM_W * CAM_H];

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // vänta på Serial
  }

  Serial.println("Startar kamera...");

  // Starta kameran: QCIF-upplösning, GRAYSCALE, clock divisor 1
  if (!Camera.begin(QCIF, GRAYSCALE, 1)) {
    Serial.println("Kunde inte initiera kameran!");
    while (1) {
      delay(1000);
    }
  }

  Serial.print("Kamera igång. Upplösning: ");
  Serial.print(Camera.width());
  Serial.print(" x ");
  Serial.println(Camera.height()); // borde vara 176x144
}

void loop() {
  // Läs en hel frame till 'frame'-arrayen
  Camera.readFrame(frame);

  // Skriv ut de första 20 pixlarna så vi SER att något händer
  Serial.print("Första pixlar: ");
  for (int i = 0; i < 20; i++) {
    Serial.print(frame[i]);
    Serial.print(' ');
  }
  Serial.println();

  delay(500); // lite paus
}

