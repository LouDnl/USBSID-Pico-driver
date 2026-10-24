/*
 * SIDKick-pico SYS 54301 loader replay, built by test/bench/build.sh.
 * Needs one USBSID-Pico board with a SIDKick-pico as SID1. Not part of CI.
 *
 * Runs the code a C64 fetches from SID space after SYS 54301 with a minimal
 * 6502 subset, issuing the same SID bus reads (dummy reads included) a real
 * 6502 makes. Captures the RAM writes and times the whole load. Resets the
 * SIDs before the run (a previous run may have left the SIDKick-pico in
 * transfer mode) and afterwards to leave transfer mode. Prints progress once
 * per second; Ctrl-C stops the run, prints the summary and resets the SIDs.
 * A full async load takes about 90 s, sync about 60 s.
 *
 * Usage: skpico_loader [sync|async|batch] [-o outfile]
 *
 *   sync   reads via USBSID_SingleRead (libusb_bulk_transfer)
 *   async  reads via USBSID_Read (default, the path VICE uses)
 *   batch  traces the SID addresses of one loop iteration with single
 *          reads, then reads every following iteration with one
 *          USBSID_ReadMulti call (firmware 0.8.1 or newer). Each batch is
 *          checked against the trace, a mismatch aborts the run.
 *   -o     write the captured RAM range as a PRG (load address + data)
 */

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "USBSID.h"

using namespace USBSID_NS;
using bench_clock = std::chrono::steady_clock;

/* Upper bound on SID reads, stops a runaway loop */
static const long MAX_READS = 2000000;
/* Settle time after the SID reset before the first read */
static const int RESET_SETTLE_MS = 500;

/* Set by SIGINT, ends the run loop */
static volatile std::sig_atomic_t g_stop = 0;

/**
 * @brief: SIGINT handler, requests a clean stop.
 *
 * @param sig: signal number, unused
 */
static void on_sigint(int sig)
{
  (void)sig;
  g_stop = 1;
}

struct cpu_t {
  uint16_t pc = 0xD41D;
  uint8_t a = 0, sp = 0xF6;
};

enum read_mode_t { MODE_ASYNC, MODE_SYNC, MODE_BATCH };

/* Most registers one USBSID_ReadMulti call takes */
static const size_t MAX_BATCH = 63;

struct run_t {
  USBSID_Class *us = nullptr;
  read_mode_t mode = MODE_ASYNC;
  bool sync = false;
  long reads = 0;
  long round_trips = 0;
  /* Batch mode */
  std::vector<uint16_t> trace;          /* SID addresses read so far, single read phase */
  std::map<uint16_t, size_t> first_pc;  /* Instruction PC -> trace index at its first visit */
  bool batching = false;
  uint16_t loop_pc = 0;                 /* Start of the traced iteration */
  std::vector<uint16_t> iter_addr;      /* SID addresses of one iteration */
  std::vector<uint8_t> iter_reg;        /* Same, as register numbers */
  std::vector<uint8_t> iter_val;        /* Values of the current batch */
  size_t iter_pos = 0;                  /* Next value in iter_val */
  long batches = 0;
  std::string fail;                     /* Set on a trace mismatch */
  bench_clock::time_point t0;
  std::vector<uint8_t> ram = std::vector<uint8_t>(65536, 0);
  std::vector<bool> written = std::vector<bool>(65536, false);
  long stores = 0;
};

/**
 * @brief: Check whether an address is in SID I/O space ($D400-$D7FF).
 *
 * @param addr: CPU address
 * @return: true when the address maps to the SID
 */
static bool in_sid(uint16_t addr)
{
  return (addr >= 0xD400) && (addr <= 0xD7FF);
}

/**
 * @brief: Perform one CPU read, SID space goes to the board, RAM to the local image.
 *
 * @param r: run state
 * @param addr: CPU address
 * @return: byte read
 */
static uint8_t cpu_read(run_t &r, uint16_t addr)
{
  if (!in_sid(addr)) return r.ram[addr];
  r.reads++;
  uint8_t reg = (uint8_t)(addr & 0x1F);
  if (r.mode == MODE_BATCH) {
    if (r.batching) {
      if (r.iter_pos >= r.iter_val.size() || r.iter_addr[r.iter_pos] != addr) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "trace mismatch: read $%04X at batch position %zu",
          addr, r.iter_pos);
        if (r.fail.empty()) r.fail = buf;
        return 0;
      }
      return r.iter_val[r.iter_pos++];
    }
    r.trace.push_back(addr);
    r.round_trips++;
    return r.us->USBSID_SingleRead(reg);
  }
  r.round_trips++;
  return r.sync ? r.us->USBSID_SingleRead(reg) : r.us->USBSID_Read(reg);
}

