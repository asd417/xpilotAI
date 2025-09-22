#ifndef MLP_H
#define MLP_H

typedef struct Layer {
  float *weights;
  int wW; // weight Width
  int wH; // weight Height
  float *biases; // size wH
  float *out;    // size wH
} Layer;

typedef struct MLP {
  struct Layer *layers; // array of length layerCount
  int layerCount;
  int inputCount;
  int outputCount;
} MLP;

#endif
