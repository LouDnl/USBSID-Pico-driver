/*
 * C API test for driver-repo/libusbsiddrv-vice, built as strict C by
 * test/ci/build.sh. Exercises the USBSIDInterface.h calls VICE makes,
 * without opening a board: no init_USBSID() call.
 */

#include <stdio.h>

#include "USBSIDInterface.h"

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

  /* NULL handle, only functions that check it */
  check(init_USBSID(NULL, false, false) == -1, "init NULL");
  check(getclockrate_USBSID(NULL) == 0, "clockrate NULL");
  check(getnumsids_USBSID(NULL) == -1, "numsids NULL");
  check(waitforcycle_USBSID(NULL, 10) == 0, "waitforcycle NULL");
  close_USBSID(NULL);

  /* Unopened handle, I/O calls must return without touching libusb */
  us = create_USBSID();
  check(us != NULL, "create");
  check(initialised_USBSID(us), "initialised");
  check(!portisopen_USBSID(us), "not open");
  check(getrefreshrate_USBSID(us) > 0, "refreshrate");
  check(getrasterrate_USBSID(us) > 0, "rasterrate");
  check(getclockrate_USBSID(us) == 0, "clockrate without connection");
  check(getnumsids_USBSID(us) == 0, "numsids without connection");
  check(readsingle_USBSID(us, 0x1B) == 0, "readsingle without connection");
  check(read_USBSID(us, 0x1B) == 0, "read without connection");
  check(waitforcycle_USBSID(us, 1000) > 0, "waitforcycle");
  pause_USBSID(us);
  mute_USBSID(us);
  unmute_USBSID(us);
  reset_USBSID(us);
  clearbus_USBSID(us);
  write_USBSID(us, 0x18, 0x0F);
  writecycled_USBSID(us, 0x18, 0x0F, 8);
  writering_USBSID(us, 0x18, 0x0F);
  writeringcycled_USBSID(us, 0x18, 0x0F, 8);
  flush_USBSID(us);
  enablethread_USBSID(us);
  disablethread_USBSID(us);
  close_USBSID(us);

  printf("vice_c_api_test: %s\n", failures ? "FAILED" : "OK");
  return failures ? 1 : 0;
}
