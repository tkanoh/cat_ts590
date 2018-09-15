#!/bin/sh

CALLSIGN="my callsign"

str_1="${CALLSIGN}"
str_2="\\ 5nn tu"
str_3="\\ ${CALLSIGN} 5nn \\"
str_4="\\ ${CALLSIGN} 5nn tu"
str_5="\\ de ${CALLSIGN} ur 5nn \\"
str_6="\\ de ${CALLSIGN} ${CALLSIGN} ur 5nn \\"
str_7="qsl ur 5nn tu"
str_8="73 > e e"
str_9="tu"

if [ "$1" = "-h" ] ; then
	clear
	n=1
	while [ ! $n = 10 ] ; do
		eval str='$'str_$n
		echo "$n: $str"
		n=`expr $n + 1`
	done
	exit 0
fi

while
do
	read KEY
	n=1
	while [ ! $n = 10 ] ; do
		eval str='$'str_$n
		[ "$KEY" = "$n" ] && sudo ts590 -k "$str"
		n=`expr $n + 1`
	done
	[ "$KEY" = ""  ] && sudo ts590 -q
	[ "$KEY" = "q" ] && exit
done
