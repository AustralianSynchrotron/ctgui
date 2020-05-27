#!/bin/bash


PSS_POSITION_PV="SR99ID01DALSA:PPS:RotationPosition_RBV"
DETECTOR_PV="SR99ID01DALSA"
counter=0

camonitor -p 99 -t sI $PSS_POSITION_PV | sed -u -e "s/  \+/ /g"  -e "s +  g" |
while read pv timel val ; do
  if (( $(echo "$timel > 1" |bc -l) )) ; then
    counter=0
  else
    if (( $counter > 5 )) ; then
        caput "${DETECTOR_PV}:CAM:TriggerMode" 2
        sleep 0.2s
        kill -9 $(ps --no-headers --ppid $$ | grep camonitor | sed 's/^ *//g' | cut -d' ' -f 1)
        exit 0
    fi
    (( counter++ ))
  fi
done

