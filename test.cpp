#include <iostream>
#include "NeuralNetwork.h"
#include "data.h"



void setup() {
    
    float* trainData = createMnistDigit0Version1();
    printMnistDigit(trainData);
    createModel();
    // Training epochs
    for (size_t i = 0; i < 10; i++) {
        trainModel(trainData, 0);
    }

    float* testData = createMnistDigit0Version2();
    float* prediction = inference(testData);
    for (size_t i = 0; i < 10; i++) {
        std::cout << prediction[i];
        std::cout << std::endl;
    }
}

void loop() {
    
}

int main() {
    std::srand(std::time(nullptr));
    setup();
    loop();
    return 0;
}