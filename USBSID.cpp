/*
 * USBSID-Pico is a RPi Pico (RP2040/RP2350) based board for interfacing one
 * or two MOS SID chips and/or hardware SID emulators over (WEB)USB with your
 * computer, phone or ASID supporting player.
 *
 * USBSID.cpp
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

#pragma GCC push_options
#pragma GCC optimize ("O0")

#include "USBSID.h"


using namespace USBSID_NS;
using namespace std;

extern "C" {

/* USBSID */

USBSID_Class::USBSID_Class() :
  us_Initialised(false)
{
#ifdef DEBUG_USBSID_MEMORY
  us_DebugMemory = true;
#endif
  USBDBG(stdout, "[USBSID] Driver init start\n");
  result = (uint8_t*)malloc((sizeof(uint8_t)) * (LEN_IN_BUFFER));
  temp_buffer = (uint8_t*)malloc((sizeof(uint8_t)) * (LEN_TMP_BUFFER));
  write_buffer = (uint8_t*)malloc((sizeof(uint8_t)) * (LEN_OUT_BUFFER));

  /* Init ringbuffer */
  exit_thread = 0;
  ringbuffer.ring_read = 0;
  ringbuffer.ring_write = 0;
  memset(sid_memory, 0, (sizeof(sid_memory) / sizeof(sid_memory[0])));

  us_Initialised = true;
}

USBSID_Class::~USBSID_Class()
{
  USBDBG(stdout, "[USBSID] Driver de-init start\n");
  free(write_buffer);
  free(temp_buffer);
  free(result);
  write_buffer = NULL;
  temp_buffer = NULL;
  result = NULL;
  if (USBSID_Close() == 0) us_Initialised = false;
}

int USBSID_Class::USBSID_Init(bool start_threaded)
{
  USBDBG(stdout, "[USBSID] Setup start\n");
  /* Init USB */
  rc = LIBUSB_Setup(start_threaded);
  if (rc >= 0) {
    /* Init thread */
    if (threaded) {
      pthread_create(&this->ptid, NULL, &this->usbsid_thread, NULL);
    }
    return rc;
  } else {
    USBDBG(stdout, "[USBSID] Not found\n");
    return -1;
  }
}

int USBSID_Class::USBSID_Close(void)
{
  int e;
  if (rc >= 0) e = LIBUSB_Exit();
  #ifdef __cpp_lib_format
    static_assert(rc != -1, std::format("Expected rc == -1, got {}", rc));
    static_assert(e != 0, std::format("Expected e == 0, got {}", e));
    static_assert(devh != NULL, std::format("Expected dev == NULL, got {}", devh));
  #else
    if(rc != -1) std::cout << "Expected rc == -1, got" << rc << std::endl;
    if(e != 0) std::cout <<  "Expected e == 0, got" << e << std::endl;
    if(devh != NULL) std::cout << "Expected dev == NULL, got" << devh << std::endl;
  #endif
  USBDBG(stdout, "[USBSID] De-init finished\n");
  return 0;
}

void USBSID_Class::USBSID_Pause(void)
{
  USBDBG(stdout, "[USBSID] Pause\r\n");
  unsigned char buff[3] = {0x2, 0x0, 0x0};
  USBSID_SingleWrite(buff, 3);
}

void USBSID_Class::USBSID_Reset(void)
{
  USBDBG(stdout, "[USBSID] Reset\r\n");
  unsigned char buff[3] = {0x3, 0x0, 0x0};
  USBSID_SingleWrite(buff, 3);
}

void USBSID_Class::USBSID_SetClockRate(long clockrate_cycles)
{
  for (uint8_t i = 0; i < 4; i++) {
    if (clockSpeed[i] == clockrate_cycles) {
      cycles_per_sec = clockrate_cycles;
      USBDBG(stdout, "[USBSID] Set clockspeed to: [i]%d ~ [r]%ld\n", (int)clockSpeed[i], cycles_per_sec);
      uint8_t configbuff[5] = {0x50, i, 0, 0, 0};
      USBSID_SingleWrite(configbuff, 5);
      return;
    }
  }
}


