#!/bin/bash

if ! [ -x "$(command -v cabextract)" ]; then
    echo "cabextract not found."
    exit 1;
fi

if ! [ -x "$(command -v unshield)" ]; then
    echo "unshield not found."
    exit 1;
fi

mkdir work_tmp;
cd work_tmp;
wget http://www.olympusamerica.com/files/oima_cckb/DigitalWavePlayerUpdate214.zip;
unzip DigitalWavePlayerUpdate214.zip;
cd Vista+AMD;
cabextract DWPUP5EN.exe;
cd Disk1;
unshield x data1.cab
cp "_v___O_____DLL_(____________)/san_dec.dll" ../../../;
cd ../../../;
rm -rf work_tmp;
