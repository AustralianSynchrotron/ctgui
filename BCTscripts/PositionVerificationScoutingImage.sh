#!/bin/bash

USAGE="$0 <exposure period> <frames to be acquired>"

DETPV="SR08ID01E2"

if [ -z "$1" ] ; then
    echo "Error. No exposure period provided. I suggest 0.005"
    echo "Usage: $USAGE"
    exit 1
fi
EXPOSURE="$1"

if [ -z "$2" ] ; then
    echo "Error. No number of frames provided. I suggest 25"
    echo "Usage: $USAGE"
    exit 1
fi
PROJ="$2"

caput SR08ID01BCT01:Pps_Det_Start 0 #disable PPS triggering to clear prior position
caput ${DETPV}:CAM:Acquire 0 #stop acquiring if still live image. Also shuts imaging shutter
caput ${DETPV}:HDF:Capture 0 #cancel any current file writing
sleep 0.5s

caput ${DETPV}:CAM:ArrayCounter 0
caput ${DETPV}:CAM:NumImages $PROJ
caput ${DETPV}:HDF:AutoSave 0
caput ${DETPV}:CAM:AcquireTime $EXPOSURE
caput ${DETPV}:CAM:AcquirePeriod $EXPOSURE
sleep 1s

caput ${DETPV}:CAM:Acquire 1
echo "Acquiring scouting image - if blank, check that the IMBL safety shutter is open and that you have all shutter permits and try again."