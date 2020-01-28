#!/bin/bash
# $1 tap name
# $2 ip

#echo begin... > test.log
#echo $1 >> test.log
#echo $2 >> test.log

ifconfig $1 up
ifconfig $1 $2 netmask 255.255.255.0
