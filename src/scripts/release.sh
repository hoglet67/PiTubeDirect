#!/bin/bash

# exit on error
set -e

NAME=PiTubeDirect_$(date +"%Y%m%d_%H%M")_$USER

for MODEL in rpi3 rpi2 rpi rpibplus rpizero 
do    
    DIR=releases/${NAME}
    mkdir -p ${DIR}
    ./clobber.sh
    ./configure_${MODEL}.sh
    make -B -j
    mv kernel*.img ${DIR}
done

cp config.txt ${DIR}/config.txt
cd releases
zip -qr ${NAME}.zip ${NAME}
cd ..

unzip -l releases/${NAME}.zip
 
