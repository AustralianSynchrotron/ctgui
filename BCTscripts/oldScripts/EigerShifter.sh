#!/bin/bash

USAGE="$0 <shiftprojection> <inshift>"

DETPV="SR08ID01E2"
SHIFTMOT="SR08ID01BSS01:Z"

if [ -z "$1" ] ; then
    echo "Error. No shift projection."
    exit 1
fi
SHIFTPROJ="$1"

if [ -z "$2" ] ; then
    echo "Error. No shifter position."
    exit 1
fi
INSHIFT="$2"


IMAGECNT="${DETPV}:CAM:NumImagesCounter_RBV"
HDFCAPTR="${DETPV}:HDF:Capture_RBV"
camonitor -p 99 -n -S $IMAGECNT | sed -u "s/  \+/ /g" |
while read pv date time val ; do
  if (( $val >= $SHIFTPROJ )) ; then
    caput "${SHIFTMOT}" $INSHIFT
    killall -9 $0
    exit 0
  fi
done


