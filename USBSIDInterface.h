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
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

  /* USBSID */
  typedef void * USBSIDitf;
  USBSIDitf create_USBSID(void);
  int init_USBSID(USBSIDitf, bool start_threaded, bool with_cycles);
  void restartthread_USBSID(USBSIDitf, bool with_cycles);
  void close_USBSID(USBSIDitf);
  void pause_USBSID(USBSIDitf);
  void reset_USBSID(USBSIDitf);
  void setclockrate_USBSID(USBSIDitf, long clockrate_cycles);

  /* Synchronous */
  void writesingle_USBSID(USBSIDitf, unsigned char *buff, size_t len);

  /* Asynchronous */
  void writebuffer_USBSID(USBSIDitf, unsigned char *buff, size_t len);
  void write_USBSID(USBSIDitf, uint16_t reg, uint8_t val);
  void writecycled_USBSID(USBSIDitf, uint16_t reg, uint8_t val, uint16_t cycles);
  unsigned char read_USBSID(USBSIDitf, unsigned char *writebuff, unsigned char *buff);

  /* Ringbuffer */
  void ringpush_USBSID(USBSIDitf, uint16_t reg, uint8_t val);
  void ringpushcycled_USBSID(USBSIDitf, uint16_t reg, uint8_t val, uint16_t cycles);

  /* Timing */
  int_fast64_t waitforcycle_USBSID(USBSIDitf, uint_fast64_t cycles);

#ifdef __cplusplus
}
#endif
