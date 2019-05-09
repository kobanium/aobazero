#!/bin/bash

if [ $# -ne 2 ]
then
    echo Usage: `basename $0` positions weight
    exit
fi

fposi=$1
if [ ! -f $fposi ]
then
    echo cannot find $fposi
    exit
fi

fwght=$2
if [ ! -f $fwght ]
then
    echo cannot find $fwght
    exit
fi

while read line
do
    echo $line
    echo dbgout
done < <(xz -dc $fposi) > >(bin/aobaz -q -w <(xz -dc $fwght))
