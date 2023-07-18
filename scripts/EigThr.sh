#!/bin/bash

ethr=$(( 1000 * $CURRENTSCAN + 4000 ))
caput SR08ID01E2:CAM:ThresholdEnergy $ethr
sleep 1s

