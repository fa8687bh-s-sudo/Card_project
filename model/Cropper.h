#pragma once
#include <Arduino.h>
#include <stdint.h>

#define IMAGE_SIZE 128
#define PATCH_SIZE 32

static inline void visitedClear(uint8_t* bits, size_t nbytes) {
  memset(bits, 0, nbytes);
}

static inline bool visitedGet(const uint8_t* bits, uint16_t idx) {
  return (bits[idx >> 3] >> (idx & 7)) & 1;
}

static inline void visitedSet(uint8_t* bits, uint16_t idx) {
  bits[idx >> 3] |= (1u << (idx & 7));
}

// Pack (x,y) into a single 16-bit value since 0..127 fits in 7 bits each
static inline uint16_t packXY(uint8_t x, uint8_t y) {
  return (uint16_t(y) << 8) | uint16_t(x);
}
static inline uint8_t unpackX(uint16_t v) { return uint8_t(v & 0xFF); }
static inline uint8_t unpackY(uint16_t v) { return uint8_t(v >> 8); }

struct ComponentBox {
  int minX, minY, maxX, maxY;
  int size;
};

// fullImage is bit-packed (1 bit per pixel), MSB first in each byte
static inline uint8_t getPackedPixel(const uint8_t* packed, int x, int y) {
  const int i = y * IMAGE_SIZE + x;
  const int byteIndex = i >> 3;
  const int bitIndex  = 7 - (i & 7);
  return (packed[byteIndex] >> bitIndex) & 1;
}

// If you *already* have an unpacked 0/1 image, keep this.
static void unpackImage(const uint8_t* packed, uint8_t* outUnpacked /* IMAGE_SIZE*IMAGE_SIZE */) {
  for (int y = 0; y < IMAGE_SIZE; y++) {
    for (int x = 0; x < IMAGE_SIZE; x++) {
      outUnpacked[y * IMAGE_SIZE + x] = getPackedPixel(packed, x, y);
    }
  }
}

// Low-RAM flood fill:
// - visited is a bitset: (IMAGE_SIZE*IMAGE_SIZE)/8 bytes = 2048 bytes at 128x128
// - stack is uint16_t packed XY: IMAGE_SIZE*IMAGE_SIZE * 2 bytes = 32768 bytes
static ComponentBox findLargestRegion(
    const uint8_t* img /* unpacked IMAGE_SIZE*IMAGE_SIZE, values 0/1 */,
    int targetColor,
    int minX, int minY, int maxX, int maxY
) {
  static uint8_t visited[(IMAGE_SIZE * IMAGE_SIZE) / 8];
  static uint16_t stack[IMAGE_SIZE * IMAGE_SIZE];

  visitedClear(visited, sizeof(visited));

  ComponentBox best;
  best.size = 0;

  for (int y = minY; y <= maxY; y++) {
    for (int x = minX; x <= maxX; x++) {
      const uint16_t idx = uint16_t(y * IMAGE_SIZE + x);
      if (img[idx] != targetColor) continue;
      if (visitedGet(visited, idx)) continue;

      ComponentBox cur;
      cur.minX = cur.maxX = x;
      cur.minY = cur.maxY = y;
      cur.size = 0;

      uint16_t sp = 0;
      stack[sp++] = packXY(uint8_t(x), uint8_t(y));
      visitedSet(visited, idx);

      while (sp > 0) {
        const uint16_t v = stack[--sp];
        const int cx = unpackX(v);
        const int cy = unpackY(v);

        cur.size++;
        if (cx < cur.minX) cur.minX = cx;
        if (cx > cur.maxX) cur.maxX = cx;
        if (cy < cur.minY) cur.minY = cy;
        if (cy > cur.maxY) cur.maxY = cy;

        // 4-neighbors
        // (manually unrolled for speed + small code)
        // right
        if (cx + 1 <= maxX) {
          int nx = cx + 1, ny = cy;
          uint16_t nidx = uint16_t(ny * IMAGE_SIZE + nx);
          if (img[nidx] == targetColor && !visitedGet(visited, nidx)) {
            visitedSet(visited, nidx);
            stack[sp++] = packXY(uint8_t(nx), uint8_t(ny));
          }
        }
        // left
        if (cx - 1 >= minX) {
          int nx = cx - 1, ny = cy;
          uint16_t nidx = uint16_t(ny * IMAGE_SIZE + nx);
          if (img[nidx] == targetColor && !visitedGet(visited, nidx)) {
            visitedSet(visited, nidx);
            stack[sp++] = packXY(uint8_t(nx), uint8_t(ny));
          }
        }
        // down
        if (cy + 1 <= maxY) {
          int nx = cx, ny = cy + 1;
          uint16_t nidx = uint16_t(ny * IMAGE_SIZE + nx);
          if (img[nidx] == targetColor && !visitedGet(visited, nidx)) {
            visitedSet(visited, nidx);
            stack[sp++] = packXY(uint8_t(nx), uint8_t(ny));
          }
        }
        // up
        if (cy - 1 >= minY) {
          int nx = cx, ny = cy - 1;
          uint16_t nidx = uint16_t(ny * IMAGE_SIZE + nx);
          if (img[nidx] == targetColor && !visitedGet(visited, nidx)) {
            visitedSet(visited, nidx);
            stack[sp++] = packXY(uint8_t(nx), uint8_t(ny));
          }
        }

        // Safety: if stack would overflow, stop expanding this component
        // (shouldn't happen unless the whole area is one big component)
        if (sp >= IMAGE_SIZE * IMAGE_SIZE) break;
      }

      if (cur.size > best.size) best = cur;
    }
  }

  return best;
}

static void cropPatch(
    const uint8_t* img /* unpacked IMAGE_SIZE*IMAGE_SIZE */,
    const ComponentBox& box,
    uint8_t* patch /* PATCH_SIZE*PATCH_SIZE */
) {
  // fill with 1s
  memset(patch, 1, PATCH_SIZE * PATCH_SIZE);

  const int boxW = box.maxX - box.minX + 1;
  const int boxH = box.maxY - box.minY + 1;

  const int cropW = (boxW < PATCH_SIZE) ? boxW : PATCH_SIZE;
  const int cropH = (boxH < PATCH_SIZE) ? boxH : PATCH_SIZE;

  const int offX = (PATCH_SIZE - cropW) / 2;
  const int offY = (PATCH_SIZE - cropH) / 2;

  for (int y = 0; y < cropH; y++) {
    const int srcY = box.minY + y;
    for (int x = 0; x < cropW; x++) {
      const int srcX = box.minX + x;
      patch[(offY + y) * PATCH_SIZE + (offX + x)] = img[srcY * IMAGE_SIZE + srcX];
    }
  }
}

void cropImage(const uint8_t* fullImage, uint8_t* out) {
  static uint8_t unpacked[IMAGE_SIZE * IMAGE_SIZE];
  unpackImage(fullImage, unpacked);

  ComponentBox cardBox = findLargestRegion(unpacked, 1, 0, 0, IMAGE_SIZE - 1, IMAGE_SIZE - 1);
  ComponentBox blackBox = findLargestRegion(unpacked, 0, cardBox.minX, cardBox.minY, cardBox.maxX, cardBox.maxY);

  if (cardBox.size == 0 || blackBox.size == 0) {
    memset(out, 1, PATCH_SIZE * PATCH_SIZE);
    return;
  }

  cropPatch(unpacked, blackBox, out);
}
