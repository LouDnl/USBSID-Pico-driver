# NOTICE

This directory holds the build files for the driver used in [Vice](https://github.com/VICE-Team/svn-mirror) (`vice/src/lib/libusbsiddrv`), which I update when needed.  
Only `Makefile.am` and this file live in this directory. The driver sources are the ones in the [src](https://github.com/LouDnl/USBSID-Pico-driver/tree/master/src) directory of the [USBSID-Pico driver](https://github.com/LouDnl/USBSID-Pico-driver), `README.md` and `LICENSE` are the ones in the repository root.

To fill this directory with the complete VICE driver, run from the repository root:
```shell
test/ci/sync_vice.sh
```
It copies `src/*.cpp`, `src/*.h`, `README.md` and `LICENSE` into this directory, the copies are ignored by git.  
Every release also has a ready to use archive `USBSID-Pico-driver-vice-<version>` with the same files.
