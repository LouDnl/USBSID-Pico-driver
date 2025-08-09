#!/usr/bin/env bash

# pkg-config amounts to => -I/usr/local/include/libusb-1.0 -L/usr/local/lib -lusb-1.0

mkdir -p build/cpp
mkdir -p build/c

rm -f build/*.o
rm -f build/*.a
rm -f build/cpp/*.o
rm -f build/c/*.o
rm -f build/cpp/*.a
rm -f build/c/*.a
rm -f build/*.so

g++ --verbose USBSID.cpp -c -fPIC $(pkg-config --cflags --libs libusb-1.0) -o build/cpp/USBSID.o
g++ --verbose USBSIDInterface.cpp -c -fPIC $(pkg-config --cflags --libs libusb-1.0) -o build/cpp/USBSIDInterface.o
ar rcs build/cpp/USBSIDPico.a build/cpp/*.o
g++ --verbose -shared -o build/USBSIDPico-cpp.so build/cpp/*.o $(pkg-config --libs libusb-1.0)

gcc --verbose USBSID.cpp -c -fPIC $(pkg-config --cflags --libs libusb-1.0) -o build/c/USBSID.o
gcc --verbose USBSIDInterface.cpp -c -fPIC $(pkg-config --cflags --libs libusb-1.0) -o build/c/USBSIDInterface.o
ar rcs build/c/USBSIDPico.a build/c/*.o
gcc --verbose -shared -o build/USBSIDPico-c.so build/c/*.o $(pkg-config --libs libusb-1.0)