/**
 * @brief: Batch mode bookkeeping at the start of each instruction.
 *
 * Single read phase: a PC executed before closes the trace, the reads since
 * its first visit form one iteration. Batch phase: at the loop start, read
 * the whole next iteration with one USBSID_ReadMulti call.
 *
 * @param r: run state
 * @param pc: PC of the instruction about to run
 */
static void batch_step(run_t &r, uint16_t pc)
{
  if (!r.batching) {
    auto it = r.first_pc.find(pc);
    if (it == r.first_pc.end()) {
      r.first_pc[pc] = r.trace.size();
      return;
    }
    r.iter_addr.assign(r.trace.begin() + (long)it->second, r.trace.end());
    if (r.iter_addr.empty() || r.iter_addr.size() > MAX_BATCH) {
      char buf[64];
      std::snprintf(buf, sizeof(buf), "iteration of %zu reads, cannot batch", r.iter_addr.size());
      r.fail = buf;
      return;
    }
    r.iter_reg.clear();
    for (uint16_t a : r.iter_addr) r.iter_reg.push_back((uint8_t)(a & 0x1F));
    r.loop_pc = pc;
    r.batching = true;
    r.iter_val.clear();
    r.iter_pos = 0;
    std::printf("traced loop at $%04X, %zu SID reads per iteration\n", pc, r.iter_addr.size());
  }
  if (pc != r.loop_pc) return;
  if (r.iter_pos != r.iter_val.size()) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "trace mismatch: loop start after %zu of %zu reads",
      r.iter_pos, r.iter_val.size());
    r.fail = buf;
    return;
  }
  r.iter_val.assign(r.iter_reg.size(), 0);
  if (r.us->USBSID_ReadMulti(r.iter_reg.data(), r.iter_val.data(), (int)r.iter_reg.size())
      != (int)r.iter_reg.size()) {
    r.fail = "USBSID_ReadMulti failed";
    return;
  }
  r.iter_pos = 0;
  r.batches++;
  r.round_trips++;
}

/**
 * @brief: Run the loader until PC leaves SID space or an unsupported opcode.
 *
 * @param r: run state
 * @param c: CPU state, PC holds the start address
 * @return: stop reason text
 */
static std::string run_loader(run_t &r, cpu_t &c)
{
  auto t_report = r.t0 + std::chrono::seconds(1);
  while (r.reads < MAX_READS) {
    if (g_stop) return "interrupted";
    if (bench_clock::now() >= t_report) {
      t_report += std::chrono::seconds(1);
      const double secs = std::chrono::duration<double>(bench_clock::now() - r.t0).count();
      std::printf("%6.1f s  reads %7ld  stores %6ld  trips %7ld  PC $%04X\n", secs, r.reads, r.stores, r.round_trips, c.pc);
      std::fflush(stdout);
    }
    if (!in_sid(c.pc)) return "PC left SID space";
    if (r.mode == MODE_BATCH) batch_step(r, c.pc);
    if (!r.fail.empty()) return r.fail;
    uint8_t op = cpu_read(r, c.pc);
    switch (op) {
      case 0x78: /* SEI: fetch, dummy read PC+1 */
        cpu_read(r, (uint16_t)(c.pc + 1));
        c.pc += 1;
        break;
      case 0x48: /* PHA: fetch, dummy read PC+1, push */
        cpu_read(r, (uint16_t)(c.pc + 1));
        r.ram[0x100 + c.sp--] = c.a;
        c.pc += 1;
        break;
      case 0x68: /* PLA: fetch, dummy read PC+1, dummy stack read, pull */
        cpu_read(r, (uint16_t)(c.pc + 1));
        c.a = r.ram[0x100 + (uint8_t)(++c.sp)];
        c.pc += 1;
        break;
      case 0xA9: /* LDA #imm: fetch, operand */
        c.a = cpu_read(r, (uint16_t)(c.pc + 1));
        c.pc += 2;
        break;
      case 0x8D: { /* STA abs: fetch, lo, hi, write */
        uint16_t lo = cpu_read(r, (uint16_t)(c.pc + 1));
        uint16_t hi = cpu_read(r, (uint16_t)(c.pc + 2));
        uint16_t ea = (uint16_t)(lo | (hi << 8));
        if (ea >= 0xD000 && ea <= 0xDFFF) {
          char buf[64];
          std::snprintf(buf, sizeof(buf), "STA into I/O $%04X", ea);
          return buf;
        }
        r.ram[ea] = c.a;
        r.written[ea] = true;
        r.stores++;
        c.pc += 3;
        break;
      }
      case 0x4C: { /* JMP abs: fetch, lo, hi */
        uint16_t lo = cpu_read(r, (uint16_t)(c.pc + 1));
        uint16_t hi = cpu_read(r, (uint16_t)(c.pc + 2));
        c.pc = (uint16_t)(lo | (hi << 8));
        break;
      }
      default: {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "unsupported opcode $%02X at $%04X", op, c.pc);
        return buf;
      }
    }
  }
  return "read limit reached";
}

