#!/bin/bash

USAGE="$0 detectorPV rotationPV"

if [ -z "${1}" ] ; then
  echo "No detector PV is given" 1>&2
  echo "$USAGE" 1>&2
  exit 1
fi
NUMPV="${1}:CAM:NumImagesCounter_RBV"
TIFFPV="${1}:TIFF:FileName"
HDF5PV="${1}:HDF:FileName"
TIFFACTIVE="${1}:TIFF:EnableCallbacks"
HDF5ACTIVE="${1}:HDF:EnableCallbacks"
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
FMT=""
TIFFname=""
HDF5name=""
AQS=0

camonitor -p 99 -n -S $ROTPV $NUMPV $TIFFPV $HDF5PV $TIFFACTIVE $HDF5ACTIVE $AQSPV | sed -u "s/  \+/ /g" |
while read pv date time val ; do
  case "$pv" in
    ${AQSPV} )
      val=$(echo $val | sed "s: .*::g") # to remove "STATE MINOR" message
      if [ "$AQS" != "$val"  ] ; then
        AQS="$val"
        if [ "$AQS" == "0" ] ; then
          echo "${date}_${time} Acquisition finished."
        else
          IMG=""
          if [ "$FMT" == "TIFF" ] ; then
            IMG="$TIFFname"
          elif [ "$FMT" == "HDF5" ] ; then  
            IMG="$HDF5name"
          fi 
          echo "${date}_${time} Acquisition started with ${FMT} filename prefix \"${IMG}\"."
        fi
      fi
      ;;
    ${TIFFPV} )  TIFFname="$val" ;;
    ${HDF5PV} )  HDF5name="$val" ;;
    ${TIFFACTIVE} ) if [ "$val" == "1" ] ; then FMT="TIFF" ; fi ;;
    ${HDF5ACTIVE} ) if [ "$val" == "1" ] ; then FMT="HDF5" ; fi ;; 
    ${ROTPV} ) ROT="$val" ;;
    ${NUMPV} )
      if [ "$val" != "0" ] && [ "$AQS" == "1" ] ; then
        echo "${date}_${time} $(( ${val} - 1 )) ${ROT}"
      fi
      ;;
    *        )  ;;  # TODO: implement OTHERPVS
  esac

done

exit 0

