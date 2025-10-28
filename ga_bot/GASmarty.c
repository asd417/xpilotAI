// Evan Gray - May 2012
// Compile: gcc Spinner.c libcAI.so -o Spinner
// Run: ./Spinner
#include "cAI.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "sqlite3.h"

// #define DEBUGTURN
#define DEBUGTHRUST
// Alive run, only run when alive
#define AR(x)      \
  if (selfAlive()) \
  {                \
    x;             \
  }
#ifdef DEBUGTURN
#define TURNDEBUG(x) x;
#else
#define TURNDEBUG(x)
#endif

#ifdef DEBUGTHRUST
#define THRUSTDEBUG(x) x;
#else
#define THRUSTDEBUG(x)
#endif

typedef struct
{
  int aimDir;
  double frontWall;
  double wall5;
  double backWall;
  double wall7;
  double heading;
  double tracking;
  double trackWall;
  double closest;
  int closestAngle;
  double furthest;
  int furthestAngle;
  int shotDanger;
  int speed;
  int headingTrackingDiff;
  int headingAimingDiff;
} State;

typedef struct
{
  int priority;
  int result;
} Inference;

uint8_t paramFromChromosome(const Chromosome *chrom, int offset, int length)
{
  if (!chrom || !chrom->genes || length <= 0 || length > 8)
    return 0;
  uint8_t value = 0;
  for (int i = 0; i < length; i++)
  {
    bit_t bit = chrom->genes[offset + i] & 1;
    value = (value << 1) | bit;
  }
  return value;
}
// General helper to read N consecutive parameters
void readNParams(const Chromosome *chrom, int start, int N, uint8_t *out)
{
  for (int i = 0; i < N; ++i)
    out[i] = paramFromChromosome(chrom, start + i * 8, chrom->length);
}

// chromosome structure:
//  all parameters are between 0 and 511, so we use 8 bits per number

// for rp: output 0 for nothing, 1 for thruster,
// for tr: output 0 for nothing, 1 for turnRight, -1 for turnLeft, 2 for turn to aimdir
Inference ruleExample(State state, Chromosome chromosome)
{
  return (Inference){0, 0};
}

typedef unsigned char bit_t; /* 0 or 1 */

typedef struct
{
  bit_t *genes;
  int fitness;
  int length; // number of bits in the chromosome
} Chromosome;

// rp are propulsion rules
// rt are turning rules

#define R1 5 // number of parameters in this rule
Inference rp1(State state, const Chromosome *chromosome)
{
  uint8_t p[R1];
  readNParams(chromosome, 0, R1, p);
  int priority = p[0];
  int value = state.shotDanger > p[1] && state.shotDanger < p[2] && state.frontWall > p[3] && state.trackWall > p[4] ? 1 : 0;
  return (Inference){priority, value};
}
#define R2 4
Inference rp2(State state, Chromosome chromosome)
{
  uint8_t p[R2];
  readNParams(chromosome, R1, R2, p);
  int priority = p[0];
  int value = (state.furthestAngle == p[1]) && state.speed < p[2] && state.frontWall > p[3] ? 1 : 0;
  return (Inference){priority, value};
}
#define R3 5
Inference rp3(State state, Chromosome chromosome)
{
  uint8_t p[R3];
  readNParams(chromosome, R1 + R2, R3, p);
  int priority = p[0];
  int value = (state.headingTrackingDiff > p[1] && state.headingTrackingDiff < p[2] && state.trackWall < p[3] && state.frontWall > p[4]) ? 1 : 0;
  return (Inference){priority, value};
}
#define R4 3
Inference rp4(State state, Chromosome chromosome)
{
  uint8_t p[R4];
  readNParams(chromosome, R1 + R2 + R3, R4, p);
  int priority = p[0];
  int value = (state.backWall < p[1] && state.frontWall > p[2]) ? 1 : 0;
  return (Inference){priority, value};
}
#define R5 5
Inference rp5(State state, Chromosome chromosome)
{
  uint8_t p[R5];
  readNParams(chromosome, R1 + R2 + R3 + R4, R5, p);
  int priority = p[0];
  int value = (state.headingTrackingDiff > p[1] && state.headingTrackingDiff < p[2] && state.trackWall < p[3] && state.frontWall > p[4]) ? 1 : 0;
  return (Inference){priority, value};
}
#define R6 4
Inference rp6(State state, Chromosome chromosome)
{
  uint8_t p[R6];
  readNParams(chromosome, R1 + R2 + R3 + R4 + R5, R6, p);
  int priority = p[0];
  int value = ((state.wall5 < p[1] || state.wall7 < p[2]) && state.frontWall > p[3]) ? 1 : 0;
  return (Inference){priority, value};
}
#define R7 3
Inference tr1(State state, Chromosome chromosome)
{
  uint8_t p[R7];
  readNParams(chromosome, R1 + R2 + R3 + R4 + R5 + R6, R7, p);
  int priority = p[0];
  int value = 0;
  if (state.speed > p[1] && state.headingTrackingDiff < 180 - p[2])
  {
    value = -1;
  }
  else if (state.speed > p[1] && state.headingTrackingDiff > 180 + p[2])
  {
    value = 1;
  }
  return (Inference){priority, value};
}
#define R8 4
Inference tr2(State state, Chromosome chromosome)
{
  uint8_t p[R8];
  readNParams(chromosome, R1 + R2 + R3 + R4 + R5 + R6 + R7, R8, p);
  int priority = p[0];
  int value = 0;
  if (state.closest > p[1] && state.aimDir > p[2] && state.headingAimingDiff < 180 && state.headingAimingDiff > p[3])
  {
    TURNDEBUG(AR(printf("Aiming Roughly\n")));
    value = 1;
  }
  else if (state.closest > p[1] && state.aimDir > p[2] && state.headingAimingDiff > 180 && state.headingAimingDiff < 360 - p[3])
  {
    TURNDEBUG(AR(printf("Aiming Roughly\n")));
    value = -1;
  }
  return (Inference){priority, value};
}
#define R9 4
Inference tr3(State state, Chromosome chromosome)
{
  uint8_t p[R9];
  readNParams(chromosome, R1 + R2 + R3 + R4 + R5 + R6 + R7 + R8, R9, p);
  int priority = p[0];
  int value = (state.closest > p[1] && state.aimDir > p[2]) ? 2 : 0;
  return (Inference){priority, value};
}
#define R10 4
Inference tr4(State state, Chromosome chromosome)
{
  uint8_t p[R10];
  readNParams(chromosome, R1 + R2 + R3 + R4 + R5 + R6 + R7 + R8 + R9, R10, p);
  int priority = p[0];
  int value = 0;
  if (state.speed > p[1] && state.closest < p[2] && state.closestAngle > p[3] && state.closestAngle <= 180)
  {
    value = 1;
  }
  // if the ship is already facing the furthest area, then turn away from nearby geometries
  else if (state.speed > p[1] && state.closest < p[2] && state.closestAngle < 360 - p[3] && state.closestAngle > 180)
  {
    value = -1;
  }
  return (Inference){priority, value};
}
#define R11 4
Inference tr5(State state, Chromosome chromosome)
{
  uint8_t p[R11];
  readNParams(chromosome, R1 + R2 + R3 + R4 + R5 + R6 + R7 + R8 + R9 + R10, R11, p);
  int priority = p[0];
  int value = 0;
  if ((state.furthestAngle > p[1] && state.furthestAngle <= 180))
  {
    value = -1;
  }
  // Always try to face the furthest area
  else if ((state.furthestAngle < 360 - p[1] && state.furthestAngle > 180))
  {
    value = 1;
  }
  return (Inference){priority, value};
}

