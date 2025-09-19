#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

typedef struct {
  int inputCount;
  float* expectedIn;
  float expectedOut;
} IOSet;

typedef struct {
  int weightCount;
  float* weights;
} Neuron;

float sigmoid(float x)
{
  return 1.0f / (1.0f + exp(-x));
}

float relu(float x)
{
  return x > 0.0f ? x : 0.0f;
}

//Print neuron weights
void printNeuron(Neuron* neuron)
{
  printf("Neuron:\n");
  for(int i=0;i<neuron->weightCount;i++)
  {
    printf("\t%f\n", neuron->weights[i]);
  }
}

void printIOSet(IOSet* set)
{
  printf("IOSet:\n");
  for(int i=0;i<set->inputCount;i++)
  {
    printf("\t%f\n", set->expectedIn[i]);
  }
  printf("\tOut: %f\n", set->expectedOut);
}

void printDataset(IOSet* sets, int size)
{
  for(int i=0;i<size;i++)
  {
    printIOSet(&sets[i]);
  }
}

//Randomly initialize neuron with x random weights 
void createNeuron(int x, Neuron* out)
{
  out->weights = malloc(x * sizeof(float));
  for(int i=0;i<x;i++)
  {
    out->weights[i] = (float)rand() / (float)RAND_MAX;
  }
  out->weightCount = x;
}

float activate(Neuron* neuron, float* inputs, float (*activation)(float))
{
  float sum = 0;
  for(int i=0;i<neuron->weightCount;i++)
  {
    sum += inputs[i] * neuron->weights[i];
  }
  return activation(sum);
}

//Returns error
float activateIO(Neuron* neuron, IOSet* io, float (*activation)(float))
{
  printf("Activating with following IOSet\n");
  printIOSet(io);
  float r = activate(neuron, io->expectedIn, activation);
  printf("Result %f\n", r);
  float e = io->expectedOut - r;
  printf("Error %f\n", e);
  return e;
}

float activateDataset(Neuron* neuron, IOSet* ioset, int datasetSize, float (*activation)(float))
{
  printf("Activating Neuron with dataset");
  for(int i=0;i<datasetSize;i++)
  {
    activateIO(neuron, &(ioset[i]), activation);
  }
}
//Create the dataset where if there is more than 3 1s the output is supposed to be 1
int createDataset1(IOSet** out, int inputs)
{
  int dataSize = pow(2,inputs);
  *out = malloc(dataSize * sizeof(IOSet));
  for(int i=0;i<dataSize;i++)
  {
    (*out)[i].inputCount = inputs;
    (*out)[i].expectedIn = malloc(inputs * sizeof(float));
    float sum = 0;
    for (int j = inputs; j > 0; j--) {
      float v = (float)((i >> j-1) & 1); // shift right and mask
      (*out)[i].expectedIn[inputs - j] = v;
      sum += v;
    }
    (*out)[i].expectedOut = sum > 3.5f ? 1.0f : 0.0f;
  }
  return pow(2,inputs);
}



int main() {
  srand((unsigned int)time(NULL));

  IOSet* dataset = NULL;
  int dataSize = createDataset1(&dataset, 5);

  float (*activation)(float);
  activation = &sigmoid;
  Neuron exact;
  exact.weightCount = 5;
  exact.weights = malloc(5 * sizeof(float));
  static float exactWeights[5] = {0.25f,0.25f,0.25f,0.25f,0.25f};
  exact.weights = exactWeights;
  printNeuron(&exact);
  activateDataset(&exact, dataset, dataSize, activation);
  printf("Generating 1 Neuron with 5 inputs\n");
  Neuron neuron;
  createNeuron(5, &neuron);
  printNeuron(&neuron);
  float inputs[5] = {1.0f,1.0f,1.0f,1.0f,0.0f};
  float act = activate(&neuron, inputs, activation);
  printf("%f\n", act);
  //printDataset(dataset, dataSize);
  float error = activateIO(&neuron, &(dataset[0]), activation);
  return 0;
}
