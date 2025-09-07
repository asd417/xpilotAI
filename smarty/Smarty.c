//Evan Gray - May 2012
//Compile: gcc Spinner.c libcAI.so -o Spinner
//Run: ./Spinner
#include "cAI.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

//#define DEBUGTURN
#define DEBUGTHRUST
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
  //printf("heading %f, tracking %f, diff: %d, trackWall: %f\n", heading, tracking, headingTrackingDiff, trackWall);
  //Thrust rules
  if(shotDanger > 0 && shotDanger < 200 && frontWall > 200)
  {
    THRUSTDEBUG(AR(printf("avoiding shot: %d\n", shotDanger)));
    shouldThrust = 1;
  }
  else if((furthest_angle == 0) && selfSpeed() < 6 && frontWall > 200)
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
    THRUSTDEBUG(AR(printf("thrusters type4\n")));
    shouldThrust = 1;
  }
  else if(wall7 < 70 && frontWall > 250)
  {
    THRUSTDEBUG(AR(printf("thrusters type5\n")));
    shouldThrust = 1;
  }

  //AR(printf("headingTrackingDiff: %d\n", headingTrackingDiff));
  //Turn rules
  //Speed too fast, turn to face against tracking direction
  if(selfSpeed() > 9 && headingTrackingDiff < 175)
  {
    TURNDEBUG(AR(printf("Turning reverse\n")));
    turnDir = -1;
  }
  //Speed too fast, turn to face against tracking direction
  else if(selfSpeed() > 9 && headingTrackingDiff > 185)
  {
    TURNDEBUG(AR(printf("Turning reverse\n")));
    turnDir = 1;
  }
  // aim at enemy
  else if(closest > 70 && aimDir > 0 && headingAimingDiff < 180 && headingAimingDiff > 40)
  {
    TURNDEBUG(AR(printf("Aiming Roughly\n")));
    turnDir = 1;
  }
  else if(closest > 70 && aimDir > 0 && headingAimingDiff > 180 && headingAimingDiff < 320)
  {
    TURNDEBUG(AR(printf("Aiming Roughly\n")));
    turnDir = -1;
  }
  else if(closest > 70 && aimDir > 0)
  {
    TURNDEBUG(AR(printf("Aiming Accurately\n")))
    turnToDeg(aimDir);
    turnDir = 0;
  }
  else if(selfSpeed() > 1 && closest < 100 && closest_angle > 2 && closest_angle <= 180)
  {
    TURNDEBUG(AR(printf("Turning away\n")));
    turnDir = 1;
  }
  //if the ship is already facing the furthest area, then turn away from nearby geometries
  else if(selfSpeed() > 1 && closest < 100 && closest_angle < 358 && closest_angle > 180)
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
  
  //printf("ship angle: %f turning to %d", heading, aimDir);
  
  if(headingAimingDiff < 90 || headingAimingDiff > 270)
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
