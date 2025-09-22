
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


typedef struct {
  int inputCount;
  float *expectedIn;
  float expectedOut;
} IOSet;

typedef struct {
  int weightCount;
  float *weights;
  float threshold;
} Neuron;

float sigmoid(float x) { return 1.0f / (1.0f + exp(-x)); }
float relu(float x) { return x > 0.0f ? x : 0.0f; }
float step(float x) { return x > 0 ? 1 : 0; }

// Print neuron weights
void printNeuron(Neuron *neuron) {
  printf("Neuron:\n");
  for (int i = 0; i < neuron->weightCount; i++) {
    printf("\t%f\n", neuron->weights[i]);
  }
  printf("\tThreshold: %f\n", neuron->threshold);
}
void printIOSet(IOSet *set) {
  printf("IOSet:\n");
  for (int i = 0; i < set->inputCount; i++) {
    printf("\t%f\n", set->expectedIn[i]);
  }
  printf("\tOut: %f\n", set->expectedOut);
}
void printDataset(IOSet *sets, int size) {
  for (int i = 0; i < size; i++) {
    printIOSet(&sets[i]);
  }
}

// Randomly initialize neuron with x random weights
void createNeuron(int x, Neuron *out) {
  out->weights = malloc(x * sizeof(float));
  for (int i = 0; i < x; i++) {
    out->weights[i] = (float)rand() / (float)RAND_MAX;
  }
  out->weightCount = x;
  out->threshold = (float)rand() / (float)RAND_MAX;
}

float activate(Neuron *neuron, float *inputs, float (*activation)(float)) {
  float sum = 0;
  for (int i = 0; i < neuron->weightCount; i++) {
    sum += inputs[i] * neuron->weights[i];
    // printf("s: %f\n", sum);
  }
  sum -= neuron->threshold;
  // printf("s: %f\n", sum);
  return activation(sum);
}

// Returns error
float activateIO(Neuron *neuron, IOSet *io, float (*activation)(float)) {
  float r = activate(neuron, io->expectedIn, activation);
  float e = io->expectedOut - r;
  return e;
}
void activateDataset(Neuron *neuron, IOSet *dataset, int datasetSize,
                     float (*activation)(float)) {
  printf("Activating Neuron with dataset\n");
  for (int i = 0; i < datasetSize; i++) {
    activateIO(neuron, &(dataset[i]), activation);
  }
}
//Activate neuron and calculate deltaW
float activateLearn(Neuron *neuron, IOSet *ioset, float (*activation)(float), float** deltaW, float lr) {
  float error = activateIO(neuron, ioset, activation);
  *deltaW = malloc(ioset->inputCount * sizeof(float));
  for (int i = 0; i < ioset->inputCount; i++) {
    (*deltaW)[i] = ioset->expectedIn[i] * error * lr;
  }
  return error;
}
float learn(Neuron *neuron, IOSet *ioset, float (*activation)(float), float lr) {
  float *deltaW = NULL;
  float error = activateLearn(neuron, ioset, activation, &deltaW, lr);
  for (int i = 0; i < neuron->weightCount; i++) {
    neuron->weights[i] += deltaW[i];
  }
  return error;
}
float learnDataset(Neuron *neuron, IOSet *dataset, int datasetSize, float (*activation)(float), float lr) {
  printf("Learning Neuron on dataset\n");
  float error = 0.0f;
  for (int i = 0; i < datasetSize; i++) {
    error += learn(neuron, &(dataset[i]), activation, lr);
  }
  return error / datasetSize;
}
// Create the dataset where if there is more than 3 1s the output is supposed to
// be 1
int createDataset1(IOSet **out, int inputs) {
  int dataSize = pow(2, inputs);
  *out = malloc(dataSize * sizeof(IOSet));
  for (int i = 0; i < dataSize; i++) {
    (*out)[i].inputCount = inputs;
    (*out)[i].expectedIn = malloc(inputs * sizeof(float));
    float sum = 0;
    for (int j = inputs; j > 0; j--) {
      float v = (float)((i >> (j - 1)) & 1); // shift right and mask
      (*out)[i].expectedIn[inputs - j] = v;
      sum += v;
    }
    (*out)[i].expectedOut = sum > 3.5f ? 1.0f : 0.0f;
  }
  return pow(2, inputs);
}
// Create the dataset with 6 inputs, with 1 input being constant -1 for weight
// training
int createDataset2(IOSet **out) {
  int inputs = 5;
  int dataSize = pow(2, inputs);
  *out = malloc(dataSize * sizeof(IOSet));
  for (int i = 0; i < dataSize; i++) {
    (*out)[i].inputCount = inputs + 1;
    (*out)[i].expectedIn = malloc((inputs + 1) * sizeof(float));
    float sum = 0;
    for (int j = inputs; j > 0; j--) {
      float v = (float)((i >> (j - 1)) & 1); // shift right and mask
      (*out)[i].expectedIn[inputs - j] = v;
      sum += v;
    }
    (*out)[i].expectedIn[inputs] = -1; // constant activation
    (*out)[i].expectedOut = sum > 3.5f ? 1.0f : 0.0f;
  }
  return pow(2, inputs);
}

int main() {
  srand((unsigned int)time(NULL));

  IOSet *dataset = NULL;
  int dataSize = createDataset1(&dataset, 5);

  float (*activation)(float);
  activation = &step;
  Neuron exact;
  exact.weightCount = 5;
  exact.weights = malloc(5 * sizeof(float));
  static float exactWeights[5] = {2.0f, 2.0f, 2.0f, 2.0f, 2.0f};
  exact.weights = exactWeights;
  exact.threshold = 7.0f;
  printNeuron(&exact);
  // float error = activateIO(&exact, &(dataset[12]), activation);
  activateDataset(&exact, dataset, dataSize, activation);
  printf("////////////////////////////////\n");
  printf("////////////////////////////////\n");
  printf("Generating Neuron with 6 weights\n");
  Neuron neuron;
  createNeuron(6, &neuron);
  printNeuron(&neuron);
  // printf("%f\n", act);
  IOSet *dataset2 = NULL;
  int dataSize2 = createDataset2(&dataset2);
  // printDataset(dataset2, dataSize2);
  float learningRate = 0.01f;
  int epoch = 10;
  for (int i = 1; i <= epoch; i++) {
    float error = learnDataset(&neuron, dataset2, dataSize2, activation, learningRate);
    printNeuron(&neuron);
    // Dollarstore Adam optimizer
    //learningRate *= 0.999f; 
    printf("Epoch %d: Avg error: %f\n", i, error);
  }
  return 0;
}
