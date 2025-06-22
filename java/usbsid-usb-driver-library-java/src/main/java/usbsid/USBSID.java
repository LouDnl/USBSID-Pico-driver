package usbsid;

import javax.usb.UsbException;

public class USBSID extends Device implements IUSBSID {

  private volatile boolean run_thread = false;
  private volatile byte[] ring_buffer;
  private volatile int ring_read, ring_write;
  private byte[] thread_buffer = new byte[64];
  private byte buffer_pos = 1;

  private volatile boolean flush_buffer = false;
  private static long cpufrequency = DEFAULT;

  private long last_time = System.nanoTime();

  private void RingBuffer(int size) {
    ring_read = ring_write = 0;
    ring_buffer = new byte[size];
  }
  private void put(byte item) {
    ring_buffer[ring_write] = item;
    ring_write = (ring_write + 1) % ring_buffer.length;
  }
  private byte get() {
    byte item = ring_buffer[ring_read];
    ring_read = (ring_read + 1) % ring_buffer.length;
    return item;
  }
  private boolean empty() {
    return (ring_read == 0 && ring_write == 0);
  }
  private boolean higher() {
    return (ring_read < ring_write);
  }
  private int diff() {
    int d = (higher() ? (ring_read - ring_write) : (ring_write - ring_read));
    return ((d < 0) ? (d * -1) : d);
  }

  private Thread USBSID_Thread = new Thread(new Runnable() {
		@Override
		public void run() {
			try {
        System.out.printf("[USBSID] Thread started\n");
				while (run_thread) {
          if ((ring_read != ring_write) || flush_buffer) {
            if (diff() > 256 || (flush_buffer && !empty()) ) {
              if (flush_buffer) {
                USBSID_flushbuffer();
              } else {
                USBSID_fillbuffer();
              }
            }
          }
				}
			} catch (UsbException e) {
				e.printStackTrace();
			}
		}
	});

  private void USBSID_flushbuffer() throws UsbException
  {
    try {
      thread_buffer[0] = (byte)((CYCLED_WRITE << 6) | (buffer_pos - 1));
      asyncWrite(thread_buffer);
      flush_buffer = false;
      buffer_pos = 1;
    } catch (UsbException uE) {
      System.err.println("[USBSID] Exception occured: " + uE);
      uE.printStackTrace();
      throw uE;
    }
  }

  private void USBSID_fillbuffer() throws UsbException
  {
    try {
      thread_buffer[buffer_pos++] = get(); // addr
      thread_buffer[buffer_pos++] = get(); // data
      thread_buffer[buffer_pos++] = get(); // cycles_hi;
      thread_buffer[buffer_pos++] = get(); // cycles_lo;
      if (buffer_pos == 61 || flush_buffer) {
        thread_buffer[0] = (byte)((CYCLED_WRITE << 6) | (buffer_pos - 1));
        asyncWrite(thread_buffer);
        flush_buffer = false;
        buffer_pos = 1;
      }
    } catch (UsbException uE) {
      System.err.println("[USBSID] Exception occured: " + uE);
      uE.printStackTrace();
      throw uE;
    }
  }

  @Override
  public int USBSID_init()
  {
    try {
      if (device != null && isOpen()) {
        System.err.printf("[USBSID] Device is already open!\n");
        flush_buffer = true;
        return -1;
      }
      open_USBSID();
      if (isOpen()) {
        System.out.printf("[USBSID] Device opened\n");
        ring_read = ring_write = 0;
        RingBuffer(65535);
        run_thread = true;
        USBSID_Thread.setDaemon(true);
        USBSID_Thread.start();
        last_time = System.nanoTime();
        return 0;
      }
      return -1;
    } catch (UsbException uE) {
      System.err.println("[USBSID] Exception occured: " + uE);
      uE.printStackTrace();
      return -1;
    }
  }

  @Override
  public void USBSID_exit()
  {
    try {
      if (device == null || !isOpen()) {
        System.err.printf("Device is not open!\n");
        return;
      }
      USBSID_flushbuffer();
      ring_read = ring_write = 0;
      run_thread = false;
      USBSID_Thread.join();
      System.out.printf("USBSID thread stopped\n");
      close_USBSID();
      System.out.printf("USBSID-Pico closed\n");
      return;
    } catch (UsbException | InterruptedException uE) {
      System.err.println("[USBSID] Exception occured: " + uE);
      uE.printStackTrace();
      return;
    }
  }

  @Override
  public void USBSID_reset(byte volume)
  {
    try {
      if (device == null || !isOpen()) {
        System.err.printf("[USBSID] Device is not open!\n");
        return;
      }
      // if ((byte)volume == 0) sendCommand(MUTE);
      // if ((byte)volume != 0) sendCommand(UNMUTE);
      flush_buffer = true;
      sendCommand(RESET_SID);
    } catch (UsbException uE) {
      System.err.println("[USBSID] Exception occured: " + uE);
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
      System.err.println("[USBSID] Exception occured: " + uE);
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
    put(addr);
    put(data);
    put(cycles_hi);
    put(cycles_lo);
  }

  @Override
  public void USBSID_setflush()
  {
    try {
      flush_buffer = true;
    } catch (Exception E) {
      System.err.println("[USBSID] Exception occured: " + E);
      E.printStackTrace();
      return;
    }
  }

  @Override
  public void USBSID_setclock(double CpuClock)
  {
    try {
      System.out.printf("[USBSID] Clock requested: %f\n", CpuClock);
      cpufrequency = (long)CpuClock;
      System.out.printf("[USBSID] Clock converted: %d\n", cpufrequency);
      short freq = 0;
      if (cpufrequency == DEFAULT) freq = 0;
      if (cpufrequency == PAL) freq = 1;
      if (cpufrequency == NTSC) freq = 2;
      if (cpufrequency == DREAN) freq = 3;
      System.out.printf("[USBSID] Clock change: %d\n", freq);
      sendConfigCommand(0x50, freq, 0, 0, 0);
      flush_buffer = true;
    } catch (UsbException uE) {
      System.err.println("[USBSID] Exception occured: " + uE);
      uE.printStackTrace();
      return;
    }
  }

  // private short minCycles = 0;
  @Override
  public void USBSID_delay(short cycles)
  {
    final long cpu_cycle_in_ns = (long)((float)(1.0 / cpufrequency) * 1000000000);
    long now = System.nanoTime();
    long duration = (cycles * cpu_cycle_in_ns);
    long target_time = (last_time + duration);
    long target_delta = (target_time - now);
    long delayNs = target_delta;
    // short minCycles = (short)(((delayNs < 0) ? (delayNs / cpu_cycle_in_ns) : 0) & 0xFFFF);

    // System.out.printf("[MC]%04d [C]%04d [D]%06d [NS]%06d [N]%06d [LT]%06d [TT]%06d [TD]%06d\n",
    //    minCycles, (cycles & 0xFFFF), duration, delayNs,
    //    now, last_time, target_time, target_delta);

    if (delayNs > 999999) {
      do {
        nsleep(999999);
        delayNs -= 999999;
      } while (delayNs > 999999);
    };
    if (delayNs > 0) {
      nsleep(delayNs);
    };
    last_time = target_time;
  }

  private void nsleep(long delayNs) {
    long start = System.nanoTime();
    long end = 0;
    do {
      end = System.nanoTime();
    } while (start + delayNs >= end);
  }

}
