/*
 * SID memory stream check, built by test/bench/build.sh.
 * Needs one USBSID-Pico board with the SID stream (US_FEATURE_SIDSTREAM) firmware attached. Not part of
 * CI. Talks raw libusb to the stream interface only, another program can
 * keep using the data interface at the same time.
 *
 * Usage: stream_dump [-r rate] [-s seconds] [-m sidmask]
 *
 *   -r  rate in Hz: 0 (firmware default), 50, 60, 250, 500 or 1000
 *   -s  run time in seconds (default 5)
 *   -m  SID mask, bit n = SID n+1, 0 = all configured (default 0)
 *
 * Prints INFO before and after, per second the frame count per SID, the
 * sequence gaps (skipped ticks), the frames built
 * late and with a non USB bus owner, and the last registers of every streamed SID. Ctrl-C ends the run
 * early, still sends STOP and prints the summary.
 */

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <libusb.h>

using bench_clock = std::chrono::steady_clock;

/* Set by SIGINT, ends the read loop */
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

static const uint16_t VID = 0xCAFE;
static const uint16_t PID = 0x4011;
static const int STREAM_ITF = 7;
static const unsigned char STREAM_EP_IN = 0x87;
static const uint8_t REQ_START = 0x10;
static const uint8_t REQ_STOP = 0x11;
static const uint8_t REQ_INFO = 0x12;
static const uint8_t FRAME_SID = 0x53;
static const int FRAME_SIZE = 63;
static const uint8_t REQTYPE_OUT = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_INTERFACE;
static const uint8_t REQTYPE_IN = LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_INTERFACE;

/**
 * @brief: Read a little endian 16 bit value.
 */
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

/**
 * @brief: Read a little endian 32 bit value.
 */
static uint32_t le32(const uint8_t *p)
{
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/**
 * @brief: Query and print STREAM_REQUEST_INFO.
 *
 * @param h: open device handle
 * @param label: print prefix
 * @return: session byte, -1 on failure
 */
static int print_info(libusb_device_handle *h, const char *label)
{
  uint8_t info[16] = {0};
  int r = libusb_control_transfer(h, REQTYPE_IN, REQ_INFO, 0, STREAM_ITF, info, sizeof(info), 500);
  if (r < 0) {
    std::printf("%s: INFO failed: %s (firmware without stream?)\n", label, libusb_error_name(r));
    return -1;
  }
  std::printf("%s: version %u, frame %u B, max %u Hz, SIDs %u, running %u, rate %u Hz, "
    "mask 0x%X, session %u, dropped %u\n",
    label, info[0], info[1], le16(&info[2]), info[4], info[5] & 1, le16(&info[6]),
    info[8], info[10], le32(&info[12]));
  return info[10];
}

int main(int argc, char **argv)
{
  int rate = 0, seconds = 5, mask = 0;
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "-r") && i + 1 < argc) rate = std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "-s") && i + 1 < argc) seconds = std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "-m") && i + 1 < argc) mask = (int)std::strtol(argv[++i], NULL, 0);
    else {
      std::fprintf(stderr, "Usage: %s [-r rate] [-s seconds] [-m sidmask]\n", argv[0]);
      return 2;
    }
  }

  libusb_context *ctx = NULL;
  if (libusb_init(&ctx) != 0) return 1;
  libusb_device_handle *h = libusb_open_device_with_vid_pid(ctx, VID, PID);
  if (h == NULL) {
    std::fprintf(stderr, "No USBSID-Pico found\n");
    libusb_exit(ctx);
    return 1;
  }
  int r = libusb_claim_interface(h, STREAM_ITF);
  if (r < 0) {
    std::fprintf(stderr, "Claim interface %d failed: %s\n", STREAM_ITF, libusb_error_name(r));
    libusb_close(h);
    libusb_exit(ctx);
    return 1;
  }

  std::signal(SIGINT, on_sigint);
  print_info(h, "before");
  uint8_t start[4] = {(uint8_t)(rate & 0xFF), (uint8_t)(rate >> 8), (uint8_t)mask, 0};
  r = libusb_control_transfer(h, REQTYPE_OUT, REQ_START, 0, STREAM_ITF, start, sizeof(start), 500);
  if (r < 0) std::printf("START failed: %s (unsupported rate stalls)\n", libusb_error_name(r));
  const int session = print_info(h, "started");

  uint8_t regs[4][32] = {{0}};
  long frames[4] = {0, 0, 0, 0}, stale = 0, gaps = 0, total = 0, other_owner = 0, late = 0;
  int last_seq[4] = {-1, -1, -1, -1};
  uint32_t first_cyc = 0, last_cyc = 0, last_rx = 0;
  auto t_end = bench_clock::now() + std::chrono::seconds(seconds);
  auto t_report = bench_clock::now() + std::chrono::seconds(1);
  while (!g_stop && bench_clock::now() < t_end) {
    uint8_t f[64];
    int got = 0;
    r = libusb_bulk_transfer(h, STREAM_EP_IN, f, sizeof(f), &got, 100);
    if (r == 0 && got == FRAME_SIZE && f[0] == FRAME_SID) {
      if (f[48] != (uint8_t)session) {
        stale++;
      } else {
        const int sid = f[1] & 0x03;
        const int seq = le16(&f[2]);
        if (last_seq[sid] >= 0 && ((last_seq[sid] + 1) & 0xFFFF) != seq) gaps++;
        last_seq[sid] = seq;
        if (f[1] & 0x40) other_owner++;
        if (f[1] & 0x20) late++;
        last_cyc = le32(&f[4]);
        if (total == 0) first_cyc = last_cyc;
        last_rx = le32(&f[8]);
        std::memcpy(regs[sid], &f[12], 32);
        frames[sid]++;
        total++;
      }
    } else if (r != 0 && r != LIBUSB_ERROR_TIMEOUT) {
      std::printf("bulk IN: %s\n", libusb_error_name(r));
      break;
    }
    if (bench_clock::now() >= t_report) {
      t_report += std::chrono::seconds(1);
      std::printf("frames SID1-4 %ld %ld %ld %ld, seq gaps %ld, stale %ld, late %ld, "
        "non USB owner %ld, rx packets %u\n",
        frames[0], frames[1], frames[2], frames[3], gaps, stale, late, other_owner, last_rx);
    }
  }

  libusb_control_transfer(h, REQTYPE_OUT, REQ_STOP, 0, STREAM_ITF, NULL, 0, 500);
  print_info(h, "stopped");
  if (total > 1) {
    std::printf("PHI1 cycles first to last frame: %u over %ld frames\n", last_cyc - first_cyc, total);
  }
  for (int s = 0; s < 4; s++) {
    if (frames[s] == 0) continue;
    std::printf("SID%d:", s + 1);
    for (int i = 0; i < 32; i++) std::printf("%s%02X", (i % 8) ? " " : "  ", regs[s][i]);
    std::printf("\n");
  }

  libusb_release_interface(h, STREAM_ITF);
  libusb_close(h);
  libusb_exit(ctx);
  return 0;
}
