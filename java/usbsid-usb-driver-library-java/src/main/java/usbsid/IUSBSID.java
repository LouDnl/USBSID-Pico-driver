package usbsid;

public interface IUSBSID {

  int USBSID_init();

  void USBSID_exit();

  void USBSID_reset(byte volume);

  void USBSID_clkdwrite(long cycles, byte addr, byte data);

  void USBSID_clkdbuffer(long cycles, byte addr, byte data);

  void USBSID_setflush();

  void USBSID_delay(short cycles);

  void USBSID_setclock(double CpuClock);

}
