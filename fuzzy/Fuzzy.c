//Evan Gray - May 2012
//Compile: gcc Spinner.c libcAI.so -o Spinner
//Run: ./Spinner
#include "cAI.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

//#define DEBUGTURN
//#define DEBUGTHRUST
#define RAD(x) x/180*atan(1)*4
//Alive run, only run when alive
#define AR(x) if(selfAlive()){x;}
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


//integral of ax^2+bx over x values between x=minx and x=maxx
float integrateLinearX(float a, float b, float minx, float maxx)
{
  if(minx > maxx) return 0.0f;
  else return a*(pow(maxx, 3)-pow(minx, 3))/3.0f + b*(pow(maxx, 2)-pow(minx, 2))/2.0f;
}
//integral of ax^2+bx over x values between y=0 and y=maxy
float integrateLinearXWithY(float a, float b, float maxy)
{
  float x1 = (maxy-b)/a;
  float x2 = -b/a;
  if(x1 > x2) return integrateLinearX(a, b, x2, x1);
  else return integrateLinearX(a, b, x1, x2);
}
//for area under linear function ax + b over x values between x=minx and x=maxx
float integrateLinear(float a, float b, float minx, float maxx)
{
  if(minx > maxx) return 0.0f;
  else return a*(pow(maxx, 2)-pow(minx, 2))/2.0f + b*((maxx-minx));
}
//for area under linear function ax + b over x values between y=0 and y=maxy
float integrateLinearWithY(float a, float b, float maxy)
{
  float x1 = (maxy-b)/a;
  float x2 = -b/a;
  if(x1 > x2) return integrateLinear(a, b, x2, x1);
  else return integrateLinear(a, b, x1, x2);
}
float integrateBox(float y, float minx, float maxx)
{
  if(minx > maxx) return 0.0f;
  else return y * (maxx - minx);
}
float integrateBoxX(float y, float minx, float maxx)
{
  if(minx > maxx) return 0.0f;
  else return y * (pow(maxx,2) - pow(minx,2))/2.0f;
}

//integral of piecewise function defined by f1a*x+f1b, y=maxy, f2a*x+f2b
//assume f1 is left and f3 is right
float integrateDoubleLinear(float maxy, float f1a, float f1b, float f2a, float f2b)
{
  float p1 = (maxy-f1b)/f1a;
  float p2 = (maxy-f2b)/f2a;
  //assume p2 > p1
  return integrateLinearWithY(f1a, f1b, maxy) + integrateBox(maxy, p1, p2) + integrateLinearWithY(f2a, f2b, maxy);
}

//integral piecewise function defined by (f1a*x+f1b)x, y=maxy*x, (f2a*x+f2b)x
//assume f1 is left and f3 is right
float integrateDoubleLinearX(float maxy, float f1a, float f1b, float f2a, float f2b)
{
  float p1 = (maxy-f1b)/f1a;
  float p2 = (maxy-f2b)/f2a;
  //assume p2 > p1
  return integrateLinearXWithY(f1a, f1b, maxy) + integrateBoxX(maxy, p1, p2) + integrateLinearXWithY(f2a, f2b, maxy);
}
//calculate centroid with sum of all integrateDoubleLinearX over integrateDoubleLinear

float tanAngle(int x) //converts angle to tan(x/2) so that front is 0, left is positive, right is negative
{
  if(x==180) return 50.0f;
  else return fmax(fmin(tan(RAD(x/2.0f)), 50.0f),-50.0f);
}
//these membership functions are shared between linguistic variables ClosestAngle and FurthestAngle
//anticlockwise rotation angle
float mem_Angle_Front(int x)
{
  float tanx = tanAngle(x);
  if(tanx <= 0) return fmax(5*tanx+1.0f, 0.0f);
  else return fmax(-5*tanx+1.0f, 0.0f);
}
float mem_Angle_Left(int x)
{
  float tanx = tanAngle(x);
  return fmin(fmax(2*tanx-1.0f, 0.0f), 1.0f);
}
float mem_Angle_Right(int x)
{
  float tanx = tanAngle(x);
  return fmin(fmax(-2*tanx-1.0f, 0.0f), 1.0f);
}
//distance to closest wall
float mem_Wall_Safe(int x)
{
  return fmin(fmax(0.015f*x-5.0f, 0.0f), 1.0f);
}
float mem_Wall_Close(int x)
{
  if(0.008f*x-1.2f < 1) return fmin(fmax(0.008f*x-1.2f, 0.0f), 1.0f);
  else return fmin(fmax(-0.009f*x+3.6f, 0.0f),1.0f);
}
float mem_Wall_Danger(int x)
{
  return fmin(fmax(-0.006f*x+1.6f, 0.0f), 1.0f);
}

//centroid for turn. requires three fuzzy inputs and outputs clear value
float centroidTurn(float turnLeft, float noTurn, float turnRight)
{
  float denum = integrateDoubleLinear(turnLeft,2,0,-2,2) + integrateDoubleLinear(noTurn,3,-2,-3,4) + integrateDoubleLinear(turnRight,2,-2,-2,4);
  if(denum == 0.0f) return 1.0f;
  float num = integrateDoubleLinearX(turnLeft,2,0,-2,2) + integrateDoubleLinearX(noTurn,3,-2,-3,4) + integrateDoubleLinearX(turnRight,2,-2,-2,4);
  return num/denum;
}

float and(float a, float b)
{
  return a < b ? a : b;
}
float or(float a, float b)
{
  return a > b ? a : b;
}

