//Evan Gray - May 2012
//Compile: gcc Spinner.c libcAI.so -o Spinner
//Run: ./Spinner
#include "cAI.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define DEBUGTURN
//#define DEBUGTHRUST
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

int AI_loop() {
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
  //printf("heading %f, tracking %f, diff: %d, trackWall: %f\n", heading, tracking, headingTrackingDiff, trackWall);
  //Thrust rules
  if((furthest_angle == 0) && selfSpeed() < 6 && frontWall > 200)
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

  //Turn rules
  //Speed too fast, turn to face against tracking direction
  if(selfSpeed() > 7 && headingTrackingDiff < 160)
  {
    TURNDEBUG(AR(printf("Turning reverse\n")));
    turnDir = -1;
  }
  //Speed too fast, turn to face against tracking direction
  else if(selfSpeed() > 7 && headingTrackingDiff > 200)
  {
    TURNDEBUG(AR(printf("Turning reverse\n")));
    turnDir = 1;
  }
  else if(selfSpeed() > 1 && closest < 150 && closest_angle > 2 && closest_angle <= 180)
  {
    TURNDEBUG(AR(printf("Turning away\n")));
    turnDir = 1;
  }
  //if the ship is already facing the furthest area, then turn away from nearby geometries
  else if(selfSpeed() > 1 && closest < 150 && closest_angle < 358 && closest_angle > 180)
  {
    TURNDEBUG(AR(printf("Turning away\n")));
    turnDir = -1;
  }
  //Always try to face the furthest area
  else if(furthest_angle > 3 && furthest_angle <= 180)
  {
    TURNDEBUG(AR(printf("Turning face\n")));
    turnDir = -1;
  }
  //Always try to face the furthest area
  else if(furthest_angle < 357 && furthest_angle > 180)
  {
    TURNDEBUG(AR(printf("Turning face\n")));
    turnDir = 1;
  }
  //if the ship is already facing the furthest area, then turn away from nearby geometries
  
  // aim at enemy
  else if(aimDir > 0 && aimDir < heading)
  {
    TURNDEBUG(AR(printf("Aiming enemy\n")));
    turnDir = 1;
  }
  else if(aimDir > 0 && aimDir > 180 && aimDir - 180 > heading)
  {
    TURNDEBUG(AR(printf("Aiming enemy\n")));
    turnDir = 1;
  }
  else if(aimDir > 0 && aimDir > heading)
  {
    TURNDEBUG(AR(printf("Aiming enemy\n")));
    turnDir = -1;
  }
  //printf("ship angle: %f turning to %d", heading, aimDir);
  
  if(backWall > 50 && aimDir == heading)
  {
    fireShot(1);
  }
  //shot speed is 21
  
  //Handle commands. 
  //the bot seems to behave more accurately and consistently when only the necessary commands are sent
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
