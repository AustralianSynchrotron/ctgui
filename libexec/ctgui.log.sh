#!/bin/bash

USAGE="$0 detectorPV rotationPV"

if [ -z "${1}" ] ; then
  echo "No detector PV is given" 1>&2
  echo "$USAGE" 1>&2
  exit 1
fi
NUMPV="${1}:CAM:NumImagesCounter_RBV"
IMGPV="${1}:TIFF:FileTemplate"
AQSPV="${1}:CAM:Acquire_RBV"

if [ -z "${2}" ] ; then
  echo "No rotation PV is given" 1>&2
  echo "$USAGE" 1>&2
  exit 1
fi
ROTPV="${2}.RBV"

shift 2
OTHERPVS="$@"

ROT=0
IMG=""
AQS=0

camonitor -p 99 -n -S $ROTPV $NUMPV $IMGPV $AQSPV | sed -u "s/  \+/ /g" |
while read pv date time val ; do

  case "$pv" in
    ${AQSPV} )
      val=$(echo $val | sed "s: .*::g") # to remove "STATE MINOR" message
      if [ "$AQS" != "$val"  ] ; then
        AQS="$val"
        if [ "$AQS" == "0" ] ; then
          echo "${date}_${time} MSG: Acquisition finished."
        else
          echo "${date}_${time} MSG: Acquisition started with the image file template \"${IMG}\"."
        fi
      fi
      ;;
    ${ROTPV} ) ROT="$val" ;;
    ${IMGPV} ) IMG="$val" ;;
    ${NUMPV} )
      if [ "$AQS" == "1" ] ; then
        echo "${date}_${time} ${val} ${ROT}"
      fi
      ;;
    *        )  ;;  # TODO: implement OTHERPVS
  esac

done

