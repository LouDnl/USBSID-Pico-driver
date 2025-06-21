package usbsid;

// import java.io.*;
// import java.util.List;
// import java.util.logging.Logger;

import javax.usb.UsbException;
// import javax.usb.*;
// import javax.usb.util.*;


public class USBSID extends Device implements IUSBSID {

  private volatile boolean run_thread = false;
  private byte[] ring_buffer = new byte[256];
  private volatile byte ring_read, ring_write;
  /* private Object ring_buffMtx = new Object(); */
  private byte[] thread_buffer = new byte[64];
  private byte buffer_pos = 1;

  private volatile boolean flush_buffer = false;
  private static long cpufrequency = DEFAULT;

  private Thread USBSID_Thread = new Thread(new Runnable() {
		@Override
		public void run() {
			try {
        System.out.println("USBSID thread started");
				while (run_thread) {
          // if (flush_buffer) { /* BUG: Always true!? */
            // Do something
            // flush_buffer = false;

          // }
          // System.out.println("USBSID_Thread1: ring_read: " + ring_read + " ring_write: " + ring_write);
          if (((byte)ring_read & 0xFF) != ((byte)ring_write & 0xFF)) {
            // System.out.println("USBSID_Thread2: ring_read: " + ring_read + " ring_write: " + ring_write);
            USBSID_fillbuffer();
          }
          // System.out.println("USBSID_Thread1: ring_read: " + ring_read + " ring_write: " + ring_write);
				}
        System.out.println("USBSID thread stopped");
			} catch (UsbException e) {
			// } catch (UsbException | InterruptedException e) {
				e.printStackTrace();
			}
		}
	});

  private void USBSID_fillbuffer() throws UsbException
  {
    try {
      thread_buffer[buffer_pos++] = (byte)ring_buffer[(ring_read++ & 0xFF)]; // addr
      thread_buffer[buffer_pos++] = (byte)ring_buffer[(ring_read++ & 0xFF)]; // data
      thread_buffer[buffer_pos++] = (byte)ring_buffer[(ring_read++ & 0xFF)]; // cycles_hi;
      thread_buffer[buffer_pos++] = (byte)ring_buffer[(ring_read++ & 0xFF)]; // cycles_lo;
      // System.out.println("USBSID_fillbuffer: ring_read: " + (ring_read & 0xFF) + " ring_write: " + (ring_write & 0xFF));
      if (buffer_pos == 61 || flush_buffer) {
        thread_buffer[0] = (byte)((CYCLED_WRITE << 6) | (buffer_pos - 1));
        flush_buffer = false;
        buffer_pos = 1;
        // System.out.println(thread_buffer);
        asyncWrite(thread_buffer);
        /* syncWrite(thread_buffer); */
      }
    } catch (UsbException uE) {
      System.err.println("Exception occured: " + uE);
      uE.printStackTrace();
      throw uE;
      // return;
    }
  }

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
        ring_read = ring_write = (byte)(0 & 0xFF);
        run_thread = true;
        USBSID_Thread.setDaemon(true);
        USBSID_Thread.start();
        return 0;
      }
      return -1;
    } catch (UsbException uE) {
      System.err.println("Exception occured: " + uE);
      uE.printStackTrace();
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
      USBSID_Thread.interrupt();
      ring_read = ring_write = (byte)(0 & 0xFF);
      run_thread = false;
      USBSID_reset((byte)0);
      USBSID_Thread.join();
      close_USBSID();
      System.out.println("USBSID-Pico closed");
      return;
    } catch (UsbException | InterruptedException uE) {
      System.err.println("Exception occured: " + uE);
      uE.printStackTrace();
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
      // if ((byte)volume == 0) sendCommand(MUTE);
      // if ((byte)volume != 0) sendCommand(UNMUTE);
      sendCommand(RESET_SID);
    } catch (UsbException uE) {
      System.err.println("Exception occured: " + uE);
      uE.printStackTrace();
      return;
    }
  }

  @Override
  public void USBSID_clkdwrite(byte addr, byte data, long cycles)
  {
    try {
      byte[] writebuffer = new byte[5];
      byte cycles_hi = (byte)((cycles >> 8) & (byte)0xff);
      byte cycles_lo = (byte)(cycles & (byte)0xff);
      writebuffer[0] = (byte)(CYCLED_WRITE << 6);
      writebuffer[1] = (byte)addr;
      writebuffer[2] = (byte)data;
      writebuffer[3] = (byte)cycles_hi;
      writebuffer[4] = (byte)(cycles_lo - 2); // NOTE: TEMPORARY
      asyncWrite(writebuffer);
    } catch (UsbException uE) {
      System.err.println("Exception occured: " + uE);
      uE.printStackTrace();
      return;
    }
  }

  @Override
  public void USBSID_writeclkdbuffer(byte addr, byte data, long cycles)
  {
    // cycles -= 1; /* Works for Coma Light 13 tune 4 */
    byte cycles_hi = (byte)((cycles >> 8) & (byte)0xff);
    byte cycles_lo = (byte)(cycles & (byte)0xff);
    ring_buffer[(ring_write++ & 0xFF)] = addr;
    ring_buffer[(ring_write++ & 0xFF)] = data;
    ring_buffer[(ring_write++ & 0xFF)] = cycles_hi;
    ring_buffer[(ring_write++ & 0xFF)] = cycles_lo;
    // System.out.println("USBSID_writeclkdbuffer: ring_read: " + (ring_read & 0xFF) + " ring_write: " + (ring_write & 0xFF));
  }

  @Override
  public void USBSID_setflush()
  {
    try {
      flush_buffer = true;
    } catch (Exception E) {
      System.err.println("Exception occured: " + E);
      E.printStackTrace();
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
      uE.printStackTrace();
      return;
    }
  }

  @Override
  public void USBSID_delay(short cycles)
  {
    // cycles -= 1; /* Works for Coma Light 13 tune 4 */
    long cpu_cycle_in_ns = (long)((float)(1.0 / cpufrequency) * 1000000000);
    // int delayNs = (int)((cycles - 4) * cpu_cycle_in_ns);
    int delayNs = (int)(cycles * cpu_cycle_in_ns);
    // String callingMethod = new Throwable().fillInStackTrace().getStackTrace()[1].getMethodName();
    if (cycles < 0) return;
    // System.out.println("(" + callingMethod + ") Delay cycles: " + cycles + " Delay ns: " + delayNs + " CPU Cycle NS: " + cpu_cycle_in_ns);
    if (delayNs > 999999) {
      do {
        nsleep(999999);
        delayNs -= 999999;
      } while (delayNs > 999999);
    };
    if (delayNs > 0) {
      nsleep(delayNs);
    };
  }

  private void nsleep(int delayNs) {
    // try {

    //   Thread.sleep(0, delayNs);
    // } catch (java.lang.InterruptedException IE) {
    //   System.err.println("Exception occured: " + IE);
    // }
    long start = System.nanoTime();
    long end = 0;
    do {
      end = System.nanoTime();
    // } while (start + (delayNs * 0.9) >= end);
    } while (start + delayNs >= end);
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