/* SYNCHRONOUS */

void USBSID_Class::USBSID_SingleWrite(unsigned char *buff, size_t len)
{
  int actual_length = 0;
  if (libusb_bulk_transfer(devh, EP_OUT_ADDR, buff, len, &actual_length, 0) < 0) {
    USBERR(stderr, "[USBSID] Error while sending synchronous write buffer of length %d\n", actual_length);
  }
}


/* ASYNCHRONOUS */

void USBSID_Class::USBSID_Write(unsigned char *buff, size_t len)
{
  write_completed = 0;
  memcpy(out_buffer, buff, len);
  libusb_submit_transfer(transfer_out);
  libusb_handle_events_completed(ctx, &write_completed);
}

void USBSID_Class::USBSID_Write(uint16_t reg, uint8_t val, uint8_t cycles)
{
  write_completed = 0;
  write_buffer[0] = 0x0;
  write_buffer[1] = (reg & 0xFF);
  write_buffer[2] = val;
  USBSID_Write(write_buffer, 3);
}

unsigned char USBSID_Class::USBSID_Read(unsigned char *writebuff, unsigned char *buff)
{
  if (threaded == 0) {  /* Reading not supported with threaded writes */
    read_completed = 0;
    memcpy(out_buffer, writebuff, 3);
    libusb_submit_transfer(transfer_out);
    libusb_handle_events_completed(ctx, NULL);
    libusb_submit_transfer(transfer_in);
    libusb_handle_events_completed(ctx, &read_completed);
    return *result;
  }
  return 0xFF;
}


/* LIBUSB */

