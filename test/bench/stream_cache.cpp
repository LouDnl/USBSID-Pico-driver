/*
 * SID stream consumer check (USBSID_StreamStart / USBSID_ReadCached),
 * built by test/bench/build.sh. Needs one USBSID-Pico board with the SID stream
 * (US_FEATURE_SIDSTREAM) firmware. Not part of CI. Nothing else may use the board while it runs:
 * the SIDs are reset, voice 1 and 3 of every SID are reprogrammed.
 *
 * Opens the board through the driver (threaded + cycled like VICE, or
 * unthreaded with -u), starts the stream, then for N seconds writes a
 * counter to voice 1 frequency low ($00) of every SID, one write per SID
 * every -c cycles, with voice 3 running a sawtooth. Checks:
 *   - frames per second per SID match the rate, tick gaps, errors
 *   - after the writes stop: ReadCached($00) of every SID equals the last
 *     value written
 *   - during the run: cached $00 never goes back (counter mod 256 aside)
 *
 * Usage: stream_cache [-r rate] [-n seconds] [-c cycles] [-u]
 *
 *   -r  stream rate in Hz: 50, 60, 250, 500 or 1000 (default 1000)
 *   -n  seconds of writes (default 3)
 *   -c  cycles between two writes (default 1000)
 *   -u  unthreaded driver, USBSID_WriteCycled per write
 */

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#include "USBSID.h"

using namespace USBSID_NS;
using bench_clock = std::chrono::steady_clock;

/* Set by SIGINT, ends the write loop */
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

/* Frame statistics, written by the stream callback */
struct stats_t {
  std::atomic<long> frames[4];
  std::atomic<long> deferred[4];
  std::atomic<long> other[4];
  std::atomic<long> gaps[4];
  int last_tick[4];
};

/**
 * @brief: Stream callback: count frames, flags, tick gaps.
 *
 * @param f: frame, STREAM_FRAME_SIZE bytes
 * @param len: frame length
 * @param user: stats_t
 */
static void on_frame(const uint8_t *f, int len, void *user)
{
  (void)len;
  stats_t *st = (stats_t *)user;
  const int sid = f[1] & 0x03;
  const int tick = f[2] | (f[3] << 8);
  st->frames[sid]++;
  if (f[1] & STREAM_FLAG_DEFERRED) st->deferred[sid]++;
  if (f[1] & STREAM_FLAG_OTHER) st->other[sid]++;
  if (st->last_tick[sid] >= 0 && ((st->last_tick[sid] + 1) & 0xFFFF) != tick) st->gaps[sid]++;
  st->last_tick[sid] = tick;
}

