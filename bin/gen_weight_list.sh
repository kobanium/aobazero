#!/bin/bash -eu
if [ $# -le 1 ]
then
    cmd=`basename $0`
    echo usage: $cmd command-checksum weights...
    exit
fi

cmdsum=$1
if [ ! -x $cmdsum ]
then
    echo bad executable file $cmdsum
    exit
fi

shift
for fname in $@
do
    if [ ! -f $fname ]
    then
	echo $fname is not a file. exit.
	exit
    fi
    bname=`basename $fname`
    sum=`xz -dc $fname | $cmdsum | cut -d' ' -f1`
    echo $bname $sum
done