int USBSID_Class::LIBUSB_Setup(bool start_threaded)
{
  rc = read_completed = write_completed = -1;
  threaded = start_threaded;

  /* Line encoding ~ baud rate is ignored by TinyUSB */
  unsigned char encoding[] = { 0x40, 0x54, 0x89, 0x00, 0x00, 0x00, 0x08 };

  /* Initialize libusb */
  rc = libusb_init(&ctx);
  // rc = libusb_init_context(&ctx, /*options=NULL, /*num_options=*/0);  // NOTE: REQUIRES LIBUSB 1.0.27!!
  if (rc != 0) {
    USBERR(stderr, "[USBSID] Error initializing libusb: %d %s: %s\r\n", rc, libusb_error_name(rc), libusb_strerror(rc));
    goto out;
  }

  /* Set debugging output to min/max (4) level */
  libusb_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, 0);

  /* Look for a specific device and open it. */
  devh = libusb_open_device_with_vid_pid(ctx, VENDOR_ID, PRODUCT_ID);
  if (!devh) {
    USBERR(stderr, "[USBSID] Error opening USB device with VID & PID: %d %s: %s\r\n", rc, libusb_error_name(rc), libusb_strerror(rc));
    rc = -1;
    goto out;
  }

  /* As we are dealing with a CDC-ACM device, it's highly probable that
   * Linux already attached the cdc-acm driver to this device.
   * We need to detach the drivers from all the USB interfaces. The CDC-ACM
   * Class defines two interfaces: the Control interface and the
   * Data interface.
   */
  for (int if_num = 0; if_num < 2; if_num++) {
    if (libusb_kernel_driver_active(devh, if_num)) {
      libusb_detach_kernel_driver(devh, if_num);
    }
    rc = libusb_claim_interface(devh, if_num);
    if (rc < 0) {
      USBERR(stderr, "[USBSID] Error claiming interface: %d, %s: %s\r\n", rc, libusb_error_name(rc), libusb_strerror(rc));
      rc = -1;
      goto out;
    }
  }

  /* Start configuring the device:
   * set line state */
  rc = libusb_control_transfer(devh, 0x21, 0x22, ACM_CTRL_DTR | ACM_CTRL_RTS, 0, NULL, 0, 0);
  if ( rc < 0 /* rc != 0 && rc != 7 */) {
    USBERR(stderr, "[USBSID] Error configuring line state during control transfer: %d, %s: %s\r\n", rc, libusb_error_name(rc), libusb_strerror(rc));
    rc = -1;
    goto out;
  }

  /* set line encoding here */
  rc = libusb_control_transfer(devh, 0x21, 0x20, 0, 0, encoding, sizeof(encoding), 0);
  if ( rc < 0 /* rc != 0 && rc != 7 */) {
    USBERR(stderr, "[USBSID] Error configuring line encoding during control transfer: %d, %s: %s\r\n", rc, libusb_error_name(rc), libusb_strerror(rc));
    rc = -1;
    goto out;
  }

  out_buffer = libusb_dev_mem_alloc(devh, LEN_OUT_BUFFER);
  if (out_buffer == NULL) {
    USBDBG(stdout, "[USBSID] libusb_dev_mem_alloc failed on out_buffer, allocating with malloc\r\n");
    /* TODO: Maybe change to vector array? https://stackoverflow.com/a/24575552 */
    out_buffer = (uint8_t*)malloc((sizeof(uint8_t)) * LEN_OUT_BUFFER);
  } else {
    out_buffer_dma = true;
  }
  USBDBG(stdout, "[USBSID] Alloc out_buffer complete\r\n");
  transfer_out = libusb_alloc_transfer(0);
  USBDBG(stdout, "[USBSID] Alloc transfer_out complete\r\n");
  libusb_fill_bulk_transfer(transfer_out, devh, EP_OUT_ADDR, out_buffer, LEN_OUT_BUFFER, usb_out, &write_completed, 0);
  USBDBG(stdout, "[USBSID] libusb_fill_bulk_transfer transfer_out complete\r\n");

  in_buffer = libusb_dev_mem_alloc(devh, LEN_IN_BUFFER);
  if (in_buffer == NULL) {
    USBDBG(stdout, "[USBSID] libusb_dev_mem_alloc failed on in_buffer, allocating with malloc\r\n");
    /* TODO: Maybe change to vector array? https://stackoverflow.com/a/24575552 */
    in_buffer = (uint8_t*)malloc((sizeof(uint8_t)) * LEN_IN_BUFFER);
  } else {
    in_buffer_dma = true;
  }
  USBDBG(stdout, "[USBSID] Alloc in_buffer complete\r\n");
  transfer_in = libusb_alloc_transfer(0);
  USBDBG(stdout, "[USBSID] Alloc transfer_in complete\r\n");
  libusb_fill_bulk_transfer(transfer_in, devh, EP_IN_ADDR, in_buffer, LEN_IN_BUFFER, usb_in, &read_completed, 0);
  USBDBG(stdout, "[USBSID] libusb_fill_bulk_transfer transfer_in complete\r\n");

  if (rc < 0) {
    USBERR(stderr, "[USBSID] Error, could not open SID device\r\n");
    goto out;
  }

  return rc;
out:
  LIBUSB_Exit();
  return rc;
}

