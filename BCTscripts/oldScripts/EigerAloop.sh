#~/bin/bash

ethr=$(( 2000 * $CURRENTISCAN + 6000 ))
caput SR08ID01E2:CAM:ThresholdEnergy $ethr
sleep 1s

#caput SR08ID01E2:TIFF:FileNumber 0
#caput -S SR08ID01E2:TIFF:FileName "th_${ethr}_eV"
#caput SR08ID01E2:TIFF:AutoSave 1
#sleep 1s
#caput SR08ID01E2:CAM:Acquire 1
#sleep 0.5s

#while caget SR08ID01E2:CAM:Acquire_RBV | grep -q Acquire ; do 
#   sleep 0.1s
#done

#caput SR08ID01E2:TIFF:AutoSave 0
