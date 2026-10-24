/*
 * Read latency benchmark for driver-repo/src, built by test/bench/build.sh.
 * Needs one USBSID-Pico board attached. Not part of CI.
 *
 * Usage: read_latency [-n iterations] [--probe]
 *
 *   -n N     samples per case (default 2000)
 *   --probe  also query the firmware READ_COST_PROBE (firmware built with
 *            -DREAD_COST_PROBE=1), reports bus read cost in PHI1 cycles,
 *            skips the USBSID_ReadMulti cases
 */

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "USBSID.h"

using namespace USBSID_NS;
using bench_clock = std::chrono::steady_clock;

/* READ packet type, top 2 bits of byte 0 */
static const uint8_t PKT_READ = (1 << 6);
/* CYCLED_WRITE packet type, top 2 bits of byte 0 */
static const uint8_t PKT_CYCLED_WRITE = (2 << 6);
/* Register read in every case: SID1 OSC3 */
static const uint8_t READ_REG = 0x1B;
/* Register written in the queued-writes case: SID1 voice 3 freq lo */
static const uint8_t WRITE_REG = 0x0E;

/**
 * @brief: Print min/median/p90/p99/max/mean of a sample set in microseconds.
 *
 * @param name: case label
 * @param us: samples in microseconds, sorted in place
 */
static void report(const char *name, std::vector<double> &us)
{
  if (us.empty()) return;
  std::sort(us.begin(), us.end());
  double sum = 0;
  for (double v : us) sum += v;
  auto pct = [&](double p) { return us[(size_t)(p * (us.size() - 1))]; };
  std::printf("%-34s n=%-5zu min %8.1f  p50 %8.1f  p90 %8.1f  p99 %8.1f  max %8.1f  mean %8.1f us\n",
    name, us.size(), us.front(), pct(0.50), pct(0.90), pct(0.99), us.back(), sum / us.size());
}

/**
 * @brief: Microseconds elapsed since a start point.
 *
 * @param t0: start point
 * @return: elapsed time in microseconds
 */
static double since_us(bench_clock::time_point t0)
{
  return std::chrono::duration<double, std::micro>(bench_clock::now() - t0).count();
}

/**
 * @brief: Send one CYCLED_WRITE packet of k writes, d cycles each, synchronously.
 *
 * @param us: opened driver instance
 * @param k: number of writes, 1-15
 * @param d: delay cycles per write
 */
static void send_cycled_batch(USBSID_Class &us, int k, uint16_t d)
{
  unsigned char buf[64] = {0};
  buf[0] = (uint8_t)(PKT_CYCLED_WRITE | (4 * k));
  for (int i = 0; i < k; i++) {
    buf[1 + 4 * i] = WRITE_REG;
    buf[2 + 4 * i] = 0x00;
    buf[3 + 4 * i] = (uint8_t)(d >> 8);
    buf[4 + 4 * i] = (uint8_t)(d & 0xFF);
  }
  us.USBSID_SingleWrite(buf, 1 + 4 * k);
}

/**
 * @brief: Query the firmware read cost probe once.
 *
 * @param us: opened driver instance
 * @param count: reads the firmware times back to back, 1-63
 * @return: elapsed PHI1 cycles reported by the firmware
 */
static int probe_once(USBSID_Class &us, int count)
{
  unsigned char buf[3] = {(unsigned char)(PKT_READ | (count & 0x3F)), READ_REG, 0};
  unsigned char res[1] = {0};
  us.USBSID_SingleWrite(buf, 3);
  us.USBSID_SingleReadConfig(res, 1);
  return res[0];
}

