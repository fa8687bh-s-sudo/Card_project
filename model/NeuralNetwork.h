#pragma once
#include <cstddef>
#include <cmath>
#include <Arduino.h>

#define LEARNING_RATE 0.001

float *weightsAndBiasStorage = nullptr;
size_t weightBiasOffset = 0;
int numParams = 0;
const size_t RESOLUTION = 32; 
size_t calculateMaxParam();
constexpr int TOTAL_PARAMS = 10294; 
const size_t MAX_PARAMS = 11000; 
constexpr size_t NBR_CLASSES = 4;

struct Neuron {
    float *weights;
    float bias;
    float preActivation;
    float postActivation;
    float biasDerivative;
    float outputDerivative;
};

struct Layer {
    Neuron *neurons;
};

Layer *layers = nullptr;
size_t layerSizes[] = {RESOLUTION * RESOLUTION, 10, NBR_CLASSES};
size_t nbrLayers = sizeof(layerSizes) / sizeof(layerSizes[0]);

Neuron createNeuron(size_t layerIndex) {
    Neuron neuron{};
    const size_t nbrInputs = layerSizes[layerIndex - 1];
    neuron.weights = &weightsAndBiasStorage[weightBiasOffset];
    weightBiasOffset += nbrInputs;
    for (size_t input = 0; input < nbrInputs; input++) {
        neuron.weights[input] = (float) random(-1000, 1000) / 1000.0 * 0.05f; // Last number determines +/- bounds of random distribution
    }
    if (layerIndex == nbrLayers - 1) {
        neuron.bias = 0.0f;
    } else {
        neuron.bias = 0.1f; // Slightly positive bias to avoid "dead" ReLUs
    }
    weightsAndBiasStorage[weightBiasOffset] = neuron.bias;
    weightBiasOffset += 1;
    return neuron;
}

float calculatePreActivation(size_t layerIndex, size_t neuronIndex) {
    size_t nbrInputs = layerSizes[layerIndex - 1];
    float sum = 0;
    for (size_t input = 0; input < nbrInputs; input++) {
        sum += layers[layerIndex].neurons[neuronIndex].weights[input] * layers[layerIndex - 1].neurons[input].postActivation;
    }
    sum += layers[layerIndex].neurons[neuronIndex].bias;
    return sum;
}

void applySoftmax(size_t layerIndex) {
    size_t nbrNeurons = layerSizes[layerIndex];

    float maxValue = layers[layerIndex].neurons[0].preActivation;
    for (size_t neuron = 1; neuron < nbrNeurons; neuron++) {
        if (layers[layerIndex].neurons[neuron].preActivation > maxValue) {
            maxValue = layers[layerIndex].neurons[neuron].preActivation;
        }
    }

    float sum = 0;
    for (size_t neuron = 0; neuron < nbrNeurons; neuron++) {
        layers[layerIndex].neurons[neuron].postActivation = exp(layers[layerIndex].neurons[neuron].preActivation - maxValue);
        sum += layers[layerIndex].neurons[neuron].postActivation;
    }

    for (size_t neuron = 0; neuron < nbrNeurons; neuron++) {
        layers[layerIndex].neurons[neuron].postActivation /= sum;
    }
}

void updateOutputDerivative(size_t layerIndex, size_t neuronIndex, float *correctArray) {
    if (layers[layerIndex].neurons[neuronIndex].preActivation <= 0) {
        layers[layerIndex].neurons[neuronIndex].outputDerivative = 0;
        return;
    }
    float sum = 0;
    for (size_t output = 0; output < layerSizes[layerIndex + 1]; output++) {
        sum += layers[layerIndex + 1].neurons[output].weights[neuronIndex] * layers[layerIndex + 1].neurons[output].outputDerivative;
    }
    layers[layerIndex].neurons[neuronIndex].outputDerivative = sum;
}

void updateWeightsAndBias(size_t layerIndex, size_t neuronIndex){
    size_t nbrInputs = layerSizes[layerIndex - 1];
    float delta = -LEARNING_RATE * layers[layerIndex].neurons[neuronIndex].outputDerivative;
    for (size_t input = 0; input < nbrInputs; input++) {
        layers[layerIndex].neurons[neuronIndex].weights[input] += delta * layers[layerIndex - 1].neurons[input].postActivation;;
    }
    layers[layerIndex].neurons[neuronIndex].bias += delta;
}

void forwardPropagation(const uint8_t *input){
    for (size_t neuron = 0; neuron < layerSizes[0]; neuron++) {
        layers[0].neurons[neuron].postActivation = 1.0f - (float) input[neuron]; // Invert so the symbols are made of 1s instead of 0s
    }

    for (size_t layer = 1; layer < nbrLayers - 1; layer++) {
        for (size_t neuron = 0; neuron < layerSizes[layer]; neuron++) {
            layers[layer].neurons[neuron].preActivation = calculatePreActivation(layer, neuron);
            layers[layer].neurons[neuron].postActivation = fmaxf(0.0f, layers[layer].neurons[neuron].preActivation);
        }
    }

    for (size_t neuron = 0; neuron < layerSizes[nbrLayers - 1]; neuron++) {
        layers[nbrLayers - 1].neurons[neuron].preActivation = calculatePreActivation(nbrLayers - 1, neuron);
    }
    applySoftmax(nbrLayers - 1);
}

