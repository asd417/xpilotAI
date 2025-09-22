#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "mlp.h"
#include "mlp_print.h"

typedef struct {
  int inputCount;
  float *expectedIn;
  float expectedOut;
} TrainData;

// Trying to train the given data set with just 1 perceptron
//  will not work because it is a non-linear function

// This program initializes a fully connected multilayered perceptron with
// matrices of weights and biases

float sigmoid(float x) { return 1.0f / (1.0f + exp(-x)); }

// returns a random float between min and max
float frand(float min, float max) {
  return fmax(fmin((max - min) * (float)rand() / ((float)RAND_MAX) + min, max),
              min);
}
int frandArray(int size, float min, float max, float **out) {
  *out = malloc(size * sizeof(float));
  if (*out == NULL) {
    return 1;
  }
  for (int i = 0; i < size; i++) {
    (*out)[i] = frand(min, max);
  }
  return 0;
}
// Matrix dot Vector
// Assume vW = 1, vH = mW
// Assume out memory allocated
void dot(const float *m, const float *v, int mW, int mH, float* out) {
  for (int i = 0; i < mH; i++) {
    const float *row = &m[i * mW];
    float acc = 0.0f;
    for (int j = 0; j < mW; j++) {
      acc += row[j] * v[j];
    }
    out[i] = acc;
  }
}
// Vector add Vector
// Assume out memory allocated
void addV(float *v1, float *v2, int vSize, float* out) {
  for (int i = 0; i < vSize; i++) {
    out[i] = v1[i] + v2[i];
  }
}

int getOutputCount(MLP *network) {
  return network->layers[network->layerCount - 1].wH;
}
float *getOutput(MLP *network) {
  return network->layers[network->layerCount - 1].out;
}

void forwardSingle(Layer *layer, const float *in) {
  dot(layer->weights, in, layer->wW, layer->wH, layer->out);
  addV(layer->out, layer->biases, layer->wH, layer->out);
  for (int i = 0; i < layer->wH; i++) {
    layer->out[i] = sigmoid(layer->out[i]);
  }
}
void forward(MLP *network, const float *in) {
  const float *current = in;
  for (int i = 0; i < network->layerCount; i++) {
    forwardSingle(&network->layers[i], current);
    current = network->layers[i].out;
  }
}

int maxLayerOut(MLP *network) {
  int maxSize = 0;
  for (int i = 0; i < network->layerCount; i++) {
    maxSize = max(network->layers[i].wH, maxSize);
  }
  return maxSize;
}

int maxLayerWeights(MLP *network) {
  int maxSize = 0;
  for (int i = 0; i < network->layerCount; i++) {
    maxSize = max(network->layers[i].wH * network->layers[i].wW, maxSize);
  }
  return maxSize;
}

void backward(MLP *network, float* in, float *targetOutput, float lr) {
  float *errorGradientFront = malloc(maxLayerOut(network) * sizeof(float));
  float *errorGradientBack = malloc(maxLayerOut(network) * sizeof(float));
  float *deltaW = malloc(maxLayerWeights(network) * sizeof(float));
  Layer* targetLayer = &network->layers[network->layerCount-1]; // start with last layer
  const float *currentOut = getOutput(network); // start with network output
  const float *currentIn; // input to the last year
  if (network->layerCount == 1) {
    currentIn = in; // no hidden layer
  }
  else {
    currentIn = network->layers[network->layerCount - 2].out; // hidden state of the previous layer
  }
  // error gradient of the output layer (write it to front buffer)
  for (int i = 0; i < targetLayer->wH; i++) {
    errorGradientFront[i] = currentOut[i] * (1 - currentOut[i]) * (targetOutput[i] - currentOut[i]);
  }
  // delta weight of the target layer (read from front buffer) dont update weight here we need it for error gradient of the next layer
  for (int k = 0; k < targetLayer->wH; k++) {
    for (int j = 0; j < targetLayer->wW; j++) {
      deltaW[k*targetLayer->wW+j] = lr * currentIn[j] * errorGradientFront[k];
    }
  }
  // error gradient for the hidden layer (read from front buffer and write it to back buffer)
  for (int j = 0; j < targetLayer->wW; j++) {
    float error = 0.0f;
    for (int k = 0; k < targetLayer->wH; k++) {
      error += errorGradientFront[k] * targetLayer->weights[k*targetLayer->wW+j];
    }
    errorGradientBack[j] = currentIn[j] * (1-currentIn[j]) * error;
  }
  // apply delta weight
  for (int k = 0; k < targetLayer->wH; k++) {
    for (int j = 0; j < targetLayer->wW; j++) {
      targetLayer->weights[k*targetLayer->wW+j] += deltaW[k*targetLayer->wW+j];
    }
  }
  // move target layer
  targetLayer--;
  currentIn = in;
  // swap errorgradient buffers
  float *temp = errorGradientBack;
  errorGradientBack = errorGradientFront;
  errorGradientFront = temp;
  // delta weight of the target layer (read from front buffer)
  for (int k = 0; k < targetLayer->wH; k++) {
    for (int j = 0; j < targetLayer->wW; j++) {
      deltaW[k*targetLayer->wW+j] = lr * currentIn[j] * errorGradientFront[k];
    }
  }
  // apply delta weight
  for (int k = 0; k < targetLayer->wH; k++) {
    for (int j = 0; j < targetLayer->wW; j++) {
      targetLayer->weights[k*targetLayer->wW+j] += deltaW[k*targetLayer->wW+j];
    }
  }
}

// for example:
// 2 inputs, 1 hidden layer of size 2, 1 output -> [2,2,1]
int createMLP(int *nodes, int nodesSize, MLP **out) {
  if (!out || !nodes || nodesSize < 2) return 1;
  *out = malloc(sizeof(MLP));
  (*out)->layerCount = nodesSize - 1;
  (*out)->layers = calloc((nodesSize - 1), sizeof(Layer));
  int error=0, pos=0;
  for (int i = 0; i < nodesSize - 1; i++) {
    if (i == 0)
      (*out)->inputCount = nodes[i];
    //printf("%d", i);
    if (i == nodesSize - 2)
      (*out)->outputCount = nodes[i + 1];
    int layerIn = nodes[i];
    int layerOut = nodes[i + 1];
    error += frandArray(layerIn * layerOut, -2.4f / layerIn, 2.4f / layerIn,
                        &((*out)->layers[i].weights));
    error += frandArray(layerOut, -1.0f, 1.0f, &((*out)->layers[i].biases));
    (*out)->layers[i].out = calloc(layerOut, sizeof(float));
    (*out)->layers[i].wW = layerIn;
    (*out)->layers[i].wH = layerOut;
    if (error) break; // catch malloc fail
    pos = i;
  }
  if (error) {
    printf("Error while initializing network");
    // cleanup
    for (int i = 0; i <= pos; i++) {
      free((*out)->layers[i].weights);
      free((*out)->layers[i].biases);
    }
    free((*out)->layers);
    free(*out);
    *out = NULL;
  }
  return 0;
}

#define NODESSIZE 3
#define INPUTSIZE 2
#define OUTPUTSIZE 1
int main() {
  int nodes[NODESSIZE] = {INPUTSIZE, 2, OUTPUTSIZE};
  float inputs[INPUTSIZE] = {2,3};
  MLP* network;
  createMLP(nodes, NODESSIZE, &network);
  printf("MLP input size: %d output size: %d\n", network->inputCount,
         network->outputCount);
  forward(network, inputs);
  float *outs = getOutput(network);
  for (int i = 0; i < network->outputCount; i++) {
    printf("%f ", outs[i]);
  }
  printMLP(network);
  return 0;
}