#!/bin/bash
 
SHOTS=${1:-500}
STEPS=${2:-15}

ml qmio/hpc  gcccore/12.3.0 python/3.11.9
 
./build/bell_circuit "$SHOTS" "$STEPS"
 

 
