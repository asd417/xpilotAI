#include <math.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Layer {
  double *weights;
  int wW; // weight Width
  int wH; // weight Height
  double *biases; // size wH
  double *out;    // size wH
} Layer;

typedef struct MLP {
  struct Layer *layers; // array of length layerCount
  int layerCount;
  int inputCount;
  int outputCount;
} MLP;

// Trying to train the given data set with just 1 perceptron
//  will not work because it is a non-linear function

// This program initializes a fully connected multilayered perceptron with
// matrices of weights and biases

double sigmoid(double x) { return 1.0f / (1.0f + exp(-x)); }

// returns a random double between min and max
double frand(double min, double max) {
  return fmax(fmin((max - min) * (double)rand() / ((double)RAND_MAX) + min, max),
              min);
}
int frandArray(int size, double min, double max, double **out) {
  *out = malloc(size * sizeof(double));
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
// row major
void dot(const double *m, const double *v, int mW, int mH, double *out) {
  for (int i = 0; i < mH; i++) {
    double acc = 0.0f;
    for (int j = 0; j < mW; j++) {
      acc += m[i * mW + j] * v[j];
    }
    out[i] = acc;
  }
}
// Vector add Vector
// Assume out memory allocated
void addV(double *v1, double *v2, int vSize, double *out) {
  for (int i = 0; i < vSize; i++) {
    out[i] = v1[i] + v2[i];
  }
}

int getOutputCount(MLP *network) {return network->layers[network->layerCount - 1].wH;}
double *getOutput(MLP *network) {return network->layers[network->layerCount - 1].out;}

void forwardSingle(Layer *layer, const double *in) {
  double *temp = malloc(layer->wH * sizeof(double));
  dot(layer->weights, in, layer->wW, layer->wH, temp);
  addV(temp, layer->biases, layer->wH, layer->out);
  for (int i = 0; i < layer->wH; i++) {
    layer->out[i] = sigmoid(layer->out[i]);
  }
}

void forward(MLP *network, const double *in) {
  const double *current = in;
  for (int i = 0; i < network->layerCount; i++) {
    forwardSingle(&network->layers[i], current);
    current = network->layers[i].out;
  }
}

// partial differentiation of sigmoid
double partial(double x) { return x * (1 - x); }

// pain
void backward(MLP *network, double *networkInput, double *targetOutput, double lr) {
  const int lc = network->layerCount;
  Layer *layer     = &network->layers[lc - 1]; //output layer
  Layer *nextLayer = (lc > 1) ? &network->layers[lc - 2] : NULL;
  const double *in  = (lc > 1) ? nextLayer->out : networkInput; // check if the network is just a perceptron

  // output layer error gradient
  double *egLast = malloc(layer->wH * sizeof(double));
  for (int i = 0; i < layer->wH; ++i) {
    egLast[i] = partial(layer->out[i]) * (targetOutput[i] - layer->out[i]);
  }
  // output layer delta weight
  double *dw = malloc(layer->wH * layer->wW * sizeof(double));
  double *db = malloc(layer->wH * sizeof(double));
  for (int j = 0; j < layer->wH; ++j) {
    for (int k = 0; k < layer->wW; ++k) {
      dw[j * layer->wW + k] = lr * in[k] * egLast[j];
    }
    db[j] = lr * egLast[j];
  }
  // start looping
  // start from output layer
  for (int l = lc - 1; l >= 0; --l) {
    layer = &network->layers[l];
    nextLayer = (l > 0) ? &network->layers[l - 1] : NULL;

    // update weight of the last layer
    // on first iter, this updates output layer weights and biases
    for (int j = 0; j < layer->wH; ++j) {
      for (int k = 0; k < layer->wW; ++k) {
        layer->weights[j * layer->wW + k] += dw[j * layer->wW + k];
      }
      layer->biases[j] += db[j];
    }
    // reached input layer
    if (l == 0) break;
    
    // error gradient for next layer
    double *egNew = malloc(nextLayer->wH * sizeof(double));
    for (int i = 0; i < nextLayer->wH; ++i) {
      double acc = 0.0f;
      for (int j = 0; j < layer->wH; ++j) {
        acc += egLast[j] * layer->weights[j * layer->wW + i]; // sum error
      }
      egNew[i] = acc * partial(nextLayer->out[i]);
    }

    // delta weight biases of next layer
    const double *inNext = (l > 1) ? network->layers[l - 2].out : networkInput;
    double *dwNext = malloc(nextLayer->wH * nextLayer->wW * sizeof(double));
    double *dbNext = malloc(nextLayer->wH * sizeof(double));

    for (int j = 0; j < nextLayer->wH; ++j) {
      for (int k = 0; k < nextLayer->wW; ++k) {
        dwNext[j * nextLayer->wW + k] = lr * inNext[k] * egNew[j];
      }
      dbNext[j] = lr * egNew[j]; // book says multiply by -1 but doesnt work here..?
    }
    // swap buffers
    free(egLast);
    free(dw);
    free(db);
    egLast = egNew;
    dw = dwNext;
    db = dbNext;
  }
  // clean
  free(egLast);
  free(dw);
  free(db);
}

// for example:
// 2 inputs, 1 hidden layer of size 2, 1 output -> [2,2,1]
int createMLP(int *nodes, int nodesSize, MLP **out) {
  if (!out || !nodes || nodesSize < 2)
    return 1;
  *out = malloc(sizeof(MLP));
  (*out)->layerCount = nodesSize - 1;
  (*out)->layers = calloc((nodesSize - 1), sizeof(Layer));
  int error = 0, pos = 0;
  for (int i = 0; i < nodesSize - 1; i++) {
    if (i == 0)
      (*out)->inputCount = nodes[i];
    // printf("%d", i);
    if (i == nodesSize - 2)
      (*out)->outputCount = nodes[i + 1];
    int layerIn = nodes[i];
    int layerOut = nodes[i + 1];
    error += frandArray(layerIn * layerOut, -2.4f / layerIn, 2.4f / layerIn,
                        &((*out)->layers[i].weights));
    error += frandArray(layerOut, -1.0f, 1.0f, &((*out)->layers[i].biases));
    (*out)->layers[i].out = calloc(layerOut, sizeof(double));
    (*out)->layers[i].wW = layerIn;
    (*out)->layers[i].wH = layerOut;
    if (error)
      break; // catch malloc fail
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

typedef struct {
  int inputCount;
  int outputCount;
  double *expectedIn;
  double *expectedOut;
} TrainingData;

// dataset is an array of TrainingData
void train(MLP *network, int epoch, double lr, TrainingData *dataset, int datasetSize, int reportEvery) {
  for (int e = 0; e < epoch; e++) {
    double error = 0.0f;
    for (int i = 0; i < datasetSize; i++) {
      double *in = dataset[i].expectedIn;
      double *expectedO = dataset[i].expectedOut;
      forward(network, in);
      double *realO = getOutput(network);
      for (int i = 0; i < network->outputCount; i++) {
        error += fabs(expectedO[i] - realO[i]);
        //printf("\tExpected %f, Real %f\n", expectedO[i], realO[i]);
      }
      backward(network, in, expectedO, lr);
    }
    if (e % reportEvery == 0) {
      printf("Epoch %d\n", e);
      printf("\tEpoch Mean Error %f\n", error / datasetSize);
    }
  }
}

int createDataset(TrainingData **out) {
  int inputs = 5;
  int outputs = 1;
  int dataSize = pow(2, inputs);
  *out = malloc(dataSize * sizeof(TrainingData));
  for (int i = 0; i < dataSize; i++) {
    (*out)[i].inputCount = inputs;
    (*out)[i].outputCount = outputs;
    (*out)[i].expectedIn = malloc(inputs * sizeof(double));
    (*out)[i].expectedOut = malloc(outputs * sizeof(double));
    double sum = 0;
    for (int j = inputs; j > 0; j--) {
      double v = (double)((i >> (j - 1)) & 1); // shift right and mask
      (*out)[i].expectedIn[inputs - j] = v;
      sum += v;
    }
    (*out)[i].expectedOut[0] = sum > 3.9f || sum < 1.1f ? 1.0f : 0.0f;
  }
  return dataSize;
}

#define NODESSIZE 3
#define INPUTSIZE 5
#define OUTPUTSIZE 1
int main() {
  int nodes[NODESSIZE] = {INPUTSIZE, 3, OUTPUTSIZE};
  TrainingData *dataset;
  int datasetSize = createDataset(&dataset);
  MLP *network;
  printf("MLP\n  Structure: ");
  for (int i = 0; i < NODESSIZE; i++) {
    printf("%d ", nodes[i]);
  }
  printf("\n");
  double lr = 1.0f;
  int maxEpoch = 10000;
  printf("  Learning Rate: %f\n", lr);
  printf("  Epoch: %d\n", maxEpoch);
  createMLP(nodes, NODESSIZE, &network);
  train(network, maxEpoch, lr, dataset, datasetSize, 1000);
  return 0;
}