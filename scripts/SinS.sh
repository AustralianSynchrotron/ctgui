#!/bin/bash

MOTY="SR08ID01TBL32:Y"
MOTZ="SR08ID01TBL32:Z"

ZORG="17"
ZSHF="22"
YORG="67"
YSHF="69"

if (( $CURRENTSCAN == 1 )) && [ -z "$1" ] ; then
  caput $MOTY $YSHF  
  caput $MOTZ $ZSHF  
else 
  caput $MOTY $YORG  
  caput $MOTZ $ZORG  
fi
sleep 1s
cawait ${MOTY}.DMOV 1
cawait ${MOTZ}.DMOV 1

