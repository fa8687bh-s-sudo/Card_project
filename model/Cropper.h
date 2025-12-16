#include <iostream>
#include <fstream>
#include <stdint.h>
#include <vector>
#include <cstring>
#include <algorithm>

#define IMAGE_SIZE 128
#define PATCH_SIZE 32

void unpackImage(const uint8_t* packed, int rows, int cols, uint8_t image[IMAGE_SIZE][IMAGE_SIZE]) {
    for (int i = 0; i < rows * cols; i++) {
        int byteIndex = i / 8;
        int bitIndex  = 7 - (i % 8);
        image[i / cols][i % cols] = (packed[byteIndex] >> bitIndex) & 1;
    }
}

struct ComponentBox {
    int minX, minY, maxX, maxY;
    int size;
};

ComponentBox findLargestRegion(uint8_t image[IMAGE_SIZE][IMAGE_SIZE], int targetColor, int minX, int minY, int maxX, int maxY) {
    static uint8_t visited[IMAGE_SIZE][IMAGE_SIZE];
    memset(visited, 0, sizeof(visited));

    ComponentBox best;
    best.size = 0;

    static int stackX[IMAGE_SIZE * IMAGE_SIZE];
    static int stackY[IMAGE_SIZE * IMAGE_SIZE];

    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {

            if (image[y][x] != targetColor || visited[y][x])
                continue;

            ComponentBox current;
            current.minX = current.maxX = x;
            current.minY = current.maxY = y;
            current.size = 0;

            int sp = 0;
            stackX[sp] = x;
            stackY[sp] = y;
            sp++;
            visited[y][x] = 1;

            while (sp > 0) {
                sp--;
                int cx = stackX[sp];
                int cy = stackY[sp];

                current.size++;

                if (cx < current.minX) current.minX = cx;
                if (cx > current.maxX) current.maxX = cx;
                if (cy < current.minY) current.minY = cy;
                if (cy > current.maxY) current.maxY = cy;

                const int dx[4] = { 1, -1, 0, 0 };
                const int dy[4] = { 0, 0, 1, -1 };

                for (int i = 0; i < 4; i++) {
                    int nx = cx + dx[i];
                    int ny = cy + dy[i];

                    if (nx < minX || ny < minY || nx > maxX || ny > maxY)
                        continue;

                    if (image[ny][nx] == targetColor && !visited[ny][nx]) {
                        visited[ny][nx] = 1;
                        stackX[sp] = nx;
                        stackY[sp] = ny;
                        sp++;
                    }
                }
            }

            if (current.size > best.size)
                best = current;
        }
    }

    return best;
}

void cropPatch(uint8_t image[IMAGE_SIZE][IMAGE_SIZE], ComponentBox box, uint8_t patch[PATCH_SIZE][PATCH_SIZE]) {
    for (int y = 0; y < PATCH_SIZE; y++)
        for (int x = 0; x < PATCH_SIZE; x++)
            patch[y][x] = 1;

    int boxWidth  = box.maxX - box.minX + 1;
    int boxHeight = box.maxY - box.minY + 1;

    int cropWidth  = std::min(PATCH_SIZE, boxWidth);
    int cropHeight = std::min(PATCH_SIZE, boxHeight);

    int offsetX = (PATCH_SIZE - cropWidth) / 2;
    int offsetY = (PATCH_SIZE - cropHeight) / 2;

    for (int y = 0; y < cropHeight; y++)
        for (int x = 0; x < cropWidth; x++)
            patch[offsetY + y][offsetX + x] = image[box.minY + y][box.minX + x];
}

void printPatch(uint8_t patch[PATCH_SIZE][PATCH_SIZE]) {
    std::cout << "Patch pixels:\n";
    for (int y = 0; y < PATCH_SIZE; y++) {
        for (int x = 0; x < PATCH_SIZE; x++)
            std::cout << (int)patch[y][x];
        std::cout << "\n";
    }
}

void printCardBox(uint8_t image[IMAGE_SIZE][IMAGE_SIZE], ComponentBox box) {
    std::cout << "CardBox pixels:\n";
    for (int y = box.minY; y <= box.maxY; y++) {
        for (int x = box.minX; x <= box.maxX; x++)
            std::cout << (int)image[y][x];
        std::cout << "\n";
    }
}

uint8_t* cropImage(const uint8_t* fullImage) {
    uint8_t unpacked[IMAGE_SIZE][IMAGE_SIZE];
    unpackImage(fullImage, IMAGE_SIZE, IMAGE_SIZE, unpacked);

    ComponentBox cardBox = findLargestRegion(unpacked, 1, 0, 0, IMAGE_SIZE - 1, IMAGE_SIZE - 1);
    std::cout << "Card box: (" << cardBox.minX << "," << cardBox.minY << ") - (" << cardBox.maxX << "," << cardBox.maxY << ")\n";

    ComponentBox blackBox = findLargestRegion(unpacked, 0, cardBox.minX, cardBox.minY, cardBox.maxX, cardBox.maxY);
    std::cout << "Largest black region inside card: (" << blackBox.minX << "," << blackBox.minY << ") - (" << blackBox.maxX << "," << blackBox.maxY << ")\n";

    if (cardBox.size == 0 || blackBox.size == 0) {
        uint8_t* empty = new uint8_t[PATCH_SIZE * PATCH_SIZE];
        std::fill(empty, empty + PATCH_SIZE * PATCH_SIZE, 1);
        return empty;
    }

    uint8_t patch[PATCH_SIZE][PATCH_SIZE];
    cropPatch(unpacked, blackBox, patch);

    printCardBox(unpacked, cardBox);
    printPatch(patch);

    uint8_t* croppedImage = new uint8_t[PATCH_SIZE * PATCH_SIZE];
    for (int y = 0; y < PATCH_SIZE; y++)
        for (int x = 0; x < PATCH_SIZE; x++)
            croppedImage[y * PATCH_SIZE + x] = patch[y][x];

    return croppedImage;
}