Inference (*thrusterRules[])(State, const Chromosome *) = {
    rp1,
    rp2,
    rp3,
    rp4,
    rp5,
    rp6,
};

Inference (*turnRules[])(State, const Chromosome *) = {
    tr1,
    tr2,
    tr3,
    tr4,
    tr5,
    // add more here
};

Chromosome *globalChromosome;

int AI_loop()
{
  static int life = 0;
  if (!selfAlive())
  { // if dead
    if (life > 0)
    {           // and we dont know yet,
      life = 0; // set to dead
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
    life = life > 960 ? 960 : life; // cap life to 960 (40 seconds at 24fps)
  }

  if(life == 1)
  {
    // just respawned
    // load chromosome
  }

  setTurnSpeedDeg(20);
  int aimDir = aimdir(0);
  // Release keys
  // Set variables
  double heading = selfHeadingDeg();
  double tracking = selfTrackingDeg();
  double trackWall = wallFeeler(500, tracking);

  // clockwise rotation around the ship
  double frontWall = wallFeeler(500, heading);
  double wall5 = wallFeeler(500, heading + 150);
  double backWall = wallFeeler(500, heading + 180);
  double wall7 = wallFeeler(500, heading + 210);

  int shouldThrust = 0;
  // turn right = 1, turn left = -1, turn to aimdir = 2
  int turnDir = 0;

  double furthest = 0.0;
  int furthestAngle = 0;
  double closest = 600.0;
  int closestAngle = 0;
  for (int i = 0; i < 360; i++)
  {
    double dist = wallFeeler(500, heading + i);
    if (dist > furthest)
    {
      furthest = dist;
      furthestAngle = i;
    }
    if (dist < closest)
    {
      closest = dist;
      closestAngle = i;
    }
  }
  int headingTrackingDiff = (int)(heading + 360 - tracking) % 360;
  int headingAimingDiff = (int)(heading + 360 - aimDir) % 360;

  State s = {
      aimDir,
      frontWall,
      wall5,
      backWall,
      wall7,
      heading,
      tracking,
      trackWall,
      closest,
      closestAngle,
      furthest,
      furthestAngle,
      shotAlert(0),
      selfSpeed(),
      headingTrackingDiff,
      headingAimingDiff,
  };

  int prioThrust = 0.0;
  for (int i = 0; i < sizeof(thrusterRules) / sizeof(thrusterRules[0]); i++)
  {
    Inference inf = thrusterRules[i](s, NULL);
    if (prioThrust < inf.priority)
    {
      prioThrust = inf.priority;
      shouldThrust = inf.result;
    }
  }
  int prioTurn = 0.0;
  for (int i = 0; i < sizeof(turnRules) / sizeof(turnRules[0]); i++)
  {
    Inference inf = turnRules[i](s, NULL);
    if (prioThrust < inf.priority)
    {
      prioThrust = inf.priority;
      turnDir = inf.result;
    }
  }

  if (headingAimingDiff < 90 || headingAimingDiff > 270)
  {
    fireShot(1);
  }

  // Handle commands.
  // the bot seems to behave more accurately and consistently when only the necessary commands are sent
  if (shouldThrust)
    thrust(1);
  else
    thrust(0);
  if (turnDir == 2)
  {
    turnToDeg(aimDir);
  }
  else if (turnDir == 1)
  {
    turnRight(1);
    turnLeft(0);
  }
  else if (turnDir == 0)
  {
    turnRight(0);
    turnLeft(0);
  }
  else if (turnDir == -1)
  {
    turnRight(0);
    turnLeft(1);
  }
  return 0;
}
int main(int argc, char *argv[])
{
  const int chromosomeSize = (R1 + R2 + R3 + R4 + R5 + R6 + R7 + R8 + R9 + R10 + R11) * 8; // total chromosome length in bits

  return start(argc, argv);
}
