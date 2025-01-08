/*
 * USBSID-Pico is a RPi Pico (RP2040/RP2350) based board for interfacing one
 * or two MOS SID chips and/or hardware SID emulators over (WEB)USB with your
 * computer, phone or ASID supporting player.
 *
 * USBSID.h
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

#ifndef _USBSID_H_
#define _USBSID_H_
#pragma GCC push_options
#pragma GCC optimize ("O0")
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"

#ifdef __cplusplus
  #include <cstdint>
  #include <cstdio>
  #include <cstdlib>
  #include <cstring>
  #include <chrono>
  #include <thread>
#else
  #include <stdint.h>
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include <pthread.h>
#endif

#include <libusb.h>


/* #define USBSID_DEBUG */
#ifdef USBSID_DEBUG
  #define USBDBG(...) fprintf(__VA_ARGS__)
  #define DEBUG_USBSID_MEMORY
#else
  #define USBDBG(...) ((void)0)
#endif
#define USBERR(...) fprintf(__VA_ARGS__)

namespace USBSID_NS
{
  /* pre-declaration for static functions */
  class USBSID_Class;

  /* LIBUSB/USBSID related */
  enum {
    VENDOR_ID      = 0xCAFE,
    PRODUCT_ID     = 0x4011,
    ACM_CTRL_DTR   = 0x01,
    ACM_CTRL_RTS   = 0x02,
    EP_OUT_ADDR    = 0x02,
    EP_IN_ADDR     = 0x82,
    LEN_IN_BUFFER  = 1,
    LEN_OUT_BUFFER = 3,
    LEN_CYC_BUFFER = 4,
    LEN_THR_BUFFER = 64,
    LEN_TMP_BUFFER = 4
  };

  /* Ringbuffer related */
  typedef struct {
    uint8_t ring_read;
    uint8_t ring_write;
    uint16_t ring_buffer[256] = {0};
    uint16_t ringpush = 0, ringpop = 0;
  } ring_buffer;
  static ring_buffer ringbuffer;

  /* Thread related */
  static int exit_thread;
  static int run_thread;

  /* Fake C64 Memory */
  static uint8_t sid_memory[0x20];
  static uint8_t sid_memory_changed[0x20];
  static uint16_t sid_memory_cycles[0x20];

  /* LIBUSB related */
  static struct libusb_device_handle *devh = NULL;
  static struct libusb_transfer *transfer_out = NULL;  /* OUT-going transfers (OUT from host PC to USB-device) */
  static struct libusb_transfer *transfer_in = NULL;  /* IN-coming transfers (IN to host PC from USB-device) */
  static libusb_context *ctx = NULL;
  static bool in_buffer_dma = false;
  static bool out_buffer_dma = false;

  static bool threaded = false;
  static bool withcycles = false;
  static int rc, read_completed, write_completed;

  /* USB buffer related */
  static uint8_t * in_buffer;     /* incoming libusb will reside in this buffer */
  static uint8_t * out_buffer;    /* outgoing libusb will reside in this buffer */
  static uint8_t * write_buffer;  /* non async data will be written from this buffer */
  static uint8_t * temp_buffer;   /* temp buffer for debug printing */
  static uint8_t * result;        /* variable where read data is copied into */
  static int len_out_buffer;      /* changable variable for out buffer size */

  enum clock_speeds
  {
      DEFAULT = 1000000,
      PAL     = 985248,
      NTSC    = 1022730,
      DREAN   = 1023440,
  };
  static const enum clock_speeds clockSpeed[] = { DEFAULT, PAL, NTSC, DREAN };
  static long cycles_per_sec = clockSpeed[0];  /* default @ 1000000 */

  class USBSID_Class {
    private:

      /* LIBUSB related */
      int LIBUSB_Setup(bool start_threaded, bool with_cycles);
      int LIBUSB_Exit(void);
      void LIBUSB_StopThread(void);
      void LIBUSB_StopTransfers(void);
      int LIBUSB_OpenDevice(void);
      void LIBUSB_CloseDevice(void);
      int LIBUSB_DetachKernelDriver(void);
      int LIBUSB_ConfigureDevice(void);
      void LIBUSB_InitOutBuffer(void);
      void LIBUSB_FreeOutBuffer(void);
      void LIBUSB_InitInBuffer(void);
      void LIBUSB_FreeInBuffer(void);
      static void LIBUSB_CALL usb_out(struct libusb_transfer *transfer);
      static void LIBUSB_CALL usb_in(struct libusb_transfer *transfer);

