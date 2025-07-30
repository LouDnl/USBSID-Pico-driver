#!/usr/bin/env bash

# pkg-config amounts to => -I/usr/local/include/libusb-1.0 -L/usr/local/lib -lusb-1.0

mkdir -p build

rm build/*.o
rm build/*.a
rm build/*.so

g++ USBSID.cpp -c -fPIC $(pkg-config --cflags --libs libusb-1.0) -o build/USBSID.o
g++ USBSIDInterface.cpp -c -fPIC $(pkg-config --cflags --libs libusb-1.0) -o build/USBSIDInterface.o
ar rcs build/USBSIDPico.a build/*.o
g++ -shared -o build/USBSIDPico.so build/*.o $(pkg-config --libs libusb-1.0)
