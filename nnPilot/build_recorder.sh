#!/bin/bash

gcc -I../include mlpPilot.c libcAI.so -lm -o MLPRecord -D RECORDER
