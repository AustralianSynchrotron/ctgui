#!/bin/bash

# MUST BE FLOAT POINT, that is MUST HAVE DOT
ZDS="
0.0
1.1
2.2
3.3	
"

zcur=0;
foundz=""
for tryz in $ZDS ; do 
  if [ "$zcur" -eq "$CURRENTZ" ] ; then
    foundz="$tryz"
  fi
  zcur=$((zcur+1))
done

if echo $foundz | grep -q "^[0-9]*[.][0-9]*$" ; then
  echo "GOTO " $foundz
  caput $MOTORTOMOVE $foundz
  cawait ${MOTORTOMOVE}.DMOV 1
  exit 0
else
  exit 1
fi

