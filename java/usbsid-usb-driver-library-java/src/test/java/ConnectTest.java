package usbsid;

import usbsid.USBSID;

public class ConnectTest
{

 public static void main(String[] argv)
  {
    USBSID usbsid = new USBSID();
    usbsid.USBSID_init();
    // sendConfigCommand(0x59,0,0,0,0);
    // sendConfigCommand(0x52,0,0,0,0);
    usbsid.USBSID_exit();
  }

}
