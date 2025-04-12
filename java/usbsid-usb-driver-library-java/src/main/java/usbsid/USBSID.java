package usbsid;

// import java.io.*;
// import java.util.List;
// import java.util.logging.Logger;

import javax.usb.UsbException;
// import javax.usb.*;
// import javax.usb.util.*;


public class USBSID extends Device implements IUSBSID {

  @Override
  public int USBSID_init()
  {
    try {
      if (device != null && isOpen()) {
        System.err.println("Device is already open!");
        return -1;
      }
      open_USBSID();
      System.out.println("USBSID-Pico opened");
      return 0;
    } catch (UsbException uE) {
      System.err.println("Exception occured: " + uE);
      return -1;
    }
  }

  @Override
  public void USBSID_exit()
  {
    try {
      if (device == null || !isOpen()) {
        System.err.println("Device is not open!");
        return;
      }
      close_USBSID();
      System.out.println("USBSID-Pico closed");
      return;
    } catch (UsbException uE) {
      System.err.println("Exception occured: " + uE);
      return;
    }
  }

  @Override
  public void USBSID_reset(byte volume)
  {
    try {
      sendCommand(RESET_SID);
    } catch (UsbException uE) {
      System.err.println("Exception occured: " + uE);
      return;
    }
  }

  @Override
  public void USBSID_clkdwrite(long cycles, byte addr, byte data)
  {
    try {
      byte[] buffer = new byte[5];
      byte cycles_hi = (byte)((cycles >> 8) & (byte)0xff);
      byte cycles_lo = (byte)(cycles & (byte)0xff);
      buffer[0] = (byte)(CYCLED_WRITE << 6);
      buffer[1] = (byte)addr;
      buffer[2] = (byte)data;
      buffer[3] = (byte)cycles_hi;
      buffer[4] = (byte)cycles_lo;
      asyncWrite(buffer);
    } catch (UsbException uE) {
      System.err.println("Exception occured: " + uE);
      return;
    }
  }

	private void usleep(int delayUs) {
		int delayNs = delayUs * 1000;
		long start = System.nanoTime();
		long end = 0;
		do {
			end = System.nanoTime();
		} while (start + delayNs >= end);
	}

}
