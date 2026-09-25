/*
 * C API test for driver-repo/src, built as strict C by test/ci/build.sh.
 * Exercises USBSIDInterface.h and USBSIDManagerInterface.h without opening
 * a board: no init_USBSID() or openall_USBSIDMgr() calls.
 */

#include <stdio.h>
#include <string.h>

#include "USBSIDInterface.h"
#include "USBSIDManagerInterface.h"

static int failures = 0;

/**
 * @brief: Report a failed check.
 *
 * @param ok: check result
 * @param what: description printed on failure
 */
static void check(int ok, const char *what)
{
  if (!ok) {
    printf("FAIL: %s\n", what);
    failures++;
  }
}

int main(void)
{
  USBSIDitf us;
  USBSIDmgr mgr;
  USBSIDdevinfo devs[4];
  USBSIDboardinfo board;
  USBSIDlogicalslot slot;
  uint8_t cfg[USBSID_SOCKETCONFIG_LEN];
  char serial[USBSID_SERIAL_LEN];
  int found;

  /* NULL handle */
  check(init_USBSID(NULL, false, false) == -1, "init NULL");
  check(!initialised_USBSID(NULL), "initialised NULL");
  check(!available_USBSID(NULL), "available NULL");
  check(!portisopen_USBSID(NULL), "portisopen NULL");
  check(getrefreshrate_USBSID(NULL) == 0, "refreshrate NULL");
  check(getrasterrate_USBSID(NULL) == 0, "rasterrate NULL");
  check(getserial_USBSID(NULL, serial, sizeof serial) == -1, "getserial NULL");
  check(!getsocketconfig_USBSID(NULL, cfg), "socketconfig NULL");
  check(waitforcycle_USBSID(NULL, 10) == 0, "waitforcycle NULL");
  resetringbuffer_USBSID(NULL);
  close_USBSID(NULL);

  /* Unopened handle */
  us = create_USBSID();
  check(us != NULL, "create");
  check(initialised_USBSID(us), "initialised");
  check(!portisopen_USBSID(us), "not open");
  check(getrefreshrate_USBSID(us) > 0, "refreshrate");
  check(getrasterrate_USBSID(us) > 0, "rasterrate");
  check(getclockrate_USBSID(us) == 0, "clockrate without connection");
  settargetserial_USBSID(us, "CI-NO-SUCH-SERIAL");
  settargetserial_USBSID(us, NULL);
  settargetindex_USBSID(us, 0);
  memset(serial, 'x', sizeof serial);
  check(getserial_USBSID(us, serial, sizeof serial) == 0 && serial[0] == '\0',
        "empty serial before open");
  check(getserial_USBSID(us, serial, 0) == -1, "getserial zero length");
  check(!getsocketconfig_USBSID(us, cfg), "socketconfig without connection");
  check(getsocketnumsids_USBSID(us, 1, NULL) == 0, "socketnumsids NULL cfg");
  check(waitforcycle_USBSID(us, 1000) > 0, "waitforcycle");
  close_USBSID(us);

  /* Enumeration opens nothing for I/O, zero boards on CI */
  found = enumerate_USBSID(NULL, 0);
  check(found >= 0, "enumerate count");
  check(enumerate_USBSID(NULL, 1) == -1, "enumerate NULL out");
  check(enumerate_USBSID(devs, 4) == found, "enumerate fill");

  /* Manager without boards */
  check(!openall_USBSIDMgr(NULL, NULL, 0, false, false), "openall NULL");
  check(totalsids_USBSIDMgr(NULL) == 0, "totalsids NULL");
  close_USBSIDMgr(NULL);
  mgr = create_USBSIDMgr();
  check(mgr != NULL, "mgr create");
  check(!openall_USBSIDMgr(mgr, NULL, 1, false, false), "openall NULL serials");
  check(totalsids_USBSIDMgr(mgr) == 0, "mgr totalsids");
  check(boardcount_USBSIDMgr(mgr) == 0, "mgr boardcount");
  check(!getboardinfo_USBSIDMgr(mgr, 0, &board), "mgr boardinfo");
  check(!getlogicalslot_USBSIDMgr(mgr, 0, &slot), "mgr logicalslot");
  writering_USBSIDMgr(mgr, 0, 0x18, 0x0F);
  writeringcycled_USBSIDMgr(mgr, -1, 0x18, 0x0F, 8);
  check(read_USBSIDMgr(mgr, 0, 0x1B) == 0, "mgr read");
  flushall_USBSIDMgr(mgr);
  muteall_USBSIDMgr(mgr);
  unmuteall_USBSIDMgr(mgr);
  closeall_USBSIDMgr(mgr);
  close_USBSIDMgr(mgr);

  printf("c_api_test: %d board(s) attached, %s\n", found,
         failures ? "FAILED" : "OK");
  return failures ? 1 : 0;
}
