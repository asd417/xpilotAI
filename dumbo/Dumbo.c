//Evan Gray - May 2012
//Compile: gcc Spinner.c libcAI.so -o Spinner
//Run: ./Spinner
#include "cAI.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int AI_loop() {
  //Release keys
  //Set variables
  double heading = selfHeadingDeg();
  double tracking = selfTrackingDeg();
  double frontWall = wallFeeler(500,heading);
  double leftWall = wallFeeler(500,heading+5);
  double rightWall = wallFeeler(500,heading-5);
  double backLeftWall = wallFeeler(200,heading+170);
  double backRightWall = wallFeeler(200,heading-170);
  double trackWall = wallFeeler(500,tracking);
  int quad = 0;//quadrant of the map 
  // 1 \ 0
  // 2 \ 3
  // each tile is about 34 pixels big
  int tileX = selfX()/34;
  int tileY = selfY()/34;
  int shouldTurnRight = 0;
  int shouldThrust = 0;
  if(tileX <= 16 && tileY >= 16)
  {
    quad = 1;
  }
  if(tileX > 16 && tileY >= 16)
  {
    quad = 0;
  }
  if(tileX <= 16 && tileY < 16)
  {
    quad = 2;
  }
  if(tileX > 16 && tileY < 16)
  {
    quad = 3;
  }
  
  if(backLeftWall < backRightWall) shouldTurnRight = 1;
  else shouldTurnRight = 0;


  if ((selfSpeed() >= 8 && trackWall > 200) || (selfSpeed() < 1))
  {
    switch(quad)
    {
      case 1:
      if(heading < 135 || heading > 315) shouldTurnRight = 1;
      else shouldTurnRight = 0;
      break;
      case 2:
      if(heading > 225 || heading < 45) shouldTurnRight = 0;
      else shouldTurnRight = 1;
      break;
      case 0:
      if(heading > 45 && heading < 225) shouldTurnRight = 0;
      else shouldTurnRight = 1;
      break;
      case 3:
      if(heading < 315 && heading > 135) shouldTurnRight = 1;
      else shouldTurnRight = 0;
      break;
    }
  }
  else if (leftWall < rightWall)
  {
    shouldTurnRight = 1;
  }

  //Thrust rules
  //printf("frontWall: %f speed: %d\n", frontWall, selfSpeed());
  //printf("heading: %f tracking: %f\n", heading, tracking);
  //printf("%d, %d\n", tileX, tileY);
  //printf("\tQuad: %d\n", quad);
  if (selfSpeed() <= 7 && frontWall >= 200.0)
  {
    shouldThrust = 1;
  }
  else if(trackWall < 250 && abs(heading - tracking) > 110.0) // if ship is heading collision course and thruster can push against it
  {
    shouldThrust = 1;
  }
  else
  {
    shouldThrust = 0;
  }
  if(leftWall < 100 || rightWall < 100) shouldThrust = 0;

  //Turn towards center of the map
  if(shouldTurnRight)
  {
    turnRight(1);
    turnLeft(0);
  }
  else
  {
    turnLeft(1);
    turnRight(0);
  }
  if(shouldThrust) thrust(1);
  else thrust(0);
  fireShot();
  return 0;
}
int main(int argc, char *argv[]) {
  return start(argc, argv);
}
