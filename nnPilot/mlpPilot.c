#include <time.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#if defined(PLAYER) || defined(RECORDER)
#include "cAI.h"
#endif
typedef struct Layer
{
  double *weights;
  int wW;         // weight Width
  int wH;         // weight Height
  double *biases; // size wH
  double *out;    // size wH
} Layer;

typedef struct MLP
{
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
double frand(double min, double max)
{
  return fmax(fmin((max - min) * (double)rand() / ((double)RAND_MAX) + min, max),
              min);
}
int frandArray(int size, double min, double max, double **out)
{
  *out = malloc(size * sizeof(double));
  if (*out == NULL)
  {
    return 1;
  }
  for (int i = 0; i < size; i++)
  {
    (*out)[i] = frand(min, max);
  }
  return 0;
}
// Matrix dot Vector
// Assume vW = 1, vH = mW
// Assume out memory allocated
// row major
void dot(const double *m, const double *v, int mW, int mH, double *out)
{
  for (int i = 0; i < mH; i++)
  {
    double acc = 0.0f;
    for (int j = 0; j < mW; j++)
    {
      acc += m[i * mW + j] * v[j];
    }
    out[i] = acc;
  }
}
// Vector add Vector
// Assume out memory allocated
void addV(double *v1, double *v2, int vSize, double *out)
{
  for (int i = 0; i < vSize; i++)
  {
    out[i] = v1[i] + v2[i];
  }
}
// Vector sub Vector
// Assume out memory allocated
void subV(double *v1, double *v2, int vSize, double *out)
{
  for (int i = 0; i < vSize; i++)
  {
    out[i] = v1[i] - v2[i];
  }
}

int getOutputCount(MLP *network) { return network->layers[network->layerCount - 1].wH; }
double *getOutput(MLP *network) { return network->layers[network->layerCount - 1].out; }

void forwardSingle(Layer *layer, const double *in)
{
  double *temp = malloc(layer->wH * sizeof(double));
  dot(layer->weights, in, layer->wW, layer->wH, temp);
  subV(temp, layer->biases, layer->wH, layer->out);
  for (int i = 0; i < layer->wH; i++)
  {
    layer->out[i] = sigmoid(layer->out[i]);
  }
  free(temp);
}

void forward(MLP *network, const double *in)
{
  const double *current = in;
  for (int i = 0; i < network->layerCount; i++)
  {
    forwardSingle(&network->layers[i], current);
    current = network->layers[i].out;
  }
}

// partial differentiation of sigmoid
double partial(double x) { return x * (1 - x); }

// pain
double backward(MLP *network, double *networkInput, double *targetOutput, double lr)
{
  const int lc = network->layerCount;
  Layer *layer = &network->layers[lc - 1]; // output layer
  Layer *nextLayer = (lc > 1) ? &network->layers[lc - 2] : NULL;
  const double *in = (lc > 1) ? nextLayer->out : networkInput; // check if the network is just a perceptron

  // output layer error gradient
  double *egLast = malloc(layer->wH * sizeof(double));
  double error = 0.0f;
  for (int i = 0; i < layer->wH; ++i)
  {
    egLast[i] = partial(layer->out[i]) * (targetOutput[i] - layer->out[i]);
    error += (targetOutput[i] - layer->out[i]);
  }
  // output layer delta weight
  double *dw = malloc(layer->wH * layer->wW * sizeof(double));
  double *db = malloc(layer->wH * sizeof(double));
  for (int j = 0; j < layer->wH; ++j)
  {
    for (int k = 0; k < layer->wW; ++k)
    {
      dw[j * layer->wW + k] = lr * in[k] * egLast[j];
    }
    db[j] = lr * egLast[j];
  }
  // start looping
  // start from output layer
  for (int l = lc - 1; l >= 0; --l)
  {
    layer = &network->layers[l];
    nextLayer = (l > 0) ? &network->layers[l - 1] : NULL;

    // update weight of the last layer
    // on first iter, this updates output layer weights and biases
    for (int j = 0; j < layer->wH; ++j)
    {
      for (int k = 0; k < layer->wW; ++k)
      {
        layer->weights[j * layer->wW + k] += dw[j * layer->wW + k];
      }
      layer->biases[j] += db[j];
    }
    // reached input layer
    if (l == 0)
      break;

    // error gradient for next layer
    double *egNew = malloc(nextLayer->wH * sizeof(double));
    for (int i = 0; i < nextLayer->wH; ++i)
    {
      double acc = 0.0f;
      for (int j = 0; j < layer->wH; ++j)
      {
        acc += egLast[j] * layer->weights[j * layer->wW + i]; // sum error
      }
      egNew[i] = acc * partial(nextLayer->out[i]);
    }

    // delta weight biases of next layer
    const double *inNext = (l > 1) ? network->layers[l - 2].out : networkInput;
    double *dwNext = malloc(nextLayer->wH * nextLayer->wW * sizeof(double));
    double *dbNext = malloc(nextLayer->wH * sizeof(double));

    for (int j = 0; j < nextLayer->wH; ++j)
    {
      for (int k = 0; k < nextLayer->wW; ++k)
      {
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
  return error / network->outputCount;
}

// for example:
// 2 inputs, 1 hidden layer of size 2, 1 output -> [2,2,1]
int createMLP(int *nodes, int nodesSize, MLP **out)
{
  if (!out || !nodes || nodesSize < 2)
  {
    printf("Failed to create network\n");
    return 1;
  }

  *out = malloc(sizeof(MLP));
  (*out)->layerCount = nodesSize - 1;
  (*out)->layers = calloc((nodesSize - 1), sizeof(Layer));
  int error = 0, pos = 0;
  for (int i = 0; i < nodesSize - 1; i++)
  {
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
  if (error)
  {
    printf("Error while initializing network\n");
    // cleanup
    for (int i = 0; i <= pos; i++)
    {
      free((*out)->layers[i].weights);
      free((*out)->layers[i].biases);
    }
    free((*out)->layers);
    free(*out);
    *out = NULL;
  }
  printf("Successfully created network\n");
  return 0;
}

#define MLP_MAGIC "MLP1"
#define MLP_VERSION 1
static void *xmalloc(size_t n)
{
  void *p = malloc(n);
  if (!p)
  {
    fprintf(stderr, "OOM allocating %zu bytes\n", n);
    exit(1);
  } // LOW CONFIDENCE: choose your own OOM policy
  return p;
}

static int write_exact(const void *ptr, size_t sz, size_t n, FILE *f)
{
  return fwrite(ptr, sz, n, f) == n ? 0 : -1;
}

static int read_exact(void *ptr, size_t sz, size_t n, FILE *f)
{
  return fread(ptr, sz, n, f) == n ? 0 : -1;
}

void mlp_free(MLP *net)
{
  if (!net || !net->layers)
    return;
  for (int i = 0; i < net->layerCount; ++i)
  {
    free(net->layers[i].weights);
    free(net->layers[i].biases);
    free(net->layers[i].out);
    net->layers[i].weights = NULL;
    net->layers[i].biases = NULL;
    net->layers[i].out = NULL;
  }
  free(net->layers);
  net->layers = NULL;
  net->layerCount = 0;
  net->inputCount = 0;
  net->outputCount = 0;
}
/* Save only structure + weights + biases (not 'out'). */
int mlpSave(const MLP *net, const char *path)
{
  if (!net || !path || !net->layers)
    return -1;
  FILE *f = fopen(path, "wb");
  if (!f)
    return -1;

  int err = 0;
  // Header
  char magic[4] = MLP_MAGIC;
  int32_t version = (int32_t)MLP_VERSION;
  int32_t layerCount = (int32_t)net->layerCount;
  int32_t inputCount = (int32_t)net->inputCount;
  int32_t outputCount = (int32_t)net->outputCount;

  err |= write_exact(magic, 1, 4, f);
  err |= write_exact(&version, sizeof(int32_t), 1, f);
  err |= write_exact(&layerCount, sizeof(int32_t), 1, f);
  err |= write_exact(&inputCount, sizeof(int32_t), 1, f);
  err |= write_exact(&outputCount, sizeof(int32_t), 1, f);

  // Layers
  for (int i = 0; i < net->layerCount && !err; ++i)
  {
    const Layer *L = &net->layers[i];
    int32_t wW = (int32_t)L->wW;
    int32_t wH = (int32_t)L->wH;
    size_t wcount = (size_t)L->wW * (size_t)L->wH;

    err |= write_exact(&wW, sizeof(int32_t), 1, f);
    err |= write_exact(&wH, sizeof(int32_t), 1, f);
    if (!L->weights || !L->biases)
    {
      err = -1;
      break;
    }

    err |= write_exact(L->weights, sizeof(double), wcount, f);
    err |= write_exact(L->biases, sizeof(double), L->wH, f);
  }

  int cerr = fclose(f);
  if (cerr != 0)
    err = -1;
  return err ? -1 : 0;
}

/* Loads into *out, allocating memory. On error, *out is cleared. */
int mlpLoad(MLP *out, const char *path)
{
  if (!out || !path)
    return -1;

  FILE *f = fopen(path, "rb");
  if (!f)
    return -1;

  int err = 0;
  char magic[4];
  int32_t version = 0, layerCount = 0, inputCount = 0, outputCount = 0;

  err |= read_exact(magic, 1, 4, f);
  err |= read_exact(&version, sizeof(int32_t), 1, f);
  err |= read_exact(&layerCount, sizeof(int32_t), 1, f);
  err |= read_exact(&inputCount, sizeof(int32_t), 1, f);
  err |= read_exact(&outputCount, sizeof(int32_t), 1, f);

  if (err || memcmp(magic, MLP_MAGIC, 4) != 0 || version != MLP_VERSION || layerCount <= 0)
  {
    fclose(f);
    return -1;
  }

  out->layerCount = layerCount;
  out->inputCount = inputCount;
  out->outputCount = outputCount;
  out->layers = (Layer *)calloc((size_t)layerCount, sizeof(Layer));
  if (!out->layers)
  {
    fclose(f);
    return -1;
  }

  for (int i = 0; i < layerCount && !err; ++i)
  {
    Layer *L = &out->layers[i];
    int32_t wW = 0, wH = 0;
    err |= read_exact(&wW, sizeof(int32_t), 1, f);
    err |= read_exact(&wH, sizeof(int32_t), 1, f);
    if (err || wW <= 0 || wH <= 0)
    {
      err = -1;
      break;
    }

    L->wW = wW;
    L->wH = wH;
    size_t wcount = (size_t)wW * (size_t)wH;

    L->weights = (double *)xmalloc(sizeof(double) * wcount);
    L->biases = (double *)xmalloc(sizeof(double) * (size_t)wH);
    L->out = (double *)calloc((size_t)wH, sizeof(double)); // optional, zeroed

    err |= read_exact(L->weights, sizeof(double), wcount, f);
    err |= read_exact(L->biases, sizeof(double), (size_t)wH, f);
  }

  int cerr = fclose(f);
  if (cerr != 0)
    err = -1;

  if (err)
  {
    mlp_free(out);
    return -1;
  }
  return 0;
}

#ifdef DEBUGTHRUST
#define THRUSTDEBUG(x) x;
#else
#define THRUSTDEBUG(x)
#endif
#ifdef DEBUGTURN
#define TURNDEBUG(x) x;
#else
#define TURNDEBUG(x)
#endif
#if defined(PLAYER) || defined(RECORDER)
float ruleBasedBotThrust(int shotDanger, int headingTrackingDiff, int furthest_angle, int trackWall, int heading)
{
  double frontWall = wallFeeler(500, heading);
  double wall5 = wallFeeler(500, heading + 150);
  double backWall = wallFeeler(500, heading + 180);
  double wall7 = wallFeeler(500, heading + 210);
  if (shotDanger > 0 && shotDanger < 200 && frontWall > 200 && trackWall > 80)
  {
    THRUSTDEBUG(AR(printf("avoiding shot: %d\n", shotDanger)));
    return 1;
  }
  else if ((furthest_angle == 0) && selfSpeed() < 6 && frontWall > 200)
  {
    THRUSTDEBUG(AR(printf("propulsion\n")));
    return 1;
  }
  else if (headingTrackingDiff > 110 && headingTrackingDiff < 250 && trackWall < 300 && frontWall > 250)
  {
    THRUSTDEBUG(AR(printf("thrusters type 1\n")));
    return 1;
  }
  else if (backWall < 70 && frontWall > 250)
  {
    THRUSTDEBUG(AR(printf("thrusters type 2\n")));
    return 1;
  }
  else if (headingTrackingDiff > 85 && headingTrackingDiff < 275 && trackWall < 100 && frontWall > 200)
  {
    THRUSTDEBUG(AR(printf("thrusters type 3\n")));
    return 1;
  }
  else if (wall5 < 70 && frontWall > 250)
  {
    THRUSTDEBUG(AR(printf("thrusters type 4\n")));
    return 1;
  }
  else if (wall7 < 70 && frontWall > 250)
  {
    THRUSTDEBUG(AR(printf("thrusters type 5\n")));
    return 1;
  } else if (selfSpeed() < 1)
  {
    THRUSTDEBUG(AR(printf("thrusters type 6\n")));
    return 1;
  }
  return 0;
}
float ruleBasedBotTurnAim(int aimDir, int headingAimingDiff, float turn)
{
  if (aimDir < 0)
  {
    return turn;
  } // no error
  else if (aimDir > 0 && headingAimingDiff < 180 && headingAimingDiff > 0)
  {
    return 1;
  }
  else if (aimDir > 0 && headingAimingDiff > 180 && headingAimingDiff < 360)
  {
    return 0;
  }
  else
    return 0.5f;
}
float ruleBasedBotTurnNav(int headingTrackingDiff, int headingAimingDiff, int aimDir, int heading, int closest, int closest_angle, int furthest_angle)
{
  if (selfSpeed() > 9 && headingTrackingDiff < 175)
  {
    return 0;
  }
  // Speed too fast, turn to face against tracking direction
  else if (selfSpeed() > 9 && headingTrackingDiff > 185)
  {
    return 1;
  }
  else if (selfSpeed() > 1 && closest < 100 && closest_angle > 2 && closest_angle <= 180)
  {
    return 1;
  }
  // if the ship is already facing the furthest area, then turn away from nearby geometries
  else if (selfSpeed() > 1 && closest < 100 && closest_angle < 358 && closest_angle > 180)
  {
    return 0;
  }
  // Always try to face the furthest area
  else if (furthest_angle > 3 && furthest_angle <= 180)
  {
    return 0;
  }
  // Always try to face the furthest area
  else if (furthest_angle < 357 && furthest_angle > 180)
  {
    return 1;
  }
  else
    return 0.5;
}
#endif
MLP *network = NULL;

#define INPUTSIZE 21 //
#define NODESSIZE 3  // 2 + hidden layers
#define OUTPUTSIZE 1
#define AR(x)      \
  if (selfAlive()) \
  {                \
    x;             \
  }

FILE *replay;
#if defined(PLAYER) || defined(RECORDER)
int AI_loop()
{
  srand((unsigned int)time(NULL));
  static int life = 0;
  static int update = 0;
  if (!selfAlive())
  { // if dead
    if (life > 0)
    {           // and we dont know yet,
      life = 0; // set to dead
      update = 0;
    }
    return 0; // dont do anything
  }
  else if (life == 0)
  {           // if alive and we dont know yet
    life = 1; // reset life counter just to make sure
  }
  else
  { // if alive and we know
    life++;
    update++;
    update = update > 100 ? 100 : update;
    life = life > 960 ? 960 : life; // cap life to 960 (40 seconds at 24fps)
  }

  setTurnSpeedDeg(20);

  int aimDir = aimdir(0);
  int shotDanger = shotAlert(0);
  double heading = selfHeadingDeg();
  double tracking = selfTrackingDeg();
  double trackWall = wallFeeler(500, tracking);

  double furthest = 0.0;
  int furthest_angle = 0;
  double closest = 600.0;
  int closest_angle = 0;
  for (int i = 0; i < 360; i++)
  {
    double dist = wallFeeler(500, heading + i);
    if (dist > furthest)
    {
      furthest = dist;
      furthest_angle = i;
    }
    if (dist < closest)
    {
      closest = dist;
      closest_angle = i;
    }
  }
  // NN inputs
  // clockwise rotation around the ship
  double *inputs = calloc(INPUTSIZE, sizeof(double));

  int headingAimingDiff = ((int)(heading + 360 - aimDir) % 360);
  int headingTrackingDiff = ((int)(heading + 360 - tracking) % 360);
  inputs[0] = fmin(selfSpeed() / 10.0f, 1.0f);
  inputs[1] = headingTrackingDiff / 360.0f;
  inputs[2] = headingAimingDiff / 360.0f;
  inputs[3] = fmin((double)shotAlert(0) / 100.0f, 1.0f);
  inputs[4] = trackWall / 500.0f;
  inputs[5] = closest / 500.0f;
  inputs[6] = closest_angle / 360.0f;
  inputs[7] = furthest / 500.0f;
  inputs[8] = furthest_angle / 360.0f;
  for (int i = 9; i < 21; i++)
  {
    inputs[i] = wallFeeler(500, heading + (i-9) * 30) / 500.0f; // normalize
  }

  double navigationGoal = ruleBasedBotTurnNav(headingTrackingDiff, headingAimingDiff, aimDir, heading, closest, closest_angle, furthest_angle);
  double aimGoal = ruleBasedBotTurnAim(aimDir, headingAimingDiff, navigationGoal);
  //Is this better or no?
  double finalTurnGoal = (aimGoal + navigationGoal) / 2; // average the aim goal with navigation goal 
  int thrustGoal = ruleBasedBotThrust(shotDanger, headingTrackingDiff, furthest_angle, trackWall, heading);

  float turnDir = 0;
  double turnRB = 0.0f;
  if (closest > 70 && aimDir > 0)
  {
    turnRB = aimGoal;
  }
  else
    turnRB = navigationGoal;

  double backWall = wallFeeler(30, heading + 180);
  if ((headingAimingDiff < 20 || headingAimingDiff > 340) && backWall > 20 && aimDir > 0)
  {
    fireShot(1);
  }

#ifdef RECORDER
  turnDir = turnRB; // use output from the rulebased
  for (int i = 0; i < INPUTSIZE; i++)
  {
    fprintf(replay, "%f", inputs[i]);
    fprintf(replay, " "); // space between numbers
  }
  fprintf(replay, "%f", turnRB); // when creating replay, only encode 1 value. sanitize.py will split this direction value into turnRight and turnLeft
  fprintf(replay, "\n"); // end with newline
  fflush(replay);
#endif
#ifdef PLAYER
  forward(network, inputs);
  //NN output (sigmoid activation)
  turnDir = getOutput(network)[0];
#endif
free(inputs);
  if (turnDir > 0.6f)
  {
    turnRight(1);
    turnLeft(0);
  }
  else if (turnDir < 0.4f)
  {
    turnRight(0);
    turnLeft(1);
  }
  else
  {
    turnRight(0);
    turnLeft(0);
  }

  if (thrustGoal > 0.5f)
    thrust(1);
  else
    thrust(0);
}
#endif
int main(int argc, char *argv[])
{
  // best architecture so far was [21, 21, 1] at 71% accuracy for epoch 500, lr 0.05
  // [21, 21, 1] lr 0.04 epoch 700 acc 0.823
  // [21, 21, 1] lr 0.055 epoch 1000 acc 0.835
  // [21, 21, 1] lr 0.7 epoch 300 acc 0.825
  // [21, 21, 1] lr 0.03 epoch 1000 acc 0.83
  // [21, 21, 1] lr 0.045 epoch 1000 acc 0.84

  // [21, 21, 1] lr 0.7 epoch 200 decay 0.9995 acc 0.808
  // [21, 21, 1] lr 0.7 epoch 300 decay 0.9995 acc 0.805
  // [21, 21, 1] lr 0.7 epoch 500 decay 0.9995 acc 0.833
  // [21, 21, 1] lr 0.7 epoch 1000 decay 0.9995 acc 0.844

  // [21, 21, 1] lr 0.7 epoch 200 decay 0.9661 acc 0.782
  // [21, 21, 1] lr 0.7 epoch 150 decay 0.955 acc 0.799
  // [21, 21, 1] lr 0.8 epoch 500 decay 0.9995 acc 0.82
  // [21, 21, 1] lr 0.7 epoch 300 decay 0.99 acc 0.74
  // [21, 21, 1] lr 0.7 epoch 200 decay 0.99 acc 0.82

  int nodes[NODESSIZE] = {INPUTSIZE, 21, OUTPUTSIZE};

  createMLP(nodes, NODESSIZE, &network);
  char modelNameBuffer[200];
  const char* modelPath = "model-lr%f-decay%f-epoch%d.save";
  const char *replayPath = "replay.txt";
#ifdef PLAYER
  mlpLoad(network, "model.save");
  return start(argc, argv);
#endif
#ifdef RECORDER
  replay = fopen(replayPath, "a");
  start(argc, argv);
  fclose(replay);
  return 0;
#endif
#ifdef TRAINER
  if (argc < 2) {
        printf("Usage: %s lr epoch decay\n", argv[0]);
        return 1;
    }
  srand(0);
  const char *replayPaths[] = {
      "replay_clean.txt",
      "replay2_clean.txt",
  };
  const int replayCount = 2;
  double learningRate = atof(argv[1]);
  const int epoch = atoi(argv[2]);
  double decay = atof(argv[3]);
  printf("Training NN\n Epoch: %d Lr: %f\n", epoch, learningRate);
  double values[INPUTSIZE + OUTPUTSIZE]; // oneline
  for(int r=0;r<replayCount;r++)
  {
    replay = fopen(replayPaths[r], "r");
    if (!replay)
    {
      fprintf(stderr, "Error opening file %s: ", replayPath);
      return 1;
    }
    rewind(replay);
    int dataCount = 0;
    for(int e = 0;e < epoch;e++)
    {
      dataCount = 0;
      int count = 0;
      while (fscanf(replay, "%lf", &values[count]) == 1)
      {
        count++;
        if (count == INPUTSIZE + OUTPUTSIZE)
        {
          forward(network, values);
          backward(network, values, &(values[INPUTSIZE]), learningRate);
          count = 0;
          dataCount++;
        }
      }
      rewind(replay);
      learningRate *= decay;
    }
    fclose(replay);
    printf("Trained with %d lines from %s for %d epochs\n", dataCount, replayPaths[r], epoch);
  }

  double error = 0.0f;
  float accuracy = 0.0f;
  int dataCount = 0;
  for(int r=0;r<replayCount;r++)
  {
    replay = fopen(replayPaths[r], "r");
    rewind(replay);
    // get accuracy
    int count = 0;
    while (fscanf(replay, "%lf", &values[count]) == 1)
    {
      count++;
      if (count == INPUTSIZE + OUTPUTSIZE)
      {
        forward(network, values);
        double *out = getOutput(network);
        for (int i = 0; i < OUTPUTSIZE; i++)
        {
          float diff = values[INPUTSIZE + i] - out[i];
          error += diff * diff;
        }
        //Accuracy calculation for 1 output
        accuracy += out[0] > 0.6f && values[INPUTSIZE] > 0.6f ? 1:0;
        accuracy += out[0] < 0.4f && values[INPUTSIZE] < 0.4f ? 1:0;
        accuracy += out[0] > 0.4f && out[0] < 0.6f && values[INPUTSIZE] > 0.4f && values[INPUTSIZE] < 0.6f? 1:0;
        //Accuracy calculation for 2 outputs
        //accuracy += out[0] > out[1] && values[INPUTSIZE] > values[INPUTSIZE + 1] ? 1:0;
        //accuracy += out[0] < out[1] && values[INPUTSIZE] < values[INPUTSIZE + 1] ? 1:0;
        count = 0;
        dataCount++;
      }
    }
    fclose(replay);
  }
  sprintf(modelNameBuffer, modelPath, learningRate, decay, epoch);
  mlpSave(network, modelNameBuffer);
  printf(" Avg Error %f\n Avg Accuracy %f\n Model saved\n", sqrt(error / dataCount), accuracy / dataCount);
#endif
}