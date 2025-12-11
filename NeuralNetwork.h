#include <cstddef>
#include <cmath>
#include "data.h"

#define LEARNING_RATE 0.01

float *weightsAndBiasStorage = nullptr;
int numParams = 0;

struct Neuron
{
    float *weights;
    float bias;
    float preActivation;
    float postActivation;
    float *weightDerivatives;
    float biasDerivative;
    float outputDerivative;
};

struct Layer
{
    Neuron *neurons;
};

Layer *layers = nullptr;
size_t layerSizes[] = {576, 10, 4}; // 24x24 image
size_t nbrLayers = sizeof(layerSizes) / sizeof(layerSizes[0]);

Neuron createNeuron(size_t layerIndex)
{
    Neuron neuron;
    neuron.weights = new float[layerSizes[layerIndex - 1]]();
    neuron.weightDerivatives = new float[layerSizes[layerIndex - 1]]();
    for (size_t input = 0; input < layerSizes[layerIndex - 1]; input++)
    {
        neuron.weights[input] = (rand() * 1.0 / RAND_MAX - 0.5) * 2;
        // neuron.weights[input] = (float) random(-1000, 1000) / 1000.0; // Check if this works on Arduino, otherwise change
    }
    neuron.bias = (rand() * 1.0 / RAND_MAX - 0.5) * 2;
    // neuron.bias = (float) random(-1000, 1000) / 1000.0; // Check if this works on Arduino, otherwise change
    return neuron;
}

float calculatePreActivation(size_t layerIndex, size_t neuronIndex)
{
    size_t nbrInputs = layerSizes[layerIndex - 1];
    float sum = 0;
    for (size_t input = 0; input < nbrInputs; input++)
    {
        sum += layers[layerIndex].neurons[neuronIndex].weights[input] * layers[layerIndex - 1].neurons[input].postActivation;
    }
    sum += layers[layerIndex].neurons[neuronIndex].bias;
    return sum;
}

void applySoftmax(size_t layerIndex)
{
    size_t nbrNeurons = layerSizes[layerIndex];

    float maxValue = layers[layerIndex].neurons[0].preActivation;
    for (size_t neuron = 1; neuron < nbrNeurons; neuron++)
    {
        if (layers[layerIndex].neurons[neuron].preActivation > maxValue)
        {
            maxValue = layers[layerIndex].neurons[neuron].preActivation;
        }
    }

    float sum = 0;
    for (size_t neuron = 0; neuron < nbrNeurons; neuron++)
    {
        layers[layerIndex].neurons[neuron].postActivation = exp(layers[layerIndex].neurons[neuron].preActivation - maxValue);
        sum += layers[layerIndex].neurons[neuron].postActivation;
    }

    for (size_t neuron = 0; neuron < nbrNeurons; neuron++)
    {
        layers[layerIndex].neurons[neuron].postActivation /= sum;
    }
}

void updateOutputDerivative(size_t layerIndex, size_t neuronIndex, float *correctArray)
{
    if (layers[layerIndex].neurons[neuronIndex].preActivation <= 0)
    {
        layers[layerIndex].neurons[neuronIndex].outputDerivative = 0;
        return;
    }
    float sum = 0;
    for (size_t output = 0; output < layerSizes[layerIndex + 1]; output++)
    {
        sum += layers[layerIndex + 1].neurons[output].weights[neuronIndex] * layers[layerIndex + 1].neurons[output].outputDerivative;
    }
    layers[layerIndex].neurons[neuronIndex].outputDerivative = sum;
}

void updateWeightAndBiasDerivatives(size_t layerIndex, size_t neuronIndex)
{
    size_t nbrInputs = layerSizes[layerIndex - 1];
    for (size_t input = 0; input < nbrInputs; input++)
    {
        layers[layerIndex].neurons[neuronIndex].weightDerivatives[input] = -LEARNING_RATE * layers[layerIndex].neurons[neuronIndex].outputDerivative * layers[layerIndex - 1].neurons[input].postActivation;
    }
    layers[layerIndex].neurons[neuronIndex].biasDerivative = -LEARNING_RATE * layers[layerIndex].neurons[neuronIndex].outputDerivative;
}

void updateWeightsAndBias(size_t layerIndex, size_t neuronIndex)
{
    size_t nbrInputs = layerSizes[layerIndex - 1];
    for (size_t input = 0; input < nbrInputs; input++)
    {
        layers[layerIndex].neurons[neuronIndex].weights[input] += layers[layerIndex].neurons[neuronIndex].weightDerivatives[input];
    }
    layers[layerIndex].neurons[neuronIndex].bias += layers[layerIndex].neurons[neuronIndex].biasDerivative;
}

