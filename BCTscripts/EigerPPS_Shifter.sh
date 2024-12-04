#!/bin/bash

USAGE="$0 <folder name> <projections> <shiftFrame> [filename] "

TOPDIR="2024-12-04-amir"
DATALOC="/mnt/bctpro.data/${TOPDIR}"
DATADET="/data/${TOPDIR}"
DETPV="SR08ID01E2"
SHIFTMOT="SR08ID01BSS01:Z"
INSHIFT=150
INORIGN=0

if [ -z "$1" ] ; then
    echo "Error. No folder name provided. This should be the folder you want to save in (root is /data/)"
    echo "Usage: $USAGE"
    exit 1
fi
FOLDER="$1"

if [ -z "$2" ] ; then
    echo "Error. No number of projections provided. This is the number of images to take - check the exposure time and acquisition time manually. Determined by Matt's BCTFarmerDosimetry Doc - ScanParams Tab."
    echo "Usage: $USAGE"
    exit 1
fi
PROJ="$2"

# Set the detector start PV to disabled
caput SR08ID01BCT01:Pps_Det_Start 0

SHIFTPROJ="0"
if [ ! -z "$3" ] ; then
  SHIFTPROJ="$3"
  if (( 2 * $SHIFTPROJ >= $PROJ )) ; then
      echo "This is the frame at which we will send the shift command - see Matt's BCTFarmerDosimetry doc ScanParams tab for the calculator"
      echo "Usage: $USAGE"
      exit 1
  fi
fi

FILE="SAMPLE"
if [ ! -z "$4" ] ; then
    FILE="$4"
fi


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

caput "${SHIFTMOT}" $INORIGN #set shifter to origin
caput SR08ID01BCT01:Pps_Det_Start 0 #disable PPS triggering again to clear prior position
caput ${DETPV}:CAM:Acquire 0 #stop acquiring if still live image. Also shuts imaging shutter
caput ${DETPV}:HDF:Capture 0 #cancel any current file writing
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

caput ${DETPV}:CAM:ArrayCounter 0
caput ${DETPV}:CAM:NumImages $PROJ
caput ${DETPV}:HDF:NumCapture $PROJ
caput ${DETPV}:HDF:AutoSave 1
caput ${DETPV}:HDF:FileWriteMode 2
caput ${DETPV}:HDF:Capture 1

# Enable the detector trigger and lock the zero angle of A6 in the IOC
echo "Enabling PPS trigger"
caput SR08ID01BCT01:Pps_Det_Start 1
sleep 1s

if caget ${DETPV}:HDF:Capture_RBV | grep -q Done ; then
    echo "Error. HDF did not enter capturing state"
    exit 1
fi
waitCA ${SHIFTMOT}.DMOV 1
echo "Make sure to manually open the imbl safety shutter or you won't get radiation"
echo "READY for scan"

if (( $SHIFTPROJ != 0 )) ; then
    IMAGECNT="${DETPV}:CAM:NumImagesCounter_RBV"
    HDFCAPTR="${DETPV}:HDF:Capture_RBV"
    camonitor -p 99 -n -S $IMAGECNT $HDFCAPTR | sed -u "s/  \+/ /g" |
    while read pv date time val ; do
      case "$pv" in
        ${IMAGECNT} )
          if (( $val >= $SHIFTPROJ )) ; then
            caput "${SHIFTMOT}" $INSHIFT
            echo "Just sent shifter command"
	          killall camonitor
          fi
        ;;
        ${HDFCAPTR} )
          if caget -t SR08ID01E2:HDF:NumCaptured_RBV | grep -q $PROJ ; then
            killall camonitor
            echo "Numcaptured equals max projections"
          fi
        ;;
        * )  ;;
      esac
    done
fi

echo "waitCA for capture == done"
waitCA ${DETPV}:HDF:Capture_RBV Done
echo "RBV capture is done, shifting"
caput ${SHIFTMOT} $INORIGN 
echo "Disabling PPS trigger"
caput SR08ID01BCT01:Pps_Det_Start 0

sleep 1.0s
latestLog="$(ls -t /mnt/bctpro.data/*.txt | head -n 1)"
cp "$latestLog" "${LOCAQPATH}/"
hdfname=`caget -St SR08ID01E2:HDF:FileName_RBV`
cp "$latestLog" "${LOCAQPATH}/${hdfname}-PPSstream.txt"
echo "DONE"
