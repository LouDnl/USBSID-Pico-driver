# USBSID-Pico-driver
This repo contains the driver code for USBSID-Pico in C, Java and Javascript.

For more information about [USBSID-Pico](https://github.com/LouDnl/USBSID-Pico) visit the main [repo](https://github.com/LouDnl/USBSID-Pico).  

# C/C++ API
Usage information is available in [USBSID.h](src/USBSID.h).  
For native C applications refer to [USBSIDInterface.h](src/USBSIDInterface.h) which is a wrapper around the C++ functions.  

# Usage
This driver code (or an adapted/ rewritten variant of it) is used in:
- [Vice](https://github.com/VICE-Team/svn-mirror/tree/main/vice/src/lib/libusbsiddrv)
- [libsidplayfp](https://github.com/libsidplayfp/libsidplayfp/tree/master/src/builders/usbsid-builder)
- [JSidplay2](https://sourceforge.net/p/jsidplay2/git/ci/master/tree/lib/usbsid/)
- [Denise](https://github.com/LouDnl/denise_usbsid/tree/master/deps/USBSID-Pico)
- [Acid64 Pro](https://acid64.com/download)
- [Phosphor](https://github.com/sandlbn/Phosphor)
  * Uses an in Rust rewritten variant [USBSID-Pico-Rust-driver](https://github.com/sandlbn/USBSID-Pico-Rust-driver)
- [Deepsid](https://github.com/Chordian/deepsid/tree/master/js/handlers)
- [USBSID-Player](https://github.com/LouDnl/USBSID-Player)
- [SidBerry (fork)](https://github.com/LouDnl/SidBerry)
- [gt2ultra (fork)](https://github.com/LouDnl/GTUltra-USBSID/tree/master/src/driver)
- [RetroDebugger (fork)](https://github.com/LouDnl/RetroDebugger/tree/master/platform/Linux/src.Linux/usbsid)
- goattracker2 (fork)
- sidfactory2 (fork)
