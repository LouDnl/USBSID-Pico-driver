# Compilation test
```shell
rm *.o ; g++ $(pkg-config --cflags --libs libusb-1.0) -c *.cpp $(pkg-config --cflags --libs libusb-1.0) -Wall
```