void backwardPropagation(float *correctArray) {
    for (size_t neuron = 0; neuron < layerSizes[nbrLayers - 1]; neuron++) {
        layers[nbrLayers - 1].neurons[neuron].outputDerivative = layers[nbrLayers - 1].neurons[neuron].postActivation - correctArray[neuron];
        updateWeightsAndBias(nbrLayers - 1, neuron);
    }

    for (int layer = (int)nbrLayers - 2; layer >= 1; layer--) {
        for (size_t neuron = 0; neuron < layerSizes[layer]; neuron++) {
            updateOutputDerivative(layer, neuron, correctArray);
            updateWeightsAndBias(layer, neuron);
        }
    }
}

void createModel(float *weightsBiasArray) {
    weightsAndBiasStorage = weightsBiasArray;
    weightBiasOffset = 0;
    layers = new Layer[nbrLayers]();
    layers[0].neurons = new Neuron[layerSizes[0]]();
    for (size_t layer = 1; layer < nbrLayers; layer++) {
        layers[layer].neurons = new Neuron[layerSizes[layer]]();
        for (size_t neuron = 0; neuron < layerSizes[layer]; neuron++) {
            layers[layer].neurons[neuron] = createNeuron(layer);
        }
    }
}

void trainModel(const uint8_t *input, size_t correctSuit) {
    if (!layers || !layers[0].neurons) {
        return;
    }
    if (correctSuit >= NBR_CLASSES) {
        return;
    }
    static float correctArray[NBR_CLASSES];
    for (size_t i = 0; i < NBR_CLASSES; i++) {
        correctArray[i] = 0.0f;
    }
    correctArray[correctSuit] = 1.0f;
    forwardPropagation(input);
    backwardPropagation(correctArray);
}

void inference(const uint8_t* input, float prediction[NBR_CLASSES]) {
    forwardPropagation(input);
    for (size_t neuron = 0; neuron < layerSizes[nbrLayers - 1]; neuron++){
        prediction[neuron] = layers[nbrLayers - 1].neurons[neuron].postActivation;
    }
}

void updateAccuracy(const uint8_t* image, size_t label, size_t& correctCount, size_t& totalCount){
    float prediction[NBR_CLASSES];
    inference(image, prediction);
    float max =  -1.0f;
    size_t maxIndex = 0;
    
    for(size_t neuron = 0; neuron < NBR_CLASSES; neuron++) {
        /*
        Serial.print("Probability of being ");
        Serial.print(neuron);
        Serial.print(": ");
        Serial.println(prediction[neuron]);
        */
        if (prediction[neuron] > max) {
            max = prediction[neuron];
            maxIndex = neuron;
        }
    }

    Serial.print("PREDICTION: ");
    Serial.println(maxIndex);
    Serial.print("CORRECT: ");
    Serial.println(label);
    Serial.println();

    if(maxIndex == label) {
        correctCount++;
    }
    totalCount++;
}

void packWeights() {
    int counter = 0;
    for (size_t layer = 1; layer < nbrLayers; layer++) {
        for (int neuron = 0; neuron < layerSizes[layer]; neuron++) {
            for (size_t i = 0; i < layerSizes[layer - 1]; i++) {
                weightsAndBiasStorage[counter] = layers[layer].neurons[neuron].weights[i];
                counter++;
            }
            weightsAndBiasStorage[counter] = layers[layer].neurons[neuron].bias;
            counter++;
        }
    }
    numParams = counter;
    Serial.print("After packWeights, numParams = ");
    Serial.println(numParams);
}

void unpackWeights() {
    size_t counter = 0;
    for (size_t layer = 1; layer < nbrLayers; layer++) {
        for (size_t neuron = 0; neuron < layerSizes[layer]; neuron++) {
            for (size_t i = 0; i < layerSizes[layer - 1]; i++) {
                layers[layer].neurons[neuron].weights[i] = weightsAndBiasStorage[counter];
                counter++;
            }
            layers[layer].neurons[neuron].bias = weightsAndBiasStorage[counter];
            counter++;
        }
    }
}

void averageWeights() {
    size_t counter = 0;
    for (size_t layer = 1; layer < nbrLayers; layer++) {
        for (size_t neuron = 0; neuron < layerSizes[layer]; neuron++) {
            for (size_t i = 0; i < layerSizes[layer - 1]; i++) {
                layers[layer].neurons[neuron].weights[i] = (weightsAndBiasStorage[counter] + layers[layer].neurons[neuron].weights[i]) / 2.0;
                weightsAndBiasStorage[counter] = layers[layer].neurons[neuron].weights[i];
                counter++;
            }
            layers[layer].neurons[neuron].bias = (weightsAndBiasStorage[counter] + layers[layer].neurons[neuron].bias) / 2.0;
            weightsAndBiasStorage[counter] = layers[layer].neurons[neuron].bias;
            counter++;
        }
    }
}

size_t calculateMaxParam() {
  size_t MAX_PARAMS = 0;        
  for(size_t i = 0; i < nbrLayers-1; i++) {
    MAX_PARAMS += (layerSizes[i]+1)*(layerSizes[i+1]);
  }
  return MAX_PARAMS;
}


