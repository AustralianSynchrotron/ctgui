#!/bin/bash

USAGE="$0 <folder name> <projections> [filename]"
#USAGE="$0 <data name> <projections> [shiftpos]" #Deprecated - used to be used for shifting table, but we 

DATALOC="/mnt/asci.data/imbl/input"
#DATALOC="/mnt/bctpro.data/"
DATADET="x:/imbl/input"
#DATADET="/data/"
DETPV="SR99ID01DALSA"

if [ -z "$1" ] ; then
    echo "Error. No folder name provided. This should be the folder you want to save in (root is /data/)"
    echo "Usage: $USAGE"
    exit 1
fi
FOLDER="$1"

if [ -z "$2" ] ; then
    echo "This is the number of images to take - check the exposure time and acquisition time manually. Determined by Matt's BCTFarmerDosimetry Doc - ScanParams Tab."
    echo "Usage: $USAGE"
    exit 1
fi
PROJ="$2"

FILE="SAMPLE"
if [ ! -z "$3" ] ; then
    FILE="$3"
fi

#SHIFTPROJ="0"
#if [ ! -z "$3" ] ; then
#  SHIFTPROJ="$3"
#  if (( 2 * $SHIFTPROJ >= $PROJ )) ; then
#      echo "Number of projections ($PROJ) is less than two shifts ($SHIFTPROJ)."
#      echo "Usage: $USAGE"
#      exit 1
#  fi
#fi

waitCA () {
  grep -q  "$2" < <(camonitor -t n -p 99 -S "$1" | sed -u -r -e 's: +: :g' -e 's: $::g' -e 's:.* ::g' )
}

LOCAQPATH="${DATALOC}/${FOLDER}"
echo "$LOCAQPATH"
mkdir -m 777 -p "$LOCAQPATH" 
if [ ! -e "$LOCAQPATH" ] ; then
    echo "Failed to create acquisition path: '$LOCAQPATH'"
    exit 1
fi

caput SR08ID01BCT01:Pps_Det_Start 0 #set to zero to clear old 'zero position of A' info
caput ${DETPV}:CAM:Acquire 0 #stop any current acquisitions (and close IPASS imaging shutter)
caput ${DETPV}:HDF:Capture 0 #cancel any current file writing
caput SR08ID01E2:CAM:Acquire 0 #stop any current acquisitions (and close IPASS imaging shutter)
caput SR08ID01E2:HDF:Capture 0 #cancel any current file writing
sleep 0.5

DETAQPATH="${DATADET}/${FOLDER}"
caput -S  ${DETPV}:HDF:FilePath "$DETAQPATH"
sleep 0.5s
if caget ${DETPV}:HDF:FilePathExists_RBV | grep -q "No" ; then
    echo "Error. Detector can't see data path '$DETAQPATH'"
    exit 1
fi
caput -S ${DETPV}:HDF:FileName "$FILE"
caput ${DETPV}:CAM:NumImagesCounter_RBV 0 #set cam num images to be 0 manually - don't acquire 1 unless you want imaging shutter to open!!!!!!!
caput SR08ID01E2:CAM:NumImagesCounter_RBV 0 #set cam num images to be 0 manually - don't acquire 1 unless you want imaging shutter to open!!!!!!!

caput ${DETPV}:CAM:ArrayCounter 0
caput ${DETPV}:CAM:NumImages $PROJ
caput ${DETPV}:HDF:NumCapture $PROJ
caput ${DETPV}:HDF:AutoSave 1
caput ${DETPV}:HDF:FileWriteMode 2
caput ${DETPV}:HDF:Capture 1
caput ${DETPV}:CAM:ImageMode Multiple
#caput SR08ID01E2:CAM:ArrayCounter 0
caput SR08ID01E2:CAM:NumImages 100000
caput SR08ID01E2:CAM:AcquireTime 0.5
caput SR08ID01E2:CAM:AcquirePeriod 0.5

caput SR08ID01E2:HDF:AutoSave 0
# Enable the PPS trigger start capability. A6 zero anngle recorded at this point.
caput SR08ID01BCT01:Pps_Det_Start 1
sleep 1s

caget ${DETPV}:HDF:Capture_RBV
if caget ${DETPV}:HDF:Capture_RBV | grep -q Done ; then
    echo "Error. HDF did not enter capturing state"
    exit 1
fi
echo "Make sure to manually open the imbl safety shutter or you won't get radiation"
echo "READY"



IMAGECNT="SR08ID01E2:CAM:NumImagesCounter_RBV"
camonitor -p 99 -n -S $IMAGECNT | while read pv date time val ; 
do
    if (( $val >= 1 )) ; then
        caput ${DETPV}:CAM:Acquire 1
        echo "Starting Xin acquisition now that Eiger has started."
          killall camonitor
      fi
done

waitCA ${DETPV}:HDF:Capture_RBV Done
#stop eiger now that Xin is done
caput SR08ID01E2:CAM:Acquire 0 

# Disable the PPS trigger capability
caput SR08ID01BCT01:Pps_Det_Start 0

# Copy the PPS log file to the data directory
sleep 1.0s
latestLog="$(ls -t /mnt/bctpro.data/*.txt | head -n 1)"
cp "$latestLog" "${LOCAQPATH}/"
hdfname=`caget -St SR99ID01DALSA:HDF:FileName_RBV`
cp "$latestLog" "${LOCAQPATH}/${hdfname}-PPSstream.txt"

echo "DONE - Don't forget your Flats!!!!"

