/*
 * USBSID-Pico is a RPi Pico (RP2040/RP2350) based board for interfacing one
 * or two MOS SID chips and/or hardware SID emulators over (WEB)USB with your
 * computer, phone or ASID supporting player.
 *
 * USBSIDInterface.h
 * This file is part of USBSID-Pico (https://github.com/LouDnl/USBSID-Pico-driver)
 * File author: LouD
 *
 * Copyright (c) 2024 LouD
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

  /* TODO: Incomplete */
  typedef void * USBSIDitf;
  USBSIDitf create_USBSID();
  int init_USBSID(USBSIDitf, bool set_async);
  void close_USBSID(USBSIDitf);
  void pause_USBSID(USBSIDitf);
  void reset_USBSID(USBSIDitf);
  void setclockrate_USBSID(USBSIDitf, long clockrate_cycles);
  void writesingle_USBSID(USBSIDitf, unsigned char *buff, size_t len);
  void writebuffer_USBSID(USBSIDitf, unsigned char *buff, size_t len);
  void write_USBSID(USBSIDitf, uint16_t reg, uint8_t val, uint8_t cycles);
  unsigned char read_USBSID(USBSIDitf, unsigned char *writebuff, unsigned char *buff);
  void ringpush_USBSID(USBSIDitf, uint16_t reg, uint8_t val, uint8_t cycles);
  int waitforcycle_USBSID(unsigned int cycles);

#ifdef __cplusplus
}
#endif
