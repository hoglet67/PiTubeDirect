#!/bin/bash

# exit on error
set -e

NAME=PiTubeDirect_$(date +"%Y%m%d_%H%M")_$USER

DIR=releases/${NAME}
mkdir -p ${DIR}/debug

for MODEL in rpi3 rpi2 rpi rpibplus rpizero 
do    
    # compile normal kernel
    ./clobber.sh
    ./configure_${MODEL}.sh
    make -B -j
    mv kernel*.img ${DIR}
    # compile debug kernel
    ./clobber.sh
    ./configure_${MODEL}.sh -DDEBUG=1
    make -B -j
    mv kernel*.img ${DIR}/debug
done

cp -a firmware/* ${DIR}

cp config.txt ${DIR}/config.txt
cd releases/${NAME}
zip -qr ../${NAME}.zip .
cd ../..

unzip -l releases/${NAME}.zip
 
