/*
 * USBSID-Pico is a RPi Pico (RP2040/RP2350) based board for interfacing one
 * or two MOS SID chips and/or hardware SID emulators over (WEB)USB with your
 * computer, phone or ASID supporting player.
 *
 * USBSIDInterface.cpp
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

#include "USBSIDInterface.h"
#include "USBSID.h"

using namespace USBSID_NS;

extern "C"
{
  USBSIDitf create_USBSID(void){
    return (USBSID_Class*) new USBSID_Class();
  };
  int init_USBSID(USBSIDitf p, bool start_threaded, bool with_cycles){
    if( p == NULL ) return -1;
    return ((USBSID_Class*) p)->USBSID_Init(start_threaded, with_cycles);
  };
  void restartthread_USBSID(USBSIDitf p, bool with_cycles){
    if( p == NULL ) return;
    return ((USBSID_Class*) p)->USBSID_RestartThread(with_cycles);
  }
  void close_USBSID(USBSIDitf p){
    if( p == NULL ) return;
    delete (USBSID_Class*) p;
  };
  void pause_USBSID(USBSIDitf p){
    if( p == NULL ) return;
    return ((USBSID_Class*)p)->USBSID_Pause();
  };
  void reset_USBSID(USBSIDitf p){
    if( p == NULL ) return;
    return ((USBSID_Class*)p)->USBSID_Reset();
  };
  void setclockrate_USBSID(USBSIDitf p, long clockrate_cycles){
    if( p == NULL ) return;
    return ((USBSID_Class*)p)->USBSID_SetClockRate(clockrate_cycles);
  };
  void writesingle_USBSID(USBSIDitf p, unsigned char *buff, size_t len){
    if( p == NULL ) return;
    return ((USBSID_Class*)p)->USBSID_SingleWrite(buff, len);
  };
  void writebuffer_USBSID(USBSIDitf p, unsigned char *buff, size_t len){
    if( p == NULL ) return;
    return ((USBSID_Class*)p)->USBSID_Write(buff, len);
  };
  void writecycled_USBSID(USBSIDitf p, uint16_t reg, uint8_t val, uint16_t cycles){
    if( p == NULL ) return;
    return ((USBSID_Class*)p)->USBSID_Write(reg, val, cycles);
  };
  void write_USBSID(USBSIDitf p, uint16_t reg, uint8_t val){
    if( p == NULL ) return;
    return ((USBSID_Class*)p)->USBSID_Write(reg, val);
  };
  unsigned char read_USBSID(USBSIDitf p, unsigned char *writebuff, unsigned char *buff){
    if( p == NULL ) return 0;
    return ((USBSID_Class*)p)->USBSID_Read(writebuff, buff);
  };
  void ringpush_USBSID(USBSIDitf p, uint16_t reg, uint8_t val){
    if( p == NULL ) return;
    return ((USBSID_Class*) p)->USBSID_RingPush(reg, val);
  };
  void ringpushcycled_USBSID(USBSIDitf p, uint16_t reg, uint8_t val, uint16_t cycles){
    if( p == NULL ) return;
    return ((USBSID_Class*) p)->USBSID_RingPushCycled(reg, val, cycles);
  };
  int_fast64_t waitforcycle_USBSID(USBSIDitf p, uint_fast64_t cycles){
    if( p == NULL ) return 0;
    return ((USBSID_Class*) p)->USBSID_WaitForCycle(cycles);
  };
}