//If ClosestWall Danger and closestAngle Left then turn right
float r1(float closest, float closestAngle)
{
  return and(mem_Wall_Danger(closest), mem_Angle_Left(closestAngle));
}
//If ClosestWall Danger and closestAngle Right then turn left
float r2(float closest, float closestAngle)
{
  return and(mem_Wall_Danger(closest), mem_Angle_Right(closestAngle));
}
//If ClosestWall Close and closestAngle Left then turn right
float r3(float closest, float closestAngle)
{
  return and(mem_Wall_Close(closest), mem_Angle_Left(closestAngle));
}
//If ClosestWall Close and closestAngle Right then turn left
float r4(float closest, float closestAngle)
{
  return and(mem_Wall_Close(closest), mem_Angle_Right(closestAngle));
}
//If ClosestWall Safe Noturn
float r5(float closest)
{
  return mem_Wall_Safe(closest);
}
//If FurthestAngle Right turn Right
float r6(float furthestAngle)
{
  return mem_Angle_Right(furthestAngle);
}
//If FurthestAngle Left turn Left
float r7(float furthestAngle)
{
  return mem_Angle_Left(furthestAngle);
}
//If FurthestAngle Forward dont turn
float r8(float furthestAngle)
{
  return mem_Angle_Front(furthestAngle);
}

float turnRules(float closest, float closestAngle, float furthestAngle)
{
  float q = mem_Wall_Safe(closest);
  float w = mem_Wall_Close(closest);
  float e = mem_Angle_Left(closestAngle);
  float r = mem_Angle_Right(closestAngle);
  //printf("safe: %.2f close: %.2f left: %.2f right: %.2f\n", q, w, e, r);

  float turnLeft = r2(closest, closestAngle) + r4(closest, closestAngle) + r7(furthestAngle);
  float noTurn = r5(closest) + r8(furthestAngle);
  float turnRight = r1(closest, closestAngle) + r3(closest, closestAngle) + r6(furthestAngle);

  //AR(printf("left: %.2f noturn: %.2f right: %.2f\n", turnLeft, noTurn, turnRight));
  return centroidTurn(turnLeft, noTurn, turnRight);
}

int AI_loop() {
  setTurnSpeedDeg(20);
  int aimDir = aimdir(0);
  //Release keys
  //Set variables
  double heading = selfHeadingDeg();
  double tracking = selfTrackingDeg();
  double trackWall = wallFeeler(500,tracking);

  //clockwise rotation around the ship
  double frontWall = wallFeeler(500,heading);
  double wall1 = wallFeeler(500,heading+30);
  double wall2 = wallFeeler(500,heading+60);
  double wall3 = wallFeeler(500,heading+90);
  double wall4 = wallFeeler(500,heading+120);
  double wall5 = wallFeeler(500,heading+150);
  double backWall = wallFeeler(500,heading+180);
  double wall7 = wallFeeler(500,heading+210);
  double wall8 = wallFeeler(500,heading+240);
  double wall9 = wallFeeler(500,heading+270);
  double wall10 = wallFeeler(500,heading+300);
  double wall11 = wallFeeler(500,heading+330);
  
  int shouldThrust = 0;
  //turn right = 1, turn left = -1
  int turnDir = 0;

  

  double furthest = 0.0;
  int furthest_angle = 0;
  double closest = 600.0;
  int closest_angle = 0;
  for(int i=0;i<360;i++)
  {
    double dist = wallFeeler(500,heading+i);
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

  int headingTrackingDiff = (int)(heading + 360 - tracking) % 360;
  int headingAimingDiff = (int)(heading + 360 - aimDir) % 360;
  int shotDanger = shotAlert(0);
  if(shotDanger > 0 && shotDanger < 200 && frontWall > 200 && trackWall > 80)
  {
    THRUSTDEBUG(AR(printf("avoiding shot: %d\n", shotDanger)));
    shouldThrust = 1;
  }
  else if((furthest_angle == 0) && selfSpeed() < 7 && frontWall > 200)
  {
    THRUSTDEBUG(AR(printf("propulsion\n")));
    shouldThrust = 1;
  }
  else if(headingTrackingDiff > 110 && headingTrackingDiff < 250 && trackWall < 300 && frontWall > 250)
  {
    THRUSTDEBUG(AR(printf("thrusters type 1\n")));
    shouldThrust = 1;
  }
  else if(backWall < 70 && frontWall > 250)
  {
    THRUSTDEBUG(AR(printf("thrusters type 2\n")));
    shouldThrust = 1;
  }
  else if(headingTrackingDiff > 85 && headingTrackingDiff < 275 && trackWall < 100 && frontWall > 200)
  {
    THRUSTDEBUG(AR(printf("thrusters type 3\n")));
    shouldThrust = 1;
  }
  else if(wall5 < 70 && frontWall > 250)
  {
    THRUSTDEBUG(AR(printf("thrusters type 4\n")));
    shouldThrust = 1;
  }
  else if(wall7 < 70 && frontWall > 250)
  {
    THRUSTDEBUG(AR(printf("thrusters type 5\n")));
    shouldThrust = 1;
  }
  else if(selfSpeed() < 5)
  {
    shouldThrust = 1;
  }

  float turn = turnRules(closest, closest_angle, furthest_angle);
  AR(printf("centroid: %f\n", turn));
  if(turn < 0.75) turnDir = -1;
  else if(turn < 1.25) turnDir = 0;
  else turnDir = 1;

  if(shouldThrust) thrust(1);
  else thrust(0);
  if(turnDir == 1)
  {
    turnRight(1);
    turnLeft(0);
  }
  else if(turnDir == 0)
  {
    turnRight(0);
    turnLeft(0);
  }
  else if(turnDir == -1)
  {
    turnRight(0);
    turnLeft(1);
  }
  return 0;
}
int main(int argc, char *argv[]) {
  return start(argc, argv);
}
