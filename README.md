# Card_project

**About the project**
This project implements a minimal Federated Learning (FL) setup on Arduino devices using Bluetooth Low Energy (BLE).
One device acts as a peripheral (client) and one as a central (server).
Each device trains the same neural network locally, after which the peripheral sends its weights to the central, they are averaged, and sent back to the peripheral.

**The Network**
The model is a fully connected feedforward neural network designed for low-resource embedded devices.
- Input layer: 32 × 32 binary image (flattened, 1024 inputs)
- Hidden layer: 10 neurons with ReLU activation
- Output layer: 4 neurons with Softmax activation

The network is trained using stochastic gradient descent with backpropagation and a fixed learning rate.
All weights and biases are stored in a contiguous memory buffer to allow efficient parameter packing and transmission during federated learning



**Before running the project:**
Install:
Tools > Board > Board Manager..., search for Arduino Mbed OS Nano Boards
Tools > Manage libraries..., search for ArduinoBLE and Arduino_APDS9960, and install them both



