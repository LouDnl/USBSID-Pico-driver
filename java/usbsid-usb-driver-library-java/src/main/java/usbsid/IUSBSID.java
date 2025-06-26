package usbsid;

public interface IUSBSID {

  int USBSID_init(Integer...buffsize);
  int USBSID_init();

  void USBSID_exit();

  void USBSID_clkdwrite(byte addr, byte data, short cycles);

  void USBSID_writeclkdbuffer(byte addr, byte data, short cycles);

  void USBSID_setflush();

  void USBSID_reset(byte volume);

  void USBSID_setclock(double CpuClock);

  void USBSID_setstereo(int stereo);

  byte[] USBSID_getsocketconfig();

  int USBSID_getsocketsidtype(int socket, int sidno, byte[] socket_config);

  int USBSID_getnumsids();

  int USBSID_getpcbversion();

  byte[] USBSID_getfwversion();

  void USBSID_delay(short cycles);

}