int main(int argc, char **argv)
{
  int n = 2000;
  bool probe = false;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "-n") && i + 1 < argc) n = std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "--probe")) probe = true;
  }
  const int warmup = 50;

  USBSID_Class us;
  if (us.USBSID_Init(false, false) < 0) {
    std::fprintf(stderr, "No USBSID-Pico found\n");
    return 1;
  }
  long clk = us.USBSID_GetClockRate();
  const int fwver = us.USBSID_GetFirmwareVersionInt();
  std::printf("Board serial %s, SID clock %ld Hz, %d samples per case, batch read %s\n\n",
    us.USBSID_GetSerial().c_str(), clk, n,
    (fwver < 0) ? "unknown" : ((fwver >= US_FW_VERSION_BATCH_READ) ? "yes" : "no"));

  /* Async single write, OUT only: reference for the OUT half of a read */
  {
    std::vector<double> t;
    for (int i = 0; i < n + warmup; i++) {
      auto t0 = bench_clock::now();
      us.USBSID_WriteCycled(WRITE_REG, 0x00, 6);
      if (i >= warmup) t.push_back(since_us(t0));
    }
    report("write (async, OUT only)", t);
  }

  /* Sync write, OUT only via libusb_bulk_transfer */
  {
    std::vector<double> t;
    unsigned char buf[3] = {0x00, WRITE_REG, 0x00};
    for (int i = 0; i < n + warmup; i++) {
      auto t0 = bench_clock::now();
      us.USBSID_SingleWrite(buf, 3);
      if (i >= warmup) t.push_back(since_us(t0));
    }
    report("write (sync, OUT only)", t);
  }

  /* USBSID_Read: IN pre-submitted, then OUT, then event loop */
  {
    std::vector<double> t;
    for (int i = 0; i < n + warmup; i++) {
      auto t0 = bench_clock::now();
      (void)us.USBSID_Read(READ_REG);
      if (i >= warmup) t.push_back(since_us(t0));
    }
    report("read USBSID_Read (async)", t);
  }

  /* USBSID_SingleRead: sync OUT, then sync IN */
  {
    std::vector<double> t;
    for (int i = 0; i < n + warmup; i++) {
      auto t0 = bench_clock::now();
      (void)us.USBSID_SingleRead(READ_REG);
      if (i >= warmup) t.push_back(since_us(t0));
    }
    report("read USBSID_SingleRead (sync)", t);
  }

  /* USBSID_ReadMulti: n registers ($19-$1C round robin) in one round trip.
   * A READ_COST_PROBE firmware answers cycles on batch reads: skip there. */
  if (!probe) {
    for (int count : {4, 16, 63}) {
      std::vector<uint8_t> regs(count), vals(count);
      for (int r = 0; r < count; r++) regs[r] = (uint8_t)(0x19 + (r & 0x3));
      std::vector<double> t;
      int fails = 0;
      for (int i = 0; i < n + warmup; i++) {
        auto t0 = bench_clock::now();
        if (us.USBSID_ReadMulti(regs.data(), vals.data(), count) != count) fails++;
        if (i >= warmup) t.push_back(since_us(t0));
      }
      char name[64];
      std::snprintf(name, sizeof(name), "read USBSID_ReadMulti x%d (sync)", count);
      report(name, t);
      if (fails) std::printf("  %d failed calls\n", fails);
    }
  }

  /* Read queued behind k cycled writes of d cycles each */
  std::printf("\nRead behind a queued CYCLED_WRITE batch (k writes x 1000 cycles):\n");
  const uint16_t d = 1000;
  const int batch_n = std::max(50, n / 10);
  for (int k : {1, 4, 15}) {
    std::vector<double> total, read_only;
    for (int i = 0; i < batch_n; i++) {
      auto t0 = bench_clock::now();
      send_cycled_batch(us, k, d);
      auto t1 = bench_clock::now();
      (void)us.USBSID_Read(READ_REG);
      total.push_back(since_us(t0));
      read_only.push_back(since_us(t1));
    }
    double batch_us = (clk > 0) ? (1e6 * k * d / (double)clk) : 0;
    char name[64];
    std::snprintf(name, sizeof(name), "k=%-2d total (batch %.0f us)", k, batch_us);
    report(name, total);
    std::snprintf(name, sizeof(name), "k=%-2d read after write returned", k);
    report(name, read_only);
  }

  if (probe) {
    std::printf("\nFirmware bus read cost (READ_COST_PROBE, PHI1 cycles, 1 cycle resolution):\n");
    std::printf("Without a probe build the replies are register values, not cycles.\n");
    double avg[3] = {0, 0, 0};
    const int counts[3] = {1, 4, 16};
    const int reps = 500;
    for (int c = 0; c < 3; c++) {
      int mn = 255, mx = 0;
      long sum = 0;
      for (int i = 0; i < reps; i++) {
        int v = probe_once(us, counts[c]);
        mn = std::min(mn, v);
        mx = std::max(mx, v);
        sum += v;
      }
      avg[c] = (double)sum / reps;
      std::printf("  %2d reads: min %3d  max %3d  mean %6.2f cycles\n", counts[c], mn, mx, avg[c]);
    }
    std::printf("  marginal cost per read (16 vs 1): %.2f cycles\n", (avg[2] - avg[0]) / 15.0);
  }

  us.USBSID_Close();
  return 0;
}
