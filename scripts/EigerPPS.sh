#!/bin/bash

USAGE="$0 <data name> <projections>"

#DATALOC="/mnt/asci.data/imbl/input"
DATALOC="/mnt/bctpro.data"
#DATADET="/mnt/asci.data/imbl/input"
DATADET="/data"
DETPV="SR08ID01E2"

if [ -z "$1" ] ; then
    echo "Error. No data name provided."
    echo "Usage: $USAGE"
    exit 1
fi
DATA="$1"

if [ -z "$2" ] ; then
    echo "Error. No number of projections provided."
    echo "Usage: $USAGE"
    exit 1
fi
PROJ="$2"

LOCAQPATH="${DATALOC}/${DATA}"
echo "$LOCAQPATH"
mkdir -p "$LOCAQPATH"
chmod 777 "$LOCAQPATH"
if [ ! -e "$LOCAQPATH" ] ; then
    echo "Failed to create acquisition path: '$LOCAQPATH'"
    exit 1
fi


caput ${DETPV}:CAM:Acquire 0
sleep 1s

DETAQPATH="${DATADET}/${DATA}"
caput -S  ${DETPV}:HDF:FilePath "$DETAQPATH"
sleep 1s
if caget ${DETPV}:HDF:FilePathExists_RBV | grep -q "No" ; then
    echo "Error. Detector can't see data path '$DETAQPATH'"
    exit 1
fi

echo 3
DETAQPATH="${DATADET}/${DATA}"
caput -S  ${DETPV}:HDF:FilePath "$DETAQPATH"
sleep 1s
if caget ${DETPV}:HDF:FilePathExists_RBV | grep -q "No" ; then
    echo "Error. Detector can't see data path '$DETAQPATH'"
    exit 1
fi


if [ ! -z "$3" ] ; then
  caput -S ${DETPV}:HDF:FileName "$3"
fi
caput ${DETPV}:CAM:ArrayCounter 0
caput ${DETPV}:CAM:NumImages $PROJ
caput ${DETPV}:HDF:NumCapture $PROJ
caput ${DETPV}:HDF:AutoSave 1
caput ${DETPV}:HDF:FileWriteMode 2
caput ${DETPV}:HDF:Capture 1

sleep 5s
caget ${DETPV}:HDF:Capture_RBV
if caget ${DETPV}:HDF:Capture_RBV | grep -q Done ; then
    echo "Error. HDF did not enter capturing state"
    exit 1
fi

while ! caget ${DETPV}:HDF:Capture_RBV | grep -q Done ; do
   sleep 1s
done
sleep 2s
latestLog="$(ls -t ${DATALOC}/*.txt | head -n 1)"
cp "$latestLog" "${LOCAQPATH}/"



