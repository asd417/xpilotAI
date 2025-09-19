#!/bin/bash
cp /lib/xpilot-ai/binaries/libcAI.so ./libcAI.so
gcc -I../include Fuzzy.c libcAI.so -lm -o Fuzzy
