#!/bin/bash

#USAGE="$0 <folder name> <frames to be acquired> <inshift>"
USAGE="$0 <folder name> <frames to be acquired> [suffix-with-an-underscore-at-the-start]"
TOPDIR="2024-12-04-amir"
#DATALOC="/mnt/asci.data/imbl/input"
DATALOC="/mnt/bctpro.data/${TOPDIR}"
DATADET="/data/${TOPDIR}"
DETPV="SR08ID01E2"
SHIFTMOT="SR08ID01BSS01:Z"


if [ -z "$1" ] ; then
    echo "Error. No data name provided. This should be the folder you want to save in (root is /data/)"
    echo "Usage: $USAGE"
    exit 1
fi
DATA="$1"

if [ -z "$2" ] ; then
    echo "Error. No number of projections provided. This is the number of images to take - check the exposure time and acquisition time manually. Determined by Matt's BCTFarmerDosimetry Doc - ScanParams Tab."
    echo "Usage: $USAGE"
    exit 1
fi
PROJ="$2"

#if [ -z "$3" ] ; then
#    echo "Error. No shifter position."
#    exit 1
#fi
#INSHIFT="$3"
#INORIGN="$(caget -t ${SHIFTMOT}.RBV)"
INSHIFT=150
INORIGN=0
SUFFIX=""
if [ ! -z "$3" ] ; then
SUFFIX="$3"
fi


waitCA () {
  grep -q  "$2" < <(camonitor -t n -p 99 -S "$1" | sed -u -r -e 's: +: :g' -e 's: $::g' -e 's:.* ::g' )
}

acquire() {

    caput ${DETPV}:CAM:ArrayCounter 0
    caput ${DETPV}:CAM:NumImages $PROJ
    caput ${DETPV}:HDF:NumCapture $PROJ
    if [ ! -z "$1" ] ; then
        caput -S  ${DETPV}:HDF:FileName "$1"    
    fi
    caput ${DETPV}:HDF:AutoSave 1
    caput ${DETPV}:HDF:FileWriteMode 2
    echo "refresh capture state"
    caput ${DETPV}:HDF:Capture 1
    sleep 0.5
    caput ${DETPV}:HDF:Capture 0
    sleep 0.5
    echo "set capture state true"
    caput ${DETPV}:HDF:Capture 1
    sleep 2s
    if caget ${DETPV}:HDF:Capture_RBV | grep -q Done ; then
        echo "Error. HDF did not enter capturing state"
    fi

    caput ${DETPV}:CAM:Acquire 1
    sleep 1s
    waitCA ${DETPV}:CAM:Acquire Done 
    waitCA ${DETPV}:HDF:Capture_RBV Done
    waitCA ${DETPV}:CAM:WriteFile_RBV Done

}


move() {
  caput ${1} ${2}
  sleep 0.5s
  waitCA ${1}.DMOV 1
}


LOCAQPATH="${DATALOC}/${DATA}"
echo "$LOCAQPATH"
mkdir -m 777 -p "$LOCAQPATH"
if [ ! -e "$LOCAQPATH" ] ; then
    echo "Failed to create acquisition path: '$LOCAQPATH'"
    exit 1
fi

caput SR08ID01BCT01:Pps_Det_Start 0 #disable PPS triggering to clear prior position
caput ${DETPV}:CAM:Acquire 0 #stop acquiring if still live image. Also shuts imaging shutter
caput ${DETPV}:HDF:Capture 0 #cancel any current file writing
sleep 0.5
DETAQPATH="${DATADET}/${DATA}"
caput -S  ${DETPV}:HDF:FilePath "$DETAQPATH"
sleep 0.5s
if caget ${DETPV}:HDF:FilePathExists_RBV | grep -q "No" ; then
    echo "Error. Detector can't see data path '$DETAQPATH'"
    exit 1
fi


acquire "BG_$SUFFIX"





