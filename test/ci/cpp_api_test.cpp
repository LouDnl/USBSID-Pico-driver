/*
 * C++ API test for driver-repo/src, built by test/ci/build.sh.
 * Exercises USBSID.h and USBSID_Manager.h without opening a board:
 * no USBSID_Init() or OpenAll() calls.
 */

#include <cstdio>
#include <vector>

#include "USBSID.h"
#include "USBSID_Manager.h"

using namespace USBSID_NS;

static int failures = 0;

/**
 * @brief: Report a failed check.
 *
 * @param ok: check result
 * @param what: description printed on failure
 */
static void check(bool ok, const char *what)
{
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    failures++;
  }
}

int main(void)
{
  {
    USBSID_Class us;
    check(us.USBSID_isInitialised(), "initialised");
    check(!us.USBSID_isOpen(), "not open");
    check(us.USBSID_GetRefreshRate() > 0, "refreshrate");
    check(us.USBSID_GetRasterRate() > 0, "rasterrate");
    check(us.USBSID_GetClockRate() == 0, "clockrate without connection");
    us.USBSID_SetTargetSerial("CI-NO-SUCH-SERIAL");
    us.USBSID_SetTargetIndex(0);
    check(us.USBSID_GetSerial().empty(), "empty serial before open");
    uint8_t cfg[SOCKET_BUFFER_SIZE] = {0};
    check(us.USBSID_GetSocketConfig(cfg) == nullptr, "socketconfig without connection");
    check(us.USBSID_WaitForCycle(1000) > 0, "waitforcycle");
    check(us.USBSID_RingFree() == 0, "ringfree without ringbuffer");
    const uint8_t items[8] = {0x18, 0x0F, 0x00, 0x08, 0x18, 0x00, 0x00, 0x08};
    us.USBSID_WriteRingCycledN(items, 2);
    us.USBSID_SetMuted(true);
    us.USBSID_SetMuted(false);
    check(us.USBSID_Close() == 0, "close unopened");
  }

  std::vector<USBSID_DeviceInfo> found = USBSID_EnumerateDevices();
  check(USBSID_Manager::Enumerate().size() == found.size(), "enumerate");

  {
    USBSID_Manager mgr;
    check(mgr.TotalSIDs() == 0, "mgr totalsids");
    check(mgr.BoardCount() == 0, "mgr boardcount");
    check(mgr.Boards().empty(), "mgr boards");
    check(mgr.LogicalMap().empty(), "mgr logicalmap");
    mgr.WriteRing(0, 0x18, 0x0F);
    mgr.WriteRingCycled(-1, 0x18, 0x0F, 8);
    check(mgr.Read(0, 0x1B) == 0, "mgr read");
    const uint8_t items[4] = {0x18, 0x0F, 0x00, 0x08};
    mgr.WriteRingCycledN(0, items, 1);
    check(mgr.RingFreeBytes(0) == 0, "mgr ringfree unknown sid");
    mgr.FlushBoard(-1);
    mgr.SetMutedAll(true);
    mgr.SetMutedAll(false);
    mgr.FlushAll();
    mgr.MuteAll();
    mgr.UnMuteAll();
    mgr.CloseAll();
  }

  std::printf("cpp_api_test: %lu board(s) attached, %s\n", (unsigned long)found.size(),
              failures ? "FAILED" : "OK");
  return failures ? 1 : 0;
}
