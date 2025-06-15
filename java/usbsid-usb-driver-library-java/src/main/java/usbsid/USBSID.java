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
      if (isOpen()) {
        System.out.println("USBSID-Pico opened");
        return 0;
      }
      return -1;
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
      if (device == null || !isOpen()) {
        System.err.println("Device is not open!");
        return;
      }
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
  byte[] longbuffer = new byte[64];
  int flush = 0;
  static int bufn = 1;
  static long cpufrequency = DEFAULT;

  @Override
  public void USBSID_clkdbuffer(long cycles, byte addr, byte data)
  {
    try {
      byte cycles_hi = (byte)((cycles >> 8) & (byte)0xff);
      byte cycles_lo = (byte)(cycles & (byte)0xff);
      longbuffer[bufn++] = (byte)addr;
      longbuffer[bufn++] = (byte)data;
      longbuffer[bufn++] = (byte)cycles_hi;
      longbuffer[bufn++] = (byte)cycles_lo;
      if (bufn == 61 || flush == 1) {
        longbuffer[0] = (byte)((CYCLED_WRITE << 6) | (bufn - 1));
        flush = 0;
        bufn = 1;
        asyncWrite(longbuffer);
      }
    } catch (UsbException uE) {
      System.err.println("Exception occured: " + uE);
      return;
    }
  }

  @Override
  public void USBSID_setflush()
  {
    try {
      flush = 1;
    } catch (Exception E) {
      System.err.println("Exception occured: " + E);
      return;
    }
  }

  @Override
  public void USBSID_setclock(double CpuClock)
  {
    try {
      System.out.println("Clock requested: " + CpuClock);
      cpufrequency = (long)CpuClock;
      System.out.println("Clock converted: " + cpufrequency);
      short freq = 0;
      if (cpufrequency == DEFAULT) freq = 0;
      if (cpufrequency == PAL) freq = 1;
      if (cpufrequency == NTSC) freq = 2;
      if (cpufrequency == DREAN) freq = 3;
      System.out.println("Clock change: " + freq);
      sendConfigCommand(0x50, freq, 0, 0, 0);
    } catch (UsbException uE) {
      System.err.println("Exception occured: " + uE);
      return;
    }
  }

  @Override
  public void USBSID_delay(short cycles)
  {
    // int delay = (int) cycles;
    // usleep(delay);
    long cpu_cycle_in_ns = (long)((float)(1.0 / cpufrequency) * 1000000000);
    int delayNs = (int)(cycles * cpu_cycle_in_ns);
    System.out.println("Delay cycles: " + cycles + " Delay ns: " + delayNs + " CPU Cycle NS: " + cpu_cycle_in_ns);
    if (cycles < 0) return;
    if (delayNs > 999999) delayNs = 999999;
    nsleep(delayNs);
  }

  private void nsleep(int delayNs) {
    try {
      Thread.sleep(0, delayNs);
    } catch (java.lang.InterruptedException IE) {
      System.err.println("Exception occured: " + IE);
      return;
    }
    // long start = System.nanoTime();
    // long end = 0;
    // do {
      // end = System.nanoTime();
    // } while (start + delayNs >= end);
  }

  private void usleep(int delayUs) {
    int delayNs = delayUs * 1000;
    long start = System.nanoTime();
    long end = 0;
    do {
      end = System.nanoTime();
    } while (start + delayNs >= end);
  }

  // public static void main(String[] argv)
  //   throws UsbException
  // {
  //   USBSID usbsid = new USBSID();
  //   usbsid.USBSID_init();
  //   sendConfigCommand(0x59,0,0,0,0);
  //   sendConfigCommand(0x52,0,0,0,0);
  //   usbsid.USBSID_exit();
  // }
}
