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

#ifdef DEBUG_USBSID_MEMORY
  memset(sid_memory, 0, (sizeof(sid_memory) / sizeof(sid_memory[0])));
  memset(sid_memory_changed, 0, (sizeof(sid_memory_changed) / sizeof(sid_memory_changed[0])));
#endif

  us_Initialised = true;
}

USBSID_Class::~USBSID_Class()
{
  USBDBG(stdout, "[USBSID] Driver de-init start\n");
  if (USBSID_Close() == 0) us_Initialised = false;
  if (write_buffer) free(write_buffer);
  if (thread_buffer) free(thread_buffer);
  if (result) free(result);
  write_buffer = NULL;
  result = NULL;
#ifdef DEBUG_USBSID_MEMORY
  if (temp_buffer) free(temp_buffer);
#endif
#ifdef DEBUG_USBSID_MEMORY
  temp_buffer = NULL;
#endif
}

int USBSID_Class::USBSID_Init(bool start_threaded, bool with_cycles)
{
  USBDBG(stdout, "[USBSID] Setup start\n");
  /* Init USB */
  rc = LIBUSB_Setup(start_threaded, with_cycles);
  flush_buffer = 0;
  if (rc >= 0) {
    /* Start thread on init */
    if (threaded) {
      /* Init ringbuffer */
      run_thread = 1;
      buffer_pos = 1;
      ringbuffer.ring_read = 0;
      ringbuffer.ring_write = 0;
      pthread_create(&this->ptid, NULL, &this->_USBSID_Thread, NULL);
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
  if (rc != -1) USBERR(stderr, "Expected rc == -1, received: %d\n", rc);
  if (e != 0) USBERR(stderr, "Expected e == 0, received: %d\n", e);
  if (devh != NULL) USBERR(stderr, "Expected dev == NULL, received: %p", (void*)&devh);
  USBDBG(stdout, "[USBSID] De-init finished\n");
  return 0;
}

void USBSID_Class::USBSID_Pause(void)
{
  USBDBG(stdout, "[USBSID] Pause\r\n");
  unsigned char buff[3] = {(COMMAND << 6 | PAUSE), 0x0, 0x0};
  USBSID_SingleWrite(buff, 3);
}

void USBSID_Class::USBSID_Reset(void)
{
  USBDBG(stdout, "[USBSID] Reset\r\n");
  unsigned char buff[3] = {(COMMAND << 6 | RESET_SID), 0x0, 0x0};
  USBSID_SingleWrite(buff, 3);
}

void USBSID_Class::USBSID_ResetAll(void)
{
  USBDBG(stdout, "[USBSID] Reset All\r\n");
  unsigned char buff[3] = {(COMMAND << 6 | RESET_SID), 0x1, 0x0};
  USBSID_SingleWrite(buff, 3);
}

void USBSID_Class::USBSID_Mute(void)
{
  USBDBG(stdout, "[USBSID] Mute\r\n");
  unsigned char buff[3] = {(COMMAND << 6 | MUTE), 0x0, 0x0};
  USBSID_SingleWrite(buff, 3);
}

void USBSID_Class::USBSID_UnMute(void)
{
  USBDBG(stdout, "[USBSID] UnMute\r\n");
  unsigned char buff[3] = {(COMMAND << 6 | UNMUTE), 0x0, 0x0};
  USBSID_SingleWrite(buff, 3);
}

void USBSID_Class::USBSID_DisableSID(void)
{
  USBDBG(stdout, "[USBSID] DisableSID\r\n");
  unsigned char buff[3] = {(COMMAND << 6 | DISABLE_SID), 0x0, 0x0};
  USBSID_SingleWrite(buff, 3);
}

void USBSID_Class::USBSID_EnableSID(void)
{
  USBDBG(stdout, "[USBSID] EnableSID\r\n");
  unsigned char buff[3] = {(COMMAND << 6 | ENABLE_SID), 0x0, 0x0};
  USBSID_SingleWrite(buff, 3);
}

void USBSID_Class::USBSID_ClearBus(void)
{
  USBDBG(stdout, "[USBSID] ClearBus\r\n");
  unsigned char buff[3] = {(COMMAND << 6 | CLEAR_BUS), 0x0, 0x0};
  USBSID_SingleWrite(buff, 3);
}

void USBSID_Class::USBSID_SetClockRate(long clockrate_cycles)
{
  for (uint8_t i = 0; i < 4; i++) {
    if (clockSpeed[i] == clockrate_cycles) {
      cycles_per_sec = clockrate_cycles;
      cycles_per_frame = refreshRate[i];
      m_CPUcycleDuration = ratio_t::den / cycles_per_sec;
      m_InvCPUcycleDurationNanoSeconds = 1.0 / (1000000000 / cycles_per_sec);
      USBDBG(stdout, "[USBSID] Set clockspeed to: [i]%d ~ [r]%ld\n", (int)clockSpeed[i], cycles_per_sec);
      uint8_t configbuff[6] = {(COMMAND << 6 | CONFIG), 0x50, i, 0, 0, 0};
      USBSID_SingleWrite(configbuff, 6);
      return;
    }
  }
}

long USBSID_Class::USBSID_GetClockRate(void)
{
  m_InvCPUcycleDurationNanoSeconds = 1.0 / (1000000000 / cycles_per_sec);
  m_CPUcycleDuration = ratio_t::den / cycles_per_sec;
  return cycles_per_sec;
}

long USBSID_Class::USBSID_GetRefreshRate(void)
{
  return cycles_per_frame;
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
  if (len == 3 && (threaded || withcycles)) {
    USBERR(stderr, "[USBSID] Function '%s' cannot be used with length %ld when threaded (%d) and/or withcycles (%d) are enabled\n", __func__, len, threaded, withcycles);
    return;
  }
  write_completed = 0;
  memcpy(out_buffer, buff, len);
  libusb_submit_transfer(transfer_out);
  libusb_handle_events_completed(ctx, NULL);
}

void USBSID_Class::USBSID_Write(uint8_t reg, uint8_t val)
{
  if (threaded) {
    USBERR(stderr, "[USBSID] Function '%s' cannot be used when threaded (%d) and/or withcycles (%d) are enabled\n", __func__, threaded, withcycles);
    return;
  }
  write_completed = 0;
  write_buffer[0] = 0x0;
  write_buffer[1] = (reg & 0xFF);
  write_buffer[2] = val;
  USBSID_Write(write_buffer, 3);
}

void USBSID_Class::USBSID_Write(unsigned char *buff, size_t len, uint16_t cycles)
{
  if (threaded || withcycles) {
    USBERR(stderr, "[USBSID] Function '%s' cannot be used when threaded (%d) and/or withcycles (%d) are enabled\n", __func__, threaded, withcycles);
    return;
  }
  USBSID_WaitForCycle(cycles);
  write_completed = 0;
  memcpy(out_buffer, buff, len);
  libusb_submit_transfer(transfer_out);
  libusb_handle_events_completed(ctx, NULL);
}

void USBSID_Class::USBSID_Write(uint8_t reg, uint8_t val, uint16_t cycles)
{
  if (threaded || withcycles) {
    USBERR(stderr, "[USBSID] Function '%s' cannot be used when threaded (%d) and/or withcycles (%d) are enabled\n", __func__, threaded, withcycles);
    return;
  }
  USBSID_WaitForCycle(cycles);
  write_completed = 0;
  write_buffer[0] = 0x0;
  write_buffer[1] = (reg & 0xFF);
  write_buffer[2] = val;
  USBSID_Write(write_buffer, 3);
}

void USBSID_Class::USBSID_WriteCycled(uint8_t reg, uint8_t val, uint16_t cycles)
{
  if (threaded) {
    USBERR(stderr, "[USBSID] Function '%s' cannot be used when threaded (%d) and/or withcycles (%d) are enabled\n", __func__, threaded, withcycles);
    return;
  }
  write_completed = 0;
  write_buffer[0] = (CYCLED_WRITE << 6);
  write_buffer[1] = (reg & 0xFF);
  write_buffer[2] = val;
  write_buffer[3] = (cycles >> 8);
  write_buffer[4] = (cycles & 0xFF);
  USBSID_Write(write_buffer, 5);
}

unsigned char USBSID_Class::USBSID_Read(unsigned char *writebuff, unsigned char *buff)
{
  if (threaded == 0 && withcycles == 0) {  /* Reading not supported with threaded writes */
    read_completed = write_completed = 0;
    writebuff[0] = (READ << 6);
    memcpy(out_buffer, writebuff, 3);
    libusb_submit_transfer(transfer_out);
    libusb_handle_events_completed(ctx, NULL);
    libusb_submit_transfer(transfer_in);
    libusb_handle_events_completed(ctx, &read_completed);
    return *result;
  }
  return 0xFF;
}

unsigned char USBSID_Class::USBSID_Read(unsigned char *writebuff, unsigned char *buff, uint16_t cycles)
{
  if (threaded == 0) {  /* Reading not supported with threaded writes */
    USBSID_WaitForCycle(cycles);
    read_completed = 0;
    writebuff[0] = (READ << 6);
    memcpy(out_buffer, writebuff, 3);
    libusb_submit_transfer(transfer_out);
    libusb_handle_events_completed(ctx, NULL);
    libusb_submit_transfer(transfer_in);
    libusb_handle_events_completed(ctx, &read_completed);
    return *result;
  }
  return 0xFF;
}


/* THREADING */

void USBSID_Class::USBSID_Flush(void)
{
  USBSID_SetFlush();
  USBSID_FlushBuffer();
}

void USBSID_Class::USBSID_SetFlush(void)
{
  flush_buffer = 1;
}

void USBSID_Class::USBSID_FlushBuffer(void)
{
  if (flush_buffer == 1 && buffer_pos >= 5) {
    thread_buffer[0] = (CYCLED_WRITE << 6 | (buffer_pos - 1));
    memcpy(out_buffer, thread_buffer, buffer_pos);
    buffer_pos = 1;
    flush_buffer = 0;
    libusb_submit_transfer(transfer_out);
    libusb_handle_events_completed(ctx, NULL);
    memset(thread_buffer, 0, 64);
    memset(out_buffer, 0, len_out_buffer);
  }
}

void* USBSID_Class::USBSID_Thread(void)
{ /* Only starts when threaded == true */
  USBDBG(stdout, "[USBSID] Thread started\r\n");
  #ifdef _GNU_SOURCE
  pthread_setname_np(pthread_self(), "USBSID Thread");
  #endif
  pthread_detach(pthread_self());
  USBDBG(stdout, "[USBSID] Thread detached\r\n");
  if (withcycles) {
    USBDBG(stdout, "[USBSID] Thread with cycles\r\n");
  }
  while(run_thread) {
    if (flush_buffer == 1) {
      USBSID_FlushBuffer();
    }
    while (ringbuffer.ring_read != ringbuffer.ring_write) {
      if (withcycles) {
        USBSID_RingPopCycled();
      } else {
        USBSID_RingPop();
      }
      /* USBSID_FillMemory(); */
      /* USBSID_DebugPrint(); */
    }
  }
  USBDBG(stdout, "[USBSID] Thread finished\r\n");
  pthread_exit(NULL);

  return NULL;
}

void USBSID_Class::USBSID_StopThread(void)
{
  ringbuffer.ring_read = 0, ringbuffer.ring_write = 0;
  run_thread = 0;
}

int USBSID_Class::USBSID_IsRunning(void)
{
  return run_thread;
}

void USBSID_Class::USBSID_RestartThread(bool with_cycles)
{
  if (USBSID_IsRunning()) {
    /* Stop any active transfers */
    threaded = withcycles = false;
    ringbuffer.ring_read = ringbuffer.ring_write = 0;
    LIBUSB_StopTransfers();
    /* First check if not already running */
    LIBUSB_StopThread();
  }
  LIBUSB_FreeOutBuffer();
  threaded = true;
  withcycles = with_cycles;
  len_out_buffer = LEN_OUT_BUFFER;
  LIBUSB_InitOutBuffer();
  run_thread = 1;
  flush_buffer = 0;
  buffer_pos = 1;
  ringbuffer.ring_read = ringbuffer.ring_write = 0;
  pthread_create(&this->ptid, NULL, &this->_USBSID_Thread, NULL);
}


/* RINGBUFFER FOR THREADED WRITES */

void USBSID_Class::USBSID_WriteRing(uint8_t reg, uint8_t val)
{
  if (threaded && !withcycles) {
    ringbuffer.ringpush = ((reg & 0xFF) << 8) | val;
    ringbuffer.ring_buffer[ringbuffer.ring_write++] = ringbuffer.ringpush;
  } else {
    USBERR(stderr, "[USBSID] Function '%s' cannot be used when threaded = %d and withcycles = %d\n", __func__, threaded, withcycles);
  }
}

void USBSID_Class::USBSID_WriteRingCycled(uint8_t reg, uint8_t val, uint16_t cycles)
{
  if (threaded && withcycles) {
    ringbuffer.ringpush = ((reg & 0xFF) << 8) | val;
    ringbuffer.ring_buffer[ringbuffer.ring_write++] = ringbuffer.ringpush;
    ringbuffer.ringpush = cycles;
    ringbuffer.ring_buffer[ringbuffer.ring_write++] = ringbuffer.ringpush;
  } else {
    USBERR(stderr, "[USBSID] Function '%s' cannot be used when threaded = %d and withcycles = %d\n", __func__, threaded, withcycles);
  }
}

void USBSID_Class::USBSID_RingPopCycled(void)
{
  ringbuffer.ringpop = ringbuffer.ring_buffer[ringbuffer.ring_read++];
  uint8_t reg = (uint8_t)(ringbuffer.ringpop >> 8);
  uint8_t val = (uint8_t)(ringbuffer.ringpop & 0xFF);
  ringbuffer.ringpop = ringbuffer.ring_buffer[ringbuffer.ring_read++];
  uint16_t cycles = ringbuffer.ringpop;

  thread_buffer[buffer_pos++] = reg; /* register */
  thread_buffer[buffer_pos++] = val; /* value */
  thread_buffer[buffer_pos++] = (cycles >> 8);  /* n cycles high */
  thread_buffer[buffer_pos++] = (cycles & 0xFF);  /* n cycles low */

  if (buffer_pos == 61  /* >= 61 || >= 4 */
      || buffer_pos == len_out_buffer
      || flush_buffer == 1) {
    flush_buffer = 0;
    thread_buffer[0] = (CYCLED_WRITE << 6 | (buffer_pos - 1));
    memcpy(out_buffer, thread_buffer, buffer_pos);
    buffer_pos = 1;
    libusb_submit_transfer(transfer_out);
    libusb_handle_events_completed(ctx, NULL);
    memset(thread_buffer, 0, 64);
    memset(out_buffer, 0, len_out_buffer);
    return;
  }
}

void USBSID_Class::USBSID_RingPop(void)
{
  write_completed = 0;
  ringbuffer.ringpop = ringbuffer.ring_buffer[ringbuffer.ring_read++];

  /* Ex: 0xD418 */
  out_buffer[0] = 0x0;
  out_buffer[1] = (uint8_t)(ringbuffer.ringpop >> 8);
  out_buffer[2] = (uint8_t)(ringbuffer.ringpop & 0xFF);
  libusb_submit_transfer(transfer_out);
  libusb_handle_events_completed(ctx, NULL);
}

uint8_t * USBSID_Class::USBSID_RingPop(bool return_busvalue)
{ /* Unused at the moment */
#ifdef DEBUG_USBSID_MEMORY
  write_completed = 0;
  ringbuffer.ringpop = ringbuffer.ring_buffer[ringbuffer.ring_read++];

  /* Ex: 0xD418 */
  out_buffer[0] = 0x0;
  out_buffer[1] = (uint8_t)(ringbuffer.ringpop >> 8);
  out_buffer[2] = (uint8_t)(ringbuffer.ringpop & 0xFF);
  libusb_submit_transfer(transfer_out);
  libusb_handle_events_completed(ctx, NULL);

  if (threaded && return_busvalue) {  /* return value for DebugPrint */
    memcpy(temp_buffer, out_buffer, 3);
    return temp_buffer;
  }
#endif
  return 0x0;
}


/* BUS */

uint8_t USBSID_Class::USBSID_Address(uint16_t addr)
{ /* Unused at the moment */
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


/* SID MEMORY RELATED */

void USBSID_Class::USBSID_FlushMemory(void)
{ /* Unused at the moment */
#ifdef DEBUG_USBSID_MEMORY
  int pos = 1;
  out_buffer[0] = 0xFF; /* ID This as fast writes */
  for (uint8_t i = 1; i < 64; i++) {
    if (pos == 63) { /* Emergency flush? */
      libusb_submit_transfer(transfer_out);
      libusb_handle_events_completed(ctx, &write_completed);
      out_buffer[0] = 0xFF; /* ID This as fast writes */
      pos = 1;
    }
    if (sid_memory_changed[i] == 1) {
      write_completed = 0;
      out_buffer[pos++] = i; /* register */
      out_buffer[pos++] = sid_memory[i]; /* value */
      out_buffer[pos++] = (sid_memory_cycles[i] >> 8);  /* n cycles high */
      out_buffer[pos++] = (sid_memory_cycles[i] & 0xFF);  /* n cycles low */
      printf("[M%02X]$%02X:%02X[C]%d\n", out_buffer[0], i, sid_memory[i], sid_memory_cycles[i]);
    }
  }
  libusb_submit_transfer(transfer_out);
  libusb_handle_events_completed(ctx, &write_completed);
  memset(sid_memory, 0, (sizeof(sid_memory) / sizeof(sid_memory[0])));
  memset(sid_memory_changed, 0, (sizeof(sid_memory_changed) / sizeof(sid_memory_changed[0])));
  memset(sid_memory_cycles, 0, (sizeof(sid_memory_cycles) / sizeof(sid_memory_cycles[0])));
#endif
}

void USBSID_Class::USBSID_FillMemory(void)
{ /* Unused at the moment */
#ifdef DEBUG_USBSID_MEMORY
  ringbuffer.ringpop = ringbuffer.ring_buffer[ringbuffer.ring_read++];
  uint8_t reg = (uint8_t)(ringbuffer.ringpop >> 8);
  uint8_t val = (uint8_t)(ringbuffer.ringpop & 0xFF);
  ringbuffer.ringpop = ringbuffer.ring_buffer[ringbuffer.ring_read++];
  /* int c = ((ringbuffer.ringpop & 0x8000) == 0x8000); */
  uint16_t cycles = (ringbuffer.ringpop ^ 0x8000);
  if (sid_memory_changed[reg] == 0) {
    sid_memory[reg] = val;
    sid_memory_changed[reg]++;
    sid_memory_cycles[reg] = cycles;
    if ((reg & 0x1f) > 0x16) USBSID_FlushMemory();
  } else {  /* We're updating a previously filled register, flush immediately */
    USBSID_FlushMemory();
    sid_memory[reg] = val;
    sid_memory_changed[reg]++;
    sid_memory_cycles[reg] = cycles;
  }
#endif
}

void USBSID_Class::USBSID_DebugPrint(void)
{ /* Unused at the moment */
#ifdef DEBUG_USBSID_MEMORY
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
#endif
}


/* TIMING AND CYCLES */

uint_fast64_t USBSID_Class::USBSID_CycleFromTimestamp(timestamp_t timestamp)
{
  m_InvCPUcycleDurationNanoSeconds = 1.0 / (1000000000 / cycles_per_sec);
  auto nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(timestamp - m_StartTime);
  return (int64_t)(nsec.count() * m_InvCPUcycleDurationNanoSeconds);
}

uint_fast64_t USBSID_Class::USBSID_WaitForCycle(uint_fast16_t cycles)
{
  USBSID_GetClockRate();  /* Make sure we use the right clockrate */
  timestamp_t now = std::chrono::high_resolution_clock::now();
  /* double dur = cycles * m_InvCPUcycleDurationNanoSeconds; */
  double dur = cycles * m_CPUcycleDuration;
  duration_t duration = (duration_t)(int_fast64_t)dur;
  auto target_time = m_NextTime + duration;
  auto target_delta = target_time - now;
  /* auto wait_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(target_delta * 1000); */
  auto wait_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(target_delta);
  // auto wait_msec = std::chrono::duration_cast<std::chrono::milliseconds>(target_delta);
  if (wait_nsec.count() > 0) {
      std::this_thread::sleep_for(wait_nsec);
  }

  while (now < target_time) {
      now = std::chrono::high_resolution_clock::now();
  }
  m_CurrentTime       = now;
  m_NextTime          = target_time;

  /* ISSUE: returned cycles seem incorrect but does not affect playing */
  int_fast64_t waited_cycles = (wait_nsec.count() * m_InvCPUcycleDurationNanoSeconds);
  return waited_cycles;
}


/* LIBUSB */

void USBSID_Class::LIBUSB_StopThread(void)
{
  USBDBG(stdout, "[USBSID] Stop thread\r\n");
  if (USBSID_IsRunning()) {
    USBSID_StopThread();
    USBDBG(stdout, "[USBSID] Set thread exit = 1\r\n");
    pthread_join(ptid, NULL);
    USBDBG(stdout, "[USBSID] Thread attached\r\n");
    threaded = false;
    withcycles = false;
  }
}

int USBSID_Class::LIBUSB_OpenDevice(void)
{
  USBDBG(stdout, "[USBSID] Open device\r\n");
  /* Look for a specific device and open it. */
  devh = libusb_open_device_with_vid_pid(ctx, VENDOR_ID, PRODUCT_ID);
  if (!devh) {
    rc = -1;
    USBERR(stderr, "[USBSID] Error opening USB device with VID & PID: %d %s: %s\r\n", rc, libusb_error_name(rc), libusb_strerror(rc));
  }
  return rc;
}

void USBSID_Class::LIBUSB_CloseDevice(void)
{
  USBDBG(stdout, "[USBSID] Close device\r\n");
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
}

int USBSID_Class::LIBUSB_DetachKernelDriver(void)
{
  USBDBG(stdout, "[USBSID] Detach kernel driver\r\n");
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
      break;
    }
  }
  return rc;
}

int USBSID_Class::LIBUSB_ConfigureDevice(void)
{
  USBDBG(stdout, "[USBSID] Configure device\r\n");
  /* Start configuring the device:
   * set line state */
  rc = libusb_control_transfer(devh, 0x21, 0x22, ACM_CTRL_DTR | ACM_CTRL_RTS, 0, NULL, 0, 0);
  if (rc < 0) {  /* should return 0 or higher */
    USBERR(stderr, "[USBSID] Error configuring line state during control transfer: %d, %s: %s\r\n", rc, libusb_error_name(rc), libusb_strerror(rc));
    rc = -1;
    return rc;
  }

  /* set line encoding here */  // NOTE: NOT USED FOR CDC
  rc = libusb_control_transfer(devh, 0x21, 0x20, 0, 0, encoding, sizeof(encoding), 0);
  if (rc < 0 || rc != 7) {  /* should return 7 for the encoding size */
    USBERR(stderr, "[USBSID] Error configuring line encoding during control transfer: %d, %s: %s\r\n", rc, libusb_error_name(rc), libusb_strerror(rc));
    rc = -1;
    return rc;
  }
  return rc;
}

void USBSID_Class::LIBUSB_InitOutBuffer(void)
{
  USBDBG(stdout, "[USBSID] Init out buffers\r\n");
  out_buffer = libusb_dev_mem_alloc(devh, len_out_buffer);
  if (out_buffer == NULL) {
    USBDBG(stdout, "[USBSID] libusb_dev_mem_alloc failed on out_buffer, allocating with malloc\r\n");
    /* TODO: Maybe change to vector array? https://stackoverflow.com/a/24575552 */
    out_buffer = (uint8_t*)malloc((sizeof(uint8_t)) * len_out_buffer);
  } else {
    out_buffer_dma = true;
  }
  USBDBG(stdout, "[USBSID] Alloc out_buffer complete\r\n");
  transfer_out = libusb_alloc_transfer(0);
  USBDBG(stdout, "[USBSID] Alloc transfer_out complete\r\n");
  libusb_fill_bulk_transfer(transfer_out, devh, EP_OUT_ADDR, out_buffer, len_out_buffer, usb_out, NULL, 0);
  USBDBG(stdout, "[USBSID] libusb_fill_bulk_transfer transfer_out complete\r\n");

  if (write_buffer == NULL) {
    write_buffer = (uint8_t*)malloc((sizeof(uint8_t)) * (len_out_buffer));
  }
}

void USBSID_Class::LIBUSB_FreeOutBuffer(void)
{
  USBDBG(stdout, "[USBSID] Free out buffers\r\n");
  if (out_buffer_dma) {
    rc = libusb_dev_mem_free(devh, out_buffer, len_out_buffer);
    if (rc < 0) {
      USBERR(stderr, "[USBSID] Error, failed to free out_buffer DMA memory: %d, %s: %s\n", rc, libusb_error_name(rc), libusb_strerror(rc));
    }
  } else {
    if (out_buffer) free(out_buffer);
    out_buffer = NULL;
  }
  if (thread_buffer) {
    free(thread_buffer);
    thread_buffer = NULL;
  }
  if (write_buffer) {
    free(write_buffer);
    write_buffer = NULL;
  }
}

void USBSID_Class::LIBUSB_InitInBuffer(void)
{
  USBDBG(stdout, "[USBSID] Init in buffers\r\n");
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

  if (result == NULL) {
    result = (uint8_t*)malloc((sizeof(uint8_t)) * (LEN_IN_BUFFER));
  }
}

void USBSID_Class::LIBUSB_FreeInBuffer(void)
{
  USBDBG(stdout, "[USBSID] Free in buffers\r\n");
  if (in_buffer_dma) {
    rc = libusb_dev_mem_free(devh, in_buffer, LEN_IN_BUFFER);
    if (rc < 0) {
      USBERR(stderr, "[USBSID] Error, failed to free in_buffer DMA memory: %d, %s: %s\n", rc, libusb_error_name(rc), libusb_strerror(rc));
    }
  } else {
    if (in_buffer) free(in_buffer);
    in_buffer = NULL;
  }
  if (result) {
    free(result);
    result = NULL;
  }
}

void USBSID_Class::LIBUSB_StopTransfers(void)
{
  USBDBG(stdout, "[USBSID] Stopping transfers\r\n");
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
}

int USBSID_Class::LIBUSB_Setup(bool start_threaded, bool with_cycles)
{
  rc = read_completed = write_completed = -1;
  threaded = start_threaded;
  withcycles = with_cycles;
  len_out_buffer = LEN_OUT_BUFFER;
  thread_buffer = (uint8_t*)malloc((sizeof(uint8_t)) * (LEN_OUT_BUFFER));
#ifdef DEBUG_USBSID_MEMORY
  temp_buffer = (uint8_t*)malloc((sizeof(uint8_t)) * (LEN_TMP_BUFFER));
#endif

  /* Initialize libusb */
  rc = libusb_init(&ctx);
  // rc = libusb_init_context(&ctx, /*options=NULL, /*num_options=*/0);  // NOTE: REQUIRES LIBUSB 1.0.27!!
  if (rc != 0) {
    USBERR(stderr, "[USBSID] Error initializing libusb: %d %s: %s\r\n", rc, libusb_error_name(rc), libusb_strerror(rc));
    goto out;
  }

  /* Set debugging output to min/max (4) level */
  libusb_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, 0);

  if (LIBUSB_OpenDevice() < 0) {
    goto out;
  }
  if (LIBUSB_DetachKernelDriver() < 0) {
    goto out;
  }
  if (LIBUSB_ConfigureDevice() < 0) {
    goto out;
  }
  LIBUSB_InitOutBuffer();
  LIBUSB_InitInBuffer();

  if (rc < 0) {
    USBERR(stderr, "[USBSID] Error, could not open device: %d, %s: %s\r\n", rc, libusb_error_name(rc), libusb_strerror(rc));
    goto out;
  }

  if (rc > 0 && rc == 7) {  /* 7 for the return size of the encoding */
    rc = 0;
  }

  return rc;
out:
  LIBUSB_Exit();
  return rc;
}

int USBSID_Class::LIBUSB_Exit(void)
{

  LIBUSB_StopThread();
  USBSID_Reset();
  LIBUSB_StopTransfers();
  LIBUSB_FreeInBuffer();
  LIBUSB_FreeOutBuffer();
  LIBUSB_CloseDevice();
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
  if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
    rc = transfer->status;
    if (rc != LIBUSB_TRANSFER_CANCELLED) {
      USBERR(stderr, "[USBSID] Warning: transfer out interrupted with status %d, %s: %s\r", rc, libusb_error_name(rc), libusb_strerror(rc));
    }
    libusb_free_transfer(transfer);
    return;
  }

  if (transfer->actual_length != len_out_buffer) {
    USBERR(stderr, "[USBSID] Sent data length %d is different from the defined buffer length: %d or actual length %d\r", transfer->length, len_out_buffer, transfer->actual_length);
  }

  // BUG: Resubmit is shit for normal tunes but good for cycle exact digitunes, sigh...
  // if (threaded) libusb_submit_transfer(transfer_out);  /* Resubmit queue when finished */
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


} /* extern "C" */

#pragma GCC pop_options