int USBSID_Class::LIBUSB_Exit(void)
{
  /* usbSIDPause(); */

  if (threaded && USBSID_IsRunning()) {
    USBSID_StopThread();
    USBDBG(stdout, "[USBSID] Set thread exit = 1\r\n");
    pthread_join(ptid, NULL);
    USBDBG(stdout, "[USBSID] Thread attached\r\n");
  }

  if (transfer_out != NULL) {
    rc = libusb_cancel_transfer(transfer_out);
    if (rc < 0 && rc != -5) {
      USBERR(stderr, "[USBSID] Error, failed to cancel transfer %d - %s: %s\n", rc, libusb_error_name(rc), libusb_strerror(rc));
    }
  }

  if (transfer_in != NULL) {
    rc = libusb_cancel_transfer(transfer_in);
    if (rc < 0 && rc != -5) {
      USBERR(stderr, "[USBSID] Error, failed to cancel transfer %d - %s: %s\n", rc, libusb_error_name(rc), libusb_strerror(rc));
    }
  }

  if (in_buffer_dma) {
    rc = libusb_dev_mem_free(devh, in_buffer, LEN_IN_BUFFER);
    if (rc < 0) {
      USBERR(stderr, "[USBSID] Error, failed to free in_buffer DMA memory: %d, %s: %s\n", rc, libusb_error_name(rc), libusb_strerror(rc));
    }
  } else {
    free(in_buffer);
    in_buffer = NULL;
  }
  if (out_buffer_dma) {
    rc = libusb_dev_mem_free(devh, out_buffer, LEN_OUT_BUFFER);
    if (rc < 0) {
      USBERR(stderr, "[USBSID] Error, failed to free out_buffer DMA memory: %d, %s: %s\n", rc, libusb_error_name(rc), libusb_strerror(rc));
    }
  } else {
    free(out_buffer);
    out_buffer = NULL;
  }

  if (devh) {
    for (int if_num = 0; if_num < 2; if_num++) {
      if (libusb_kernel_driver_active(devh, if_num)) {
        rc = libusb_detach_kernel_driver(devh, if_num);
        USBERR(stderr, "[USBSID] Error, in libusb_detach_kernel_driver: %d, %s: %s\n", rc, libusb_error_name(rc), libusb_strerror(rc));
      }
      libusb_release_interface(devh, if_num);
    }
    libusb_close(devh);
  }

  if (ctx) {
    libusb_exit(ctx);
  }

  rc = -1;
  devh = NULL;
  USBDBG(stdout, "[USBSID] Closed USB device\r\n");
  return 0;
}

void LIBUSB_CALL USBSID_Class::usb_out(struct libusb_transfer *transfer)
{
  write_completed = (*(int *)transfer->user_data);

  if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
    rc = transfer->status;
    if (rc != LIBUSB_TRANSFER_CANCELLED) {
      USBERR(stderr, "[USBSID] Warning: transfer out interrupted with status %d, %s: %s\r", rc, libusb_error_name(rc), libusb_strerror(rc));
    }
    libusb_free_transfer(transfer);
    return;
  }

  if (transfer->actual_length != LEN_OUT_BUFFER) {
    USBERR(stderr, "[USBSID] Sent data length %d is different from the defined buffer length: %d or actual length %d\r", transfer->length, LEN_OUT_BUFFER, transfer->actual_length);
  }

  write_completed = 1;
  if (threaded) libusb_submit_transfer(transfer_out);  /* Resubmit queue when finished */
}

void LIBUSB_CALL USBSID_Class::usb_in(struct libusb_transfer *transfer)
{
  read_completed = (*(int *)transfer->user_data);

  if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
    rc = transfer_in->status;
    if (rc != LIBUSB_TRANSFER_CANCELLED) {
      USBERR(stderr, "[USBSID] Warning: transfer in interrupted with status '%s'\r", libusb_error_name(rc));
    }
    libusb_free_transfer(transfer);
    return;
  }

  memcpy(result, in_buffer, 1);
  read_completed = 1;
}


/* THREADING */

void* USBSID_Class::USBSID_Thread(void)  /* TODO: Unfinished */
{ /* Only starts when threaded == true */
  USBDBG(stdout, "[USBSID] Thread started\r\n");
  pthread_detach(pthread_self());
  USBDBG(stdout, "[USBSID] Thread detached\r\n");
// printf("[USBSID] us_DebugMemory %d\r\n", us_DebugMemory);
  // while(this->exit_thread == 0) {
  // kut = true;
  while(!exit_thread) {
    while (ringbuffer.ring_read != ringbuffer.ring_write) {
      if (!kut) {
        USBSID_RingPop();
      } else {
        USBSID_FillMemory();
        /* USBSID_DebugPrint(); */
      }
    }
  }
  USBDBG(stdout, "[USBSID] Thread finished\r\n");
  pthread_exit(NULL);

  return NULL;
}

