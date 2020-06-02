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
        camonpid=$(ps --no-headers --ppid $$ | grep camonitor | sed 's/^ *//g' | cut -d' ' -f 1)
        if [ ! -z "$camonpid" ] ; then
            kill -9 $camonpid
        fi
        exit 0
    fi
    (( counter++ ))
  fi
done

