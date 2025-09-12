//Evan Gray - May 2012
//Compile: gcc Spinner.c libcAI.so -o Spinner
//Run: ./Spinner
#include "cAI.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define DEBUGTURN
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


float tanAngle(int x) //converts angle to tan(x/2) so that front is 0, left is positive, right is negative
{
  if(x==180) return 50.0f;
  else return max(min(tan(RAD(x/2)), 50.0f),-50.0f);
}
//these membership functions are shared between linguistic variables ClosestAngle and FurthestAngle
//anticlockwise rotation angle
float mem_Angle_Front(int x)
{
  float tanx = tanAngle(x);
  if(tanx < -0.4f) return 0;
  else if(tanx <= 0) return tanx+1.0f;
  else if(tanx < 0.4f) return -tanx+1.0f;
  else return 0.0f;
}
float mem_Angle_Left(int x)
{
  float tanx = tanAngle(x);
  if(tanx < 0.5f) return 0;
  else if(tanx < 1) return 2*tanx-1.0f;
  else if(tanx < 2) return 1.0f;
  else if(tanx < 3) return -tanx+3.0f;
  else return 0;
}
float mem_Angle_Right(int x)
{
  float tanx = tanAngle(x);
  if(tanx < -3) return 0.0f;
  else if(tanx < -2) return tanx+3.0f;
  else if(tanx < -1) return 1.0f;
  else if(tanx < -0.5f) return -2*tanx-1.0f;
  else return 0;
}
//distance to closest wall
float mem_ClosestWall_Safe(int x)
{
  if(x < 188) return 0.0f;
  else if(x < 132) return 0.015f*x-3.0f;
  else return 1.0f;
}

float mem_ClosestWall_Close(int x)
{
  if(x < 63) return 0.0f;
  else if(x < 187) return 0.008f*x-0.5f;
  else if(x < 312.5f) return -0.008f*x+2.5f;
  else return 0.0f;
}

float mem_ClosestWall_Danger(int x)
{
  if(x > 160) return 0.0f;
  else if(x < 60) return 1.0f;
  else return -0.01f*(x-60)+1.0f;
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