      /* Line encoding ~ baud rate is ignored by TinyUSB */
      unsigned char encoding[7] = { 0x40, 0x54, 0x89, 0x00, 0x00, 0x00, 0x08 };

      /* Threading related */
      void* USBSID_Thread(void);
      void USBSID_StopThread(void);
      int USBSID_IsRunning(void);
      pthread_t ptid;

      /* Ringbuffer related */
      void USBSID_RingPopCycled(void);  /* Threaded writer with cycles */
      void USBSID_RingPop(void);  /* Threaded writer */
      uint8_t * USBSID_RingPop(bool return_busvalue);  /* Threaded writer with return value */

      /* SID memory related ~ Unused at the moment */
      void USBSID_FlushMemory(void);
      void USBSID_FillMemory(void);
      void USBSID_DebugPrint(void);

    public:

      /* Constructor */
      USBSID_Class();
      /* Deconstructor */
      ~USBSID_Class();

      bool us_Initialised;
#ifdef DEBUG_USBSID_MEMORY
      bool us_DebugMemory = false;
#endif

      /* USBSID related */
      int USBSID_Init(bool start_threaded, bool with_cycles);
      int USBSID_Close(void);
      void USBSID_Pause(void);
      void USBSID_Reset(void);
      void USBSID_DisableSID(void);
      void USBSID_EnableSID(void);
      void USBSID_ClearBus(void);
      void USBSID_SetClockRate(long clockrate_cycles);

      /* Synchronous write */
      void USBSID_SingleWrite(unsigned char *buff, size_t len);  /* Write buffer of size_t ~ for config writing */

      /* Asynchronous Write */
      void USBSID_Write(unsigned char *buff, size_t len);  /* Write buffer of size_t len */
      void USBSID_Write(unsigned char *buff, size_t len, uint16_t cycles);  /* Wait n cycles, write buffer of size_t len */
      void USBSID_Write(uint8_t reg, uint8_t val);  /* Write register and value */
      void USBSID_Write(uint8_t reg, uint8_t val, uint16_t cycles);  /* Wait n cycles, write register and value */
      void USBSID_WriteCycled(uint8_t reg, uint8_t val, uint16_t cycles);  /* Write register and value, USBSID uses cycles for delay */

      /* Asynchronous Read */
      unsigned char USBSID_Read(unsigned char *writebuff, unsigned char *buff);  /* Write buffer and return read buffer */
      unsigned char USBSID_Read(unsigned char *writebuff, unsigned char *buff, uint16_t cycles);  /* Wait for n cycles and write buffer and return read buffer */

      /* Ringbuffer related */
      void USBSID_RingPush(uint8_t reg, uint8_t val);
      void USBSID_RingPushCycled(uint8_t reg, uint8_t val, uint16_t cycles);

      /* Thread related */
      void USBSID_RestartThread(bool with_cycles);
      static void *_USBSID_Thread(void *context)
      { /* Required for supplying private function to pthread_create */
        return ((USBSID_Class *)context)->USBSID_Thread();
      }

      /* Bus related */
      uint8_t USBSID_Address(uint16_t addr);

      /* Timing related */
      typedef std::chrono::high_resolution_clock::time_point timestamp_t;
      typedef std::chrono::nanoseconds  duration_t;
      typedef std::nano                 ratio_t;
      timestamp_t m_StartTime   = std::chrono::high_resolution_clock::now();
      timestamp_t m_NextTime    = std::chrono::high_resolution_clock::now();
      timestamp_t m_CurrentTime = std::chrono::high_resolution_clock::now();
      /* Cycle related */
      double m_CPUcycleDuration               = ratio_t::den / cycles_per_sec;
      double m_InvCPUcycleDurationNanoSeconds = 1.0 / (1000000000 / cycles_per_sec);
      /* Cycle based functions */
      uint_fast64_t USBSID_WaitForCycle(uint_fast16_t cycles);
      uint_fast64_t USBSID_CycleFromTimestamp(timestamp_t timestamp);

  };

} /* USBSIDDriver */


#pragma GCC diagnostic pop
#pragma GCC pop_options

#endif /* _USBSID_H_ */
