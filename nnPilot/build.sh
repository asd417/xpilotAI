#!/bin/bash

gcc -I../include mlpPilot.c libcAI.so -lm -o MLP -D PLAYER
