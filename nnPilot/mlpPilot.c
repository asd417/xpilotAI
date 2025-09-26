#include <time.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "cAI.h"

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
// Vector sub Vector
// Assume out memory allocated
void subV(double *v1, double *v2, int vSize, double *out) {
  for (int i = 0; i < vSize; i++) {
    out[i] = v1[i] - v2[i];
  }
}

int getOutputCount(MLP *network) {return network->layers[network->layerCount - 1].wH;}
double *getOutput(MLP *network) {return network->layers[network->layerCount - 1].out;}

void forwardSingle(Layer *layer, const double *in) {
  double *temp = malloc(layer->wH * sizeof(double));
  dot(layer->weights, in, layer->wW, layer->wH, temp);
  subV(temp, layer->biases, layer->wH, layer->out);
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
      dbNext[j] = lr * -1.0f * egNew[j];
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

float ruleBasedBotThrust(int shotDanger, int headingTrackingDiff, int furthest_angle, int trackWall, int frontWall, int backWall, int wall5, int wall7)
{
  if(shotDanger > 0 && shotDanger < 200 && frontWall > 200 && trackWall > 80)
  {
    THRUSTDEBUG(AR(printf("avoiding shot: %d\n", shotDanger)));
    return 1;
  }
  else if((furthest_angle == 0) && selfSpeed() < 6 && frontWall > 200)
  {
    THRUSTDEBUG(AR(printf("propulsion\n")));
    return 1;
  }
  else if(headingTrackingDiff > 110 && headingTrackingDiff < 250 && trackWall < 300 && frontWall > 250)
  {
    THRUSTDEBUG(AR(printf("thrusters type 1\n")));
    return 1;
  }
  else if(backWall < 70 && frontWall > 250)
  {
    THRUSTDEBUG(AR(printf("thrusters type 2\n")));
    return 1;
  }
  else if(headingTrackingDiff > 85 && headingTrackingDiff < 275 && trackWall < 100 && frontWall > 200)
  {
    THRUSTDEBUG(AR(printf("thrusters type 3\n")));
    return 1;
  }
  else if(wall5 < 70 && frontWall > 250)
  {
    THRUSTDEBUG(AR(printf("thrusters type 4\n")));
    return 1;
  }
  else if(wall7 < 70 && frontWall > 250)
  {
    THRUSTDEBUG(AR(printf("thrusters type 5\n")));
    return 1;
  }
  return 0;
}
float ruleBasedBotTurnAim(int aimDir, int headingAimingDiff, float turn)
{
  if(aimDir < 0) { return turn; } // no error
  else if(aimDir > 0 && headingAimingDiff < 180 && headingAimingDiff > 0)
  {
    return 1;
  }
  else if(aimDir > 0 && headingAimingDiff > 180 && headingAimingDiff < 360)
  {
    return 0;
  }
}
float ruleBasedBotTurnNav(int headingTrackingDiff, int headingAimingDiff, int aimDir, int heading)
{
  double furthest = 0.0;
  int furthest_angle = 0;
  double closest = 600.0;
  int closest_angle = 0;
  for(int i=0;i<180;i++)
  {
    double dist = wallFeeler(500,heading+i*2);
    if(dist > furthest)
    {
      furthest = dist;
      furthest_angle = i;
    }
    if(dist < closest)
    {
      closest = dist;
      closest_angle = i;
    }
  }
  if(selfSpeed() > 9 && headingTrackingDiff < 175)
  {
    return 0;
  }
  //Speed too fast, turn to face against tracking direction
  else if(selfSpeed() > 9 && headingTrackingDiff > 185)
  {
    return 1;
  }
  else if(selfSpeed() > 1 && closest < 100 && closest_angle > 2 && closest_angle <= 180)
  {
    return 1;
  }
  //if the ship is already facing the furthest area, then turn away from nearby geometries
  else if(selfSpeed() > 1 && closest < 100 && closest_angle < 358 && closest_angle > 180)
  {
    return 0;
  }
  //Always try to face the furthest area
  else if(furthest_angle > 3 && furthest_angle <= 180)
  {
    return 0;
  }
  //Always try to face the furthest area
  else if(furthest_angle < 357 && furthest_angle > 180)
  {
    return 1;
  }
}

MLP* network = NULL;

#define INPUTSIZE 17 // 12 wall feelers + speed + tracking diff + aiming diff + shot alert + tracking wall
#define NODESSIZE 5 // 3 hidden layers
#define OUTPUTSIZE 1
#define AR(x) if(selfAlive()){x;}
#ifdef DEBUGTHRUST
  #define THRUSTDEBUG(x) x;
#else
  #define THRUSTDEBUG(x)
#endif

int AI_loop() {
  srand((unsigned int)time(NULL));
  static int life=0;
  if(!selfAlive()) { //if dead
    if(life > 0) { // and we dont know yet,
      life = 0; // set to dead
    }
    return 0; // dont do anything
  } else if(life==0) { // if alive and we dont know yet
    life = 1; //reset life counter just to make sure
  } else { // if alive and we know
    life++;
    life = life > 960 ? 960 : life; // cap life to 960 (40 seconds at 24fps)
  }
  setTurnSpeedDeg(20);
  
  int shouldThrust = 0;
  int aimDir = aimdir(0);
  int shotDanger = shotAlert(0);
  double heading = selfHeadingDeg();
  double tracking = selfTrackingDeg();
  double trackWall = wallFeeler(1000, tracking);

  // NN inputs
  // clockwise rotation around the ship
  double* inputs = calloc(INPUTSIZE, sizeof(double));
  for(int i=0;i<12;i++)
  {
    inputs[i] = wallFeeler(1000, heading + i*30) / 1000.0f; // normalize
  }
  int headingAimingDiff = ((int)(heading + 360 - aimDir) % 360);
  int headingTrackingDiff = ((int)(heading + 360 - tracking) % 360);
  inputs[12] = selfSpeed() / 10.0f;
  inputs[13] = headingTrackingDiff / 360.0f;
  inputs[14] = headingAimingDiff / 360.0f;
  inputs[15] = (double)shotAlert(0) / 1000.0f;
  inputs[16] = trackWall / 1000.0f;

  //printf("heading %f, tracking %f, diff: %d, trackWall: %f\n", heading, tracking, headingTrackingDiff, trackWall);
  //Thrust rules
  


  forward(network, inputs);
  double* out = network->layers[network->layerCount - 1].out;
  //NN output (sigmoid activation)
  double turn = out[0];     // 0 to 1

  

  //NN goal actions
  float learningRate = 0.001f;

  double aimGoal = ruleBasedBotTurnAim(aimDir, headingAimingDiff, turn);
  double navigationGoal = ruleBasedBotTurnNav(headingTrackingDiff, headingAimingDiff, aimDir, heading);
  navigationGoal = learningRate*(navigationGoal-turn)+turn;
  // scale the error based on how long the bot has been alive
  // longer the bot survives, lower the turn navigation error
  // xpilot is 24fps
  // if the bot survives longer than 40 seconds, the learning rate is minimal
  // if the life >= 960, error = 0.1

  double finalTurnGoal = (aimGoal + navigationGoal) / 2;
  
  if (turn > 0.5f)
    setTurnDir(1);
  else
    setTurnDir(-1);
  if(shouldThrust) thrust(1);
  else thrust(0);
  int turnError;

}

int main(int argc, char *argv[]) {
  int nodes[NODESSIZE] = {INPUTSIZE, INPUTSIZE/4 * 3,8,6, OUTPUTSIZE};
  createMLP(nodes, NODESSIZE, &network);
  return start(argc, argv); }

// int main() {
//   int nodes[NODESSIZE] = {INPUTSIZE, 3, OUTPUTSIZE};
//   TrainingData *dataset;
//   int datasetSize = createDataset(&dataset);
//   MLP *network;
//   printf("MLP\n  Structure: ");
//   for (int i = 0; i < NODESSIZE; i++) {
//     printf("%d ", nodes[i]);
//   }
//   printf("\n");
//   double lr = 0.1f;
//   int maxEpoch = 10000;
//   printf("  Learning Rate: %f\n", lr);
//   printf("  Epoch: %d\n", maxEpoch);
//   createMLP(nodes, NODESSIZE, &network);
//   train(network, maxEpoch, lr, dataset, datasetSize, 1000);
//   return 0;
// }