void USBSID_Class::USBSID_StopThread(void)
{
  exit_thread = 1;
}

int USBSID_Class::USBSID_IsRunning(void)
{
  return !exit_thread;
}


/* RINGBUFFER FOR THREADED WRITES */

void USBSID_Class::USBSID_RingPush(uint16_t reg, uint8_t val, uint8_t cycles)
{
  (void)cycles;  /* Not yet used */

  if (threaded) {
    ringbuffer.ringpush = ((reg & 0xFF) << 8) | val;
    ringbuffer.ring_buffer[ringbuffer.ring_write++] = ringbuffer.ringpush;
  } else {
    /* Fallback if incorrect function is used */
    USBSID_Write(reg, val, cycles);

    /* write_completed = 0;
    write_buffer[0] = 0x0;
    write_buffer[1] = (reg & 0xFF);
    write_buffer[2] = val;
    USBSID_Write(write_buffer, 3); */
  }
}

uint8_t * USBSID_Class::USBSID_RingPop(void)
{
  write_completed = 0;
  ringbuffer.ringpop = ringbuffer.ring_buffer[ringbuffer.ring_read++];

  /* Ex: 0xD418 */
  out_buffer[0] = 0x0;
  out_buffer[1] = (uint8_t)(ringbuffer.ringpop >> 8);
  out_buffer[2] = (uint8_t)(ringbuffer.ringpop & 0xFF);
  libusb_submit_transfer(transfer_out);
  libusb_handle_events_completed(ctx, &write_completed);

  if (threaded) {  /* return value for FillMemory and DebugPrint */
    memcpy(temp_buffer, out_buffer, 3);
    return temp_buffer;
  }
  return 0x0;
}


/* BUS */

uint8_t USBSID_Class::USBSID_Address(uint16_t addr)
{ /* Unused for libsidplayfp */
  enum {
    SIDUMASK = 0xFF00,
    SIDLMASK = 0xFF,
    SID1ADDR = 0xD400,
    SID1MASK = 0x1F,
    SID2ADDR = 0xD420,
    SID2MASK = 0x3F,
    SID3ADDR = 0xD440,
    SID3MASK = 0x5F,
    SID4ADDR = 0xD460,
    SID4MASK = 0x7F,
  };
  /* Set address for SID no# */
  /* D500, DE00 or DF00 is the second sid in SIDTYPE1, 3 & 4 */
  /* D500, DE00 or DF00 is the third sid in all other SIDTYPE */
  static uint8_t a;
  switch (addr) {
    case 0xD400 ... 0xD499:
      a = (addr & SIDLMASK); /* $D400 -> $D479 1, 2, 3 & 4 */
      break;
    case 0xD500 ... 0xD599:
    case 0xDE00 ... 0xDF99:
      a = ((SID3ADDR | (addr & SID2MASK)) & SIDLMASK);
      break;
    default:
      a = (addr & SIDLMASK);
      break;
  }
  return a;
}


/* MEMORY */

void USBSID_Class::USBSID_FillMemory(void)
{ /* Unused for libsidplayfp */
  uint8_t *p = USBSID_RingPop();
  uint16_t a_ = ((0xD4 << 8) | p[1]);
  uint8_t a = USBSID_Address(a_);
  uint8_t b = p[2];
  uint8_t c = p[0];
  sid_memory[a] = b;
  sid_memory_changed[a] = 1;
  printf("[W]$%04X$%02X\n", a, b);
  /* printf("[%02X|%02X]$%02X:%02X\n", ringbuffer.ring_write, ringbuffer.ring_read, a, b); */
  /* printf("[%02X|%02X]$%02X$%04X:%02X|%d\n", ringbuffer.ring_write, ringbuffer.ring_read, sid_address(a), a, b, c); */
}