int main(int argc, char **argv)
{
  run_t r;
  const char *outfile = nullptr;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "sync")) { r.mode = MODE_SYNC; r.sync = true; }
    else if (!std::strcmp(argv[i], "async")) { r.mode = MODE_ASYNC; r.sync = false; }
    else if (!std::strcmp(argv[i], "batch")) { r.mode = MODE_BATCH; r.sync = true; }
    else if (!std::strcmp(argv[i], "-o") && i + 1 < argc) outfile = argv[++i];
  }

  USBSID_Class us;
  if (us.USBSID_Init(false, false) < 0) {
    std::fprintf(stderr, "No USBSID-Pico found\n");
    return 1;
  }
  r.us = &us;
  std::signal(SIGINT, on_sigint);

  static const char *mode_name[] = {"async", "sync", "batch"};
  static const int mode_secs[] = {90, 60, 5};
  if (r.mode == MODE_BATCH) {
    const int fv = us.USBSID_GetFirmwareVersionInt();
    if (fv < US_FW_VERSION_BATCH_READ) {
      std::printf("Firmware without batch read (version %d): batches fall back to single reads\n", fv);
    }
  }
  std::printf("Resetting SIDs, %s mode, about %d s for a full load\n",
    mode_name[r.mode], mode_secs[r.mode]);
  us.USBSID_Reset();
  std::this_thread::sleep_for(std::chrono::milliseconds(RESET_SETTLE_MS));

  cpu_t c;
  r.t0 = bench_clock::now();
  std::string why = run_loader(r, c);
  double secs = std::chrono::duration<double>(bench_clock::now() - r.t0).count();

  int lo = -1, hi = -1;
  for (int a = 0; a < 65536; a++) {
    if (!r.written[a]) continue;
    if (lo < 0) lo = a;
    hi = a;
  }

  static const char *mode_desc[] = {"async (USBSID_Read)", "sync (USBSID_SingleRead)",
    "batch (USBSID_ReadMulti per iteration)"};
  std::printf("mode          %s\n", mode_desc[r.mode]);
  std::printf("stop          %s, PC $%04X, A $%02X\n", why.c_str(), c.pc, c.a);
  std::printf("SID reads     %ld\n", r.reads);
  std::printf("RAM stores    %ld\n", r.stores);
  if (lo >= 0) std::printf("RAM range     $%04X-$%04X (%d bytes)\n", lo, hi, hi - lo + 1);
  std::printf("time          %.2f s\n", secs);
  if (r.reads) std::printf("per read      %.1f us\n", 1e6 * secs / r.reads);
  if (r.stores) std::printf("reads/store   %.2f\n", (double)r.reads / r.stores);
  std::printf("round trips   %ld\n", r.round_trips);
  if (r.round_trips) std::printf("per trip      %.1f us\n", 1e6 * secs / r.round_trips);
  if (r.mode == MODE_BATCH) std::printf("batches       %ld\n", r.batches);

  if (outfile && lo >= 0) {
    FILE *f = std::fopen(outfile, "wb");
    if (f) {
      uint8_t la[2] = {(uint8_t)(lo & 0xFF), (uint8_t)(lo >> 8)};
      std::fwrite(la, 1, 2, f);
      std::fwrite(&r.ram[lo], 1, (size_t)(hi - lo + 1), f);
      std::fclose(f);
      std::printf("written       %s\n", outfile);
    }
  }

  us.USBSID_Reset();
  us.USBSID_Close();
  return 0;
}
