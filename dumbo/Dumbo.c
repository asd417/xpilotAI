//Evan Gray - May 2012
//Compile: gcc Spinner.c libcAI.so -o Spinner
//Run: ./Spinner
#include "cAI.h"
#include <stdio.h>

int AI_loop() {
  turnLeft(1);
  //Release keys
  thrust(0);
  turnLeft(0);
  turnRight(0);
  //Set variables
  double heading = selfHeadingDeg();
  double tracking = selfTrackingDeg();
  double frontWall = wallFeeler(500,heading);
  double leftWall = wallFeeler(500,heading+5);
  double rightWall = wallFeeler(500,heading-5);
  double trackWall = wallFeeler(500,tracking);
  //Thrust rules
  if (selfSpeed() <= 20 && frontWall >= 15.0)
  {
    thrust(1);
  }
  else if(trackWall < 15) // if ship is heading collision course
  {
    thrust(1);
  }
  //Turn rules
  if(leftWall < rightWall)
  {
    turnRight(1);
  }
  else {
    turnLeft(1);
  }
  fireShot();
  return 0;
}
int main(int argc, char *argv[]) {
  return start(argc, argv);
}