void USBSID_Class::USBSID_DebugPrint(void)
{ /* Unused for libsidplayfp */
  printf("[SID1]\
  [V1]$%02x$%02x$%02x$%02x$%02x$%02x$%02x\
  [V2]$%02x$%02x$%02x$%02x$%02x$%02x$%02x\
  [V3]$%02x$%02x$%02x$%02x$%02x$%02x$%02x\
  [F]$%02x$%02x$%02x$%02x\
  [M]$%02x$%02x$%02x$%02x\n",
    /* Voice 1 */
    sid_memory[0x00],sid_memory[0x01],
    sid_memory[0x02],sid_memory[0x03],
    sid_memory[0x04],sid_memory[0x05],
    sid_memory[0x06],
    /* Voice 2 */
    sid_memory[0x07],sid_memory[0x08],
    sid_memory[0x09],sid_memory[0x0A],
    sid_memory[0x0B],sid_memory[0x0C],
    sid_memory[0x0D],
    /* Voice 3 */
    sid_memory[0x0E],sid_memory[0x0F],
    sid_memory[0x10],sid_memory[0x11],
    sid_memory[0x12],sid_memory[0x13],
    sid_memory[0x14]
    /* Filter */,
    sid_memory[0x15],sid_memory[0x16],
    sid_memory[0x17],sid_memory[0x18],
    /* Misc */
    sid_memory[0x19],sid_memory[0x1A],
    sid_memory[0x1B],sid_memory[0x1C]
  );
}


/* TIMING AND CYCLES */

int64_t USBSID_Class::USBSID_CycleFromTimestamp(timestamp_t timestamp)
{
  auto nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(timestamp - m_StartTime);
  return (int64_t)(nsec.count() * m_InvCPUcycleDurationNanoSeconds);
}

int USBSID_Class::USBSID_WaitForCycle(unsigned int cycles)
{
    /* printf("%s: %d\n", __func__, cycles); */
    /* timestamp_t now = std::chrono::high_resolution_clock::now();
    double dur_ = cycles * m_InvCPUcycleDurationNanoSeconds;
    duration_t dur = (duration_t)(int64_t)dur_;
    auto target_time = m_NextTime + dur;
    auto target_delta = target_time - now;
    auto wait_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(target_delta * 1000);
    if (wait_nsec.count() > 0) {
        std::this_thread::sleep_for(wait_nsec);
    } */

    timestamp_t now = std::chrono::high_resolution_clock::now();
    double dur = cycles * m_CPUcycleDuration;
    duration_t duration = (duration_t)(int64_t)dur;
    auto target_time = m_NextTime + duration;
    auto target_delta = target_time - now;
    // auto wait_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(target_delta * 1000);
    auto wait_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(target_delta);
    auto wait_msec = std::chrono::duration_cast<std::chrono::milliseconds>(target_delta);
    if (wait_msec.count() > 0) {
        std::this_thread::sleep_for(wait_msec);
    }

    while (now < target_time) {
        now = std::chrono::high_resolution_clock::now();
    }
    m_CurrentTime       = now;
    m_NextTime          = target_time;

    /* ISSUE: returned cycles seem incorrect, does not affect playing */
    // int waited_cycles   = (int)(wait_nsec.count() * m_InvCPUcycleDurationNanoSeconds);
    // unsigned int waited_cycles  = (wait_msec.count() * (1.0 / m_CPUcycleDuration));
    // unsigned int waited_cycles  = (wait_msec.count() / m_CPUcycleDuration);
    int waited_cycles   = (wait_nsec.count() * m_InvCPUcycleDurationNanoSeconds);
    return waited_cycles;
}

} /* extern "C" */

#pragma GCC pop_options
