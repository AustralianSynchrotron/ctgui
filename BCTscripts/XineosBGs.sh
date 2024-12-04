#!/bin/bash

#USAGE="$0 <folder name> <frames to be acquired> <inshift>"
USAGE="$0 <folder name> <frames to be acquired> [suffix-with-an-underscore-at-the-start]"

DATALOC="/mnt/asci.data/imbl/input"
DATADET="x:/imbl/input/"
DETPV="SR99ID01DALSA"

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

    caput ${DETPV}:HDF:Capture 1
    sleep 2s
    if caget ${DETPV}:HDF:Capture_RBV | grep -q Done ; then
        echo "Error. HDF did not enter capturing state"
    fi

    caput ${DETPV}:CAM:Acquire 1
    sleep 1s
    waitCA ${DETPV}:CAM:Acquire Done 
    echo "done 1"
    waitCA ${DETPV}:HDF:Capture_RBV Done
    echo "done 2"
    #waitCA ${DETPV}:CAM:WriteFile_RBV Done
    #echo "done 3"
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
caput SR08ID01E2:CAM:Acquire 0 #stop any current acquisitions (and close IPASS imaging shutter)
caput SR08ID01E2:HDF:Capture 0 #cancel any current file writing
sleep 0.5
DETAQPATH="${DATADET}/${DATA}"
caput -S  ${DETPV}:HDF:FilePath "$DETAQPATH"
sleep 0.5s
if caget ${DETPV}:HDF:FilePathExists_RBV | grep -q "No" ; then
    echo "Error. Detector can't see data path '$DETAQPATH'"
    exit 1
fi

#turn on the beam with eiger for BGs
caput SR08ID01E2:CAM:NumImagesCounter_RBV 0 #set cam num images to be 0 manually - don't acquire 1 unless you want imaging shutter to open!!!!!!!
caput SR08ID01E2:CAM:NumImages 10000
caput SR08ID01E2:CAM:AcquireTime 0.5
caput SR08ID01E2:CAM:AcquirePeriod 0.5
caput SR08ID01E2:HDF:AutoSave 0
sleep 1s
caput SR08ID01E2:CAM:Acquire 1
acquire "BG_XIN$SUFFIX"
#now turn off the beam with eiger for DFs
caput SR08ID01E2:CAM:Acquire 0
sleep 1s
acquire "DF_XIN$SUFFIX"



