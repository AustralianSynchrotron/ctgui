#!/bin/bash
USAGE="$0 <new topdir folder name>"
if [ -z "$1" ] ; then
    echo "Error. No folder name provided. This should be the top dir folder you want to save in (root is /data/)"
    echo "Usage: $USAGE"
    exit 1
fi
NEWFOLDER="$1"

# grep TOPDIR=".*" *.sh
OLDFOLDER="$(grep TOPDIR=".*" EigerPPS_Shifter.sh | sed 's|TOPDIR="||' | sed 's|"||')"
echo "EigerPPS_Shifter oldfolder is ${OLDFOLDER} and newfolder is ${NEWFOLDER}"
sed -i "s|$OLDFOLDER|$NEWFOLDER|" EigerPPS_Shifter.sh
OLDFOLDER="$(grep TOPDIR=".*" EigerPPS_NoShift.sh | sed 's|TOPDIR="||' | sed 's|"||')"
echo "EigerPPS_NoShift oldfolder is ${OLDFOLDER} and newfolder is ${NEWFOLDER}"
sed -i "s|$OLDFOLDER|$NEWFOLDER|" EigerPPS_NoShift.sh
OLDFOLDER="$(grep TOPDIR=".*" StaticBGs.sh | sed 's|TOPDIR="||' | sed 's|"||')"
echo "StaticBGs oldfolder is ${OLDFOLDER} and newfolder is ${NEWFOLDER}"
sed -i "s|$OLDFOLDER|$NEWFOLDER|" StaticBGs.sh
OLDFOLDER="$(grep TOPDIR=".*" ShiftedBGs.sh | sed 's|TOPDIR="||' | sed 's|"||')"
echo "ShiftedBGs oldfolder is ${OLDFOLDER} and newfolder is ${NEWFOLDER}"
sed -i "s|$OLDFOLDER|$NEWFOLDER|" ShiftedBGs.sh
echo "done"