void forwardPropagation(const float *input)
{
    // Set output values of first layer
    for (size_t neuron = 0; neuron < layerSizes[0]; neuron++)
    {
        layers[0].neurons[neuron].postActivation = input[neuron];
    }

    // Calculate output values for all hidden layers
    for (size_t layer = 1; layer < nbrLayers - 1; layer++)
    {
        for (size_t neuron = 0; neuron < layerSizes[layer]; neuron++)
        {
            layers[layer].neurons[neuron].preActivation = calculatePreActivation(layer, neuron);
            layers[layer].neurons[neuron].postActivation = fmaxf(0.0f, layers[layer].neurons[neuron].preActivation);
        }
    }

    // Calculate output values for last layer
    for (size_t neuron = 0; neuron < layerSizes[nbrLayers - 1]; neuron++) {
        layers[nbrLayers - 1].neurons[neuron].preActivation = calculatePreActivation(nbrLayers - 1, neuron);
    }
    applySoftmax(nbrLayers - 1);
}

void backwardPropagation(float *correctArray) {
    for (size_t neuron = 0; neuron < layerSizes[nbrLayers - 1]; neuron++) {
        layers[nbrLayers - 1].neurons[neuron].outputDerivative = layers[nbrLayers - 1].neurons[neuron].postActivation - correctArray[neuron];
        updateWeightAndBiasDerivatives(nbrLayers - 1, neuron);
        updateWeightsAndBias(nbrLayers - 1, neuron);
    }

    for (int layer = (int)nbrLayers - 2; layer >= 1; layer--) {
        for (size_t neuron = 0; neuron < layerSizes[layer]; neuron++) {
            updateOutputDerivative(layer, neuron, correctArray);
            updateWeightAndBiasDerivatives(layer, neuron);
            updateWeightsAndBias(layer, neuron);
        }
    }
}

void createModel(float *weightsBiasArray) {
    weightsAndBiasStorage = weightsBiasArray;
    layers = new Layer[nbrLayers]();
    layers[0].neurons = new Neuron[layerSizes[0]]();
    for (size_t layer = 1; layer < nbrLayers; layer++) {
        layers[layer].neurons = new Neuron[layerSizes[layer]]();
        for (size_t neuron = 0; neuron < layerSizes[layer]; neuron++) {
            layers[layer].neurons[neuron] = createNeuron(layer);
        }
    }
}

void trainModel(const float *input, size_t correctSuit) {
    float *correctArray = new float[layerSizes[nbrLayers - 1]]();
    correctArray[correctSuit] = 1.0f;
    forwardPropagation(input);

    for (size_t layer = 1; layer < nbrLayers; layer++) { // For testing
        std::cout << "Layer " << layer;
        std::cout << std::endl;
        for (size_t neuron = 0; neuron < layerSizes[layer]; neuron++)
        {
            std::cout << "Pre: " << layers[layer].neurons[neuron].preActivation;
            std::cout << std::endl;
            std::cout << "Post: " << layers[layer].neurons[neuron].postActivation;
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }

    backwardPropagation(correctArray);
    delete[] correctArray;
}

void trainModelAllImages(){
    for(int i = 0; i < NUM_TRAIN_SAMPLES; i++) {
        trainModel(trainImages[i], trainLabels[i]);
    }
}

float* inference(const float* input) {
    forwardPropagation(input);
    float* prediction = new float[layerSizes[nbrLayers - 1]];
    for (size_t neuron = 0; neuron < layerSizes[nbrLayers - 1]; neuron++){
        prediction[neuron] = layers[nbrLayers - 1].neurons[neuron].postActivation;
    }
    return prediction;
}

// Uses validation data and inference() method. 
float calculateAccuracy(){
    size_t correctAnswers  = 0;

    for(size_t i = 0; i < NUM_VAL_SAMPLES; i++) {
        float* prediction = inference(valImages[i]);
        float tempMax =  -1.0f;
        size_t tempIndex = 0;

        for(size_t j = 0; j < layerSizes[nbrLayers-1]; j++) {
            if(prediction[j] > tempMax) {
                tempMax = prediction[j];
                tempIndex = j;
            }
        }
        size_t pred = tempIndex;
        size_t label = valLabels[i];
        if(pred == label) {
            correctAnswers++;
        }
        delete[] prediction;
    }
    float accuracy = 1.0f * correctAnswers / NUM_VAL_SAMPLES;
    return accuracy;
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


