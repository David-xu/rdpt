#!/bin/bash
# $1 tap name
# $2 ip
# $3 default gw

#echo begin... > test.log
#echo $1 >> test.log
#echo $2 >> test.log
#echo $3 >> test.log

ifconfig $1 up
ifconfig $1 $2 netmask 255.255.255.0
#sleep 1
route add default gw $3