( caget SR08ID01ZEB01:PC_TIME SR08ID01ZEB01:PC_ENC1  | sed  "s: 0 ::g" | sed "s:00*0::g" ; echo -e "\n\n" ) >> zebra.txt
