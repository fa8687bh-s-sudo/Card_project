#include <iostream>
#include "NeuralNetwork.h"
#include "data.h"

float *weightsBiasArray = nullptr;

void setup() {
   
    weightsBiasArray = new float[numParams];
    
    createModel(weightsBiasArray);
    // Training epochs
    for (size_t i = 0; i < 10; i++) {
        trainModel(trainData, 0);
    }

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