int main(int argc, char **argv)
{
  int rate = 1000, seconds = 3;
  long cycles = 1000;
  bool unthreaded = false;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "-r") && i + 1 < argc) rate = std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "-n") && i + 1 < argc) seconds = std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "-c") && i + 1 < argc) cycles = std::strtol(argv[++i], NULL, 0);
    else if (!std::strcmp(argv[i], "-u")) unthreaded = true;
    else {
      std::fprintf(stderr, "Usage: %s [-r rate] [-n seconds] [-c cycles] [-u]\n", argv[0]);
      return 2;
    }
  }
  if (seconds < 1 || cycles < 8 || cycles > 65535) {
    std::fprintf(stderr, "Seconds >= 1, cycles 8-65535\n");
    return 2;
  }

  USBSID_Class us;
  if (us.USBSID_Init(!unthreaded, !unthreaded) < 0) {
    std::fprintf(stderr, "No USBSID-Pico found\n");
    return 1;
  }
  int nsids = us.USBSID_GetNumSIDs();
  if (nsids < 1 || nsids > 4) nsids = 1;
  std::signal(SIGINT, on_sigint);

  static stats_t st;
  for (int s = 0; s < 4; s++) {
    st.frames[s] = st.deferred[s] = st.other[s] = st.gaps[s] = 0;
    st.last_tick[s] = -1;
  }
  us.USBSID_SetStreamCallback(on_frame, &st);

  std::printf("%s driver, %d SID(s), %d Hz, write every %ld cycles for %d s\n",
    unthreaded ? "unthreaded" : "threaded", nsids, rate, cycles, seconds);
  us.USBSID_Reset();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  /* Voice 3 sawtooth on every SID, voice 1 silent */
  for (int s = 0; s < nsids; s++) {
    const uint8_t base = (uint8_t)(s * 0x20);
    if (unthreaded) {
      us.USBSID_WriteCycled(base + 0x0E, 0x00, 8);
      us.USBSID_WriteCycled(base + 0x0F, 0x10, 8);
      us.USBSID_WriteCycled(base + 0x12, 0x20, 8);
    } else {
      us.USBSID_WriteRingCycled(base + 0x0E, 0x00, 8);
      us.USBSID_WriteRingCycled(base + 0x0F, 0x10, 8);
      us.USBSID_WriteRingCycled(base + 0x12, 0x20, 8);
    }
  }
  if (!unthreaded) us.USBSID_Flush();

  if (us.USBSID_StreamStart(rate, 0) != 0) {
    std::fprintf(stderr, "Stream start failed (firmware without the SID stream or unsupported rate)\n");
    us.USBSID_Close();
    return 1;
  }

  /* Writes: counter on $00 of every SID, paced in real time by the cycle count */
  const double us_per_cycle = 1000000.0 / 985248.0;
  long writes = 0, backwards = 0;
  uint8_t last_written[4] = {0, 0, 0, 0};
  int last_cached[4] = {-1, -1, -1, -1};
  const auto t0 = bench_clock::now();
  auto next = t0;
  while (!g_stop && bench_clock::now() - t0 < std::chrono::seconds(seconds)) {
    for (int s = 0; s < nsids; s++) {
      const uint8_t val = (uint8_t)(writes & 0xFF);
      const uint16_t cyc = (uint16_t)((s == 0) ? cycles : 1);
      if (unthreaded) {
        us.USBSID_WriteCycled((uint8_t)(s * 0x20), val, cyc);
      } else {
        us.USBSID_WriteRingCycled((uint8_t)(s * 0x20), val, cyc);
      }
      last_written[s] = val;
    }
    writes++;
    /* Cached $00 may lag but never jumps back by less than a wrap */
    for (int s = 0; s < nsids; s++) {
      const int c = us.USBSID_ReadCached((uint8_t)(s * 0x20));
      if (c >= 0 && last_cached[s] >= 0) {
        const int d = (c - last_cached[s]) & 0xFF;
        if (d > 128) backwards++;
      }
      if (c >= 0) last_cached[s] = c;
    }
    next += std::chrono::microseconds((long)(cycles * us_per_cycle));
    std::this_thread::sleep_until(next);
  }
  if (!unthreaded) us.USBSID_Flush();
  /* Ring lead + a few stream periods */
  std::this_thread::sleep_for(std::chrono::milliseconds(300 + 4000 / rate));

  int bad_final = 0;
  for (int s = 0; s < nsids; s++) {
    const int c = us.USBSID_ReadCached((uint8_t)(s * 0x20));
    if (c != last_written[s]) {
      bad_final++;
      std::printf("  SID %d: cached $00 = %d, last written %d\n", s + 1, c, last_written[s]);
    }
  }
  const double secs = std::chrono::duration<double>(bench_clock::now() - t0).count();
  uint8_t info[STREAM_INFO_LEN];
  const bool have_info = us.USBSID_StreamQuery(info) == 0;
  const bool running = us.USBSID_StreamRunning();
  us.USBSID_StreamStop();

  std::printf("writes per SID %ld, cached value went back %ld times, final mismatches %d\n",
    writes, backwards, bad_final);
  bool ok = (bad_final == 0) && (backwards == 0) && running && us.USBSID_StreamErrors() == 0;
  for (int s = 0; s < nsids; s++) {
    USBSID_StreamState ss;
    us.USBSID_GetStreamState(s, &ss);
    const double fps = st.frames[s] / secs;
    std::printf("SID %d: %ld frames (%.0f/s), deferred %ld, other owner %ld, tick gaps %ld, "
      "last tick %u cycles %u processed %u\n",
      s + 1, st.frames[s].load(), fps, st.deferred[s].load(), st.other[s].load(),
      st.gaps[s].load(),
      ss.tick, ss.cycles, ss.processed);
    if (fps < rate * 0.9) ok = false;
  }
  if (have_info) {
    std::printf("firmware: rate %u, SID mask 0x%X, session %u, dropped %u\n",
      info[6] | (info[7] << 8), info[8], info[10],
      (unsigned)(info[12] | (info[13] << 8) | (info[14] << 16) | ((unsigned)info[15] << 24)));
  }
  std::printf("stream errors %u, %s\n", us.USBSID_StreamErrors(), ok ? "PASS" : "FAIL");

  us.USBSID_SetStreamCallback(NULL, NULL);
  us.USBSID_Reset();
  us.USBSID_Close();
  return ok ? 0 : 1;
}
