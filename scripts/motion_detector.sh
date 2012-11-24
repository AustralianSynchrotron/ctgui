#!/bin/bash


SERIALMOTORPV="SR08ID01MCS08:MTR63B"
BGMOTORPV="SR08ID01MCS08:MTR63B"
THETAMOTORPV="SR08ID01MCS06:MTR61B"

SERIALLASTUPDATE=$(date +"%s")
BGLASTUPDATE=$(date +"%s")
THETALASTUPDATE=$(date +"%s")

LASTSERIAL=$(caget -t "${SERIALMOTORPV}.RBV")
LASTBG=$(caget -t "${BGMOTORPV}.RBV")
LASTTHETA=$(caget -t "${THETAMOTORPV}.RBV")

MAXIMUMRELAX="60" #sec
MAXIMUMTHETARELAX="60" #sec
MAXIMUMBGRELAX="900" #sec


KEEPGOING=1
while ((KEEPGOING)) ; do

  if ! caget -t "${THETAMOTORPV}" &> /dev/null || \
     ! caget -t ${SERIALMOTORPV} &> /dev/null || \
     ! caget -t ${BGMOTORPV} &> /dev/null 
   then
     KEEPGOING=0
     echo "NONCON"
     break;
   fi

  if caget -t "${THETAMOTORPV}.HLS" | grep -q 1 || \
     caget -t "${THETAMOTORPV}.LLS" | grep -q 1 || \
     caget -t "${SERIALMOTORPV}.HLS" | grep -q 1 || \
     caget -t "${SERIALMOTORPV}.LLS" | grep -q 1 || \
     caget -t "${BGMOTORPV}.HLS" | grep -q 1 || \
     caget -t "${BGMOTORPV}.LLS" | grep -q 1  
   then
     KEEPGOING=0
     echo "LIMIT"
     break
   fi

  NOWDATE=$(date +"%s")
  CURSERIAL=$(caget -t "${SERIALMOTORPV}.RBV")
  CURBG=$(caget -t "${BGMOTORPV}.RBV")
  CURTHETA=$(caget -t "${THETAMOTORPV}.RBV")

  if [ "$LASTSERIAL" != "$CURSERIAL" ] ; then
    SERIALLASTUPDATE="$NOWDATE"
    LASTSERIAL="$CURSERIAL"
    SERIALRELAXED=0
  else
    let "SERIALRELAXED = $NOWDATE - $SERIALLASTUPDATE"
  fi
  if [ "$SERIALRELAXED" -gt "$MAXIMUMBGRELAX" ] ; then
    KEEPGOING=0
    echo "SER RELAX"
    break
  fi



  if [ "$LASTBG" != "$CURBG" ] ; then
    BGLASTUPDATE="$NOWDATE"
    LASTBG="$CURBG"
    BGRELAXED=0
  else
    let "BGRELAXED = $NOWDATE - $BGLASTUPDATE"
  fi
  if [ "$BGRELAXED" -gt "$MAXIMUMBGRELAX" ] ; then
    KEEPGOING=0
    echo "BG RELAX"
    break
  fi

  if [ "$LASTTHETA" != "$CURTHETA" ] ; then
    THETALASTUPDATE="$NOWDATE"
    LASTTHETA="$CURTHETA"
    THETARELAXED=0
  else
    let "THETARELAXED = $NOWDATE - $THETALASTUPDATE"
  fi
  if [ "$THETARELAXED" -gt "$MAXIMUMTHETARELAX" ] ; then
    KEEPGOING=0
    echo "THETA RELAX"
    break
  fi

  SHORTESTRELAX=$(echo -e "$SERIALRELAXED\n$BGRELAXED\n$THETARELAXED" | sort -g | head -n 1)
  if [ "$SHORTESTRELAX" -gt "$MAXIMUMRELAX" ] ; then
    KEEPGOING=0
    echo "SHORT RELAX"
    break
  fi

  sleep 1s

done

echo EXIT

caput SR08ID01PSS01:FE_SHUTTER_CLOSE_CMD 1







