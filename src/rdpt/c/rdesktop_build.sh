#!/bin/bash
# $1 rdpt dir

if [ -n "$1" ];
then
	rm -rf Makefile.in rdesktop.c rdesktop.h rdp.c xwin.c cpkl.* rdpt_func.c rdpt_prot.h rdpt_c.c rdpt_c.h
	
	ln -s $1/cpkl/cpkl.* ./
	ln -s $1/rdpt_func.c ./
	ln -s $1/rdpt_prot.h ./
	ln -s $1/rdpt/c/rdpt_c.c ./
	ln -s $1/rdpt/c/rdpt_c.h ./
	ln -s $1/rdpt/c/rdesktop/*.* ./
	
	./bootstrap
	./configure --disable-credssp --disable-smartcard
	make -j4
else
	echo need rdpt dir
fi
