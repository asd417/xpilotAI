#!/bin/bash

export LD_LIBRARY_PATH="/lib/xpilot-ai/binaries/:$LD_LIBRARY_PATH"
export LD_LIBRARY_PATH="$HOME/xpilot-ai/binaries/:$LD_LIBRARY_PATH"
./Fuzzy -name Fuzzy -join
