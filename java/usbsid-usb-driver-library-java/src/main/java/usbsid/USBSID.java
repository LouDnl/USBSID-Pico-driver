package usbsid;

import javax.usb.UsbException;

import java.io.ByteArrayOutputStream;
import java.util.Arrays;

import usbsid.Config.CLK;
import usbsid.Config.Cfg;

// public class USBSID extends JAVAXDevice implements IUSBSID {
public class USBSID extends USBSIDDevice implements IUSBSID {

  private final byte ZERO = 0x0;

  private volatile boolean run_thread = false;
  private volatile byte[] ring_buffer;
  private volatile int ring_read, ring_write;
  private final int min_ring_diff = 16;
  private final int default_ring_diff = 64;
  private final int default_ring_diffwin = 64;
  private final int default_ring_size = 256;  /* Init default buffer size */
  private final int default_ring_sizewin = 8196;  /* Init default buffer size */
  private static int diff_size = 0;
  private static int ring_size = 0;

  private byte[] thread_buffer = new byte[64];
  private byte buffer_pos = 1;

  private volatile boolean flush_buffer = false;

  private static boolean clk_retrieved = false;
  private static int cpufrequency = CLK.DEFAULT.get();

  private long start_time = System.nanoTime();
  private long last_time;

  private void timeSync() {
    last_time = (System.nanoTime() - start_time);
  }

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
          if (flush_buffer && (buffer_pos >= 5)) {
            USBSID_flushbuffer();
          }
          if ((ring_read != ring_write)) {
            if (diff() > diff_size) {
              USBSID_fillbuffer();
            }
          }
        }
        System.out.printf("[USBSID] Thread stopped\n");
      } catch (Exception E) {
        E.printStackTrace();
      }
    }
  });

  private void USBSID_flushbuffer()
    throws Exception
  { /* Only call this from the Thread loop! */
    try {
      // System.out.printf("Flushbuffer called! @ pos: %d\n", buffer_pos);
      thread_buffer[0] = (byte)((Cmd.CYCLED_WRITE.get() << 6) | (buffer_pos - 1));
      final byte[] out_buffer = thread_buffer.clone();
      asyncWrite(out_buffer);
      Arrays.fill(thread_buffer, (byte)0);
      flush_buffer = false;
      buffer_pos = 1;
    } catch (Exception E) {
      System.err.println("[USBSID] Unhandled exception occured: " + E);
      throw E;
    }
  }

  private void USBSID_fillbuffer() throws Exception
  {
    try {
      thread_buffer[buffer_pos++] = get(); // addr
      thread_buffer[buffer_pos++] = get(); // data
      thread_buffer[buffer_pos++] = get(); // cycles_hi;
      thread_buffer[buffer_pos++] = get(); // cycles_lo;
      /* System.out.printf("[W %02d/%02d]$%02X:%02X %d\n",
        (buffer_pos - 4), (buffer_pos - 1),
        thread_buffer[buffer_pos - 4],
        thread_buffer[buffer_pos - 3],
        (thread_buffer[buffer_pos - 2] << 8 | thread_buffer[buffer_pos - 1])); */
      if (buffer_pos == 61 || flush_buffer) {
        thread_buffer[0] = (byte)((Cmd.CYCLED_WRITE.get() << 6) | (buffer_pos - 1));
        flush_buffer = false;
        buffer_pos = 1;
        final byte[] write_buffer = thread_buffer.clone();
        asyncWrite(write_buffer);
        Arrays.fill(thread_buffer, (byte)0);
      }
    } catch (Exception E) {
      System.err.println("[USBSID] Unhandled exception occured: " + E);
      throw E;
    }
  }

  @Override
  public int USBSID_init(Integer...vars)
  {
    if (device != null && isOpen()) {
      System.err.printf("[USBSID] Device is already open!\n");
      flush_buffer = true;
      return -1;
    }
    open_USBSID();
    if (isOpen()) {
      System.out.printf("[USBSID] USBSID-Pico opened\n");
      ring_read = ring_write = 0;
      if (vars.length > 0) {
        Integer rs = vars[0];
         /* 512 bytes minimum, 65535 bytes maximum */
        ring_size = ((rs >= default_ring_size) && (rs <= 65535) ? rs : default_ring_size);
      }
      if (vars.length > 1) {
        Integer ds = vars[1];
        diff_size = ((ds >= min_ring_diff) ? ds : default_ring_diff);
      }
      RingBuffer(ring_size);
      run_thread = true;
      USBSID_Thread.setDaemon(true);
      USBSID_Thread.start();
      timeSync();  // last_time = (System.nanoTime() - start_time);
      return 0;
    }
    return -1;
  }
  @Override
  public int USBSID_init()
  {
    if (isWinblows()) {
      ring_size = default_ring_sizewin;
      diff_size = default_ring_diffwin;
    } else {
      ring_size = default_ring_size;
      diff_size = default_ring_diff;
    }
    return USBSID_init(ring_size, diff_size);
  }

  @Override
  public void USBSID_exit()
  {
    try {
      if (device == null || !isOpen()) {
        System.err.printf("Device is not open!\n");
        return;
      }
      USBSID_setflush();
      ring_read = ring_write = 0;
      run_thread = false;
      USBSID_Thread.join();
      System.out.printf("[USBSID] Thread joined\n");
      close_USBSID();
      System.out.printf("[USBSID] USBSID-Pico closed\n");
      return;
    } catch (InterruptedException | NumberFormatException E) {
      System.err.println("[USBSID] Exception occured: " + E.getMessage() + E.getCause());
      E.printStackTrace();
      return;
    }
  }

  @Override
  public void USBSID_clkdwrite(byte addr, byte data, short cycles)
  {
    try {
      byte[] writebuffer = new byte[5];
      byte cycles_hi = (byte)((cycles >> 8) & (byte)0xff);
      byte cycles_lo = (byte)(cycles & (byte)0xff);
      writebuffer[0] = (byte)(Cmd.CYCLED_WRITE.get() << 6);
      writebuffer[1] = (byte)addr;
      writebuffer[2] = (byte)data;
      writebuffer[3] = (byte)cycles_hi;
      writebuffer[4] = (byte)cycles_lo;
      asyncWrite(writebuffer);
    } catch (Exception E) {
      System.err.println("[USBSID] Unhandled exception occured: " + E);
      E.printStackTrace();
      return;
    }
  }

  @Override
  public void USBSID_writeclkdbuffer(byte addr, byte data, short cycles)
  {
    /* cycles -= 1 Works for Coma Light 13 tune 4 (do this on the emulator side!) */
    byte cycles_hi = (byte)((cycles >> 8) & (byte)0xff);
    byte cycles_lo = (byte)(cycles & (byte)0xff);
    put(addr);
    put(data);
    put(cycles_hi);
    put(cycles_lo);
    /* System.out.printf("[W]$%02X:%02X %d\n", (addr & 0xFF), (data & 0xFF), (cycles & 0xFFFF)); */
  }

  @Override
  public void USBSID_setflush()
  {
    try {
      flush_buffer = true;
      timeSync();
    } catch (Exception E) {
      System.err.println("[USBSID] Unhandled exception occured: " + E);
      E.printStackTrace();
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
      /* BUG: SKPICO WILL HAVE VOLUME AT 0 DUE TO NOT RESETTING FAST ENOUGH! */
      sendCommand(Cmd.RESET_SID.get(), (byte)0x0);
      if ((byte)volume == 0) sendCommand(Cmd.MUTE.get());
      if ((byte)volume > 0) sendCommand(Cmd.UNMUTE.get());
      flush_buffer = true;
      timeSync();
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
      if (!clk_retrieved || cpufrequency == CLK.DEFAULT.get()) {
        byte[] clock = rwConfigCommand(Cfg.GET_CLOCK.get(), 1);
        cpufrequency = CLK.IDclk((int)clock[0]);
        clk_retrieved = true;
      }
      /* System.out.printf("[USBSID] Clock change requested: %.02f\n", CpuClock); */
      if ((int)CpuClock != cpufrequency) {
        cpufrequency = (int)CpuClock;
        System.out.printf("[USBSID] Clock change requested: %d\n", cpufrequency);
        byte freq = 0;
        freq = (byte)CLK.clkID(CLK.getCLK(cpufrequency));
        sendConfigCommand(Cfg.SET_CLOCK.get(), freq);
      } else {
        System.out.printf("[USBSID] Clock not changed, already at: %d\n", cpufrequency);
        flush_buffer = true;
        timeSync();
      }
    } catch (Exception E) {
      System.err.println("[USBSID] Unhandled exception occured: " + E);
      E.printStackTrace();
      return;
    }
  }

  @Override
  public void USBSID_setstereo(int stereo)
  {
    try {
      sendConfigCommand(Cfg.SET_AUDIO.get(), (byte)stereo);
    } catch (Exception E) {
      System.err.println("[USBSID] Unhandled exception occured: " + E);
      E.printStackTrace();
      return;
    }
  }

  @Override
  public byte[] USBSID_getsocketconfig()
  {
    byte[] x = new byte[1];
    return x;
  }

  @Override
  public int USBSID_getsocketsidtype(int socket, int sidno, byte[] socket_config)
  {
    return 0;
  }

  @Override
  public int USBSID_getnumsids()
  {
    int numsids = 0;
    try {
      byte[] r = rwConfigCommand(Cfg.READ_NUMSIDS.get(), 1);
      numsids = (int)r[0];
    } catch (Exception E) {
      System.err.println("[USBSID] Unhandled exception occured: " + E);
      E.printStackTrace();
    }
    return numsids;
  }

  @Override
  public int USBSID_getpcbversion()
  { /* TODO: Convert to string */
    int pcbversion = 10;
    try {
      byte[] r = rwConfigCommand(Cfg.US_PCB_VERSION.get(), 1);
      pcbversion = (int)r[0];
    } catch (Exception E) {
      System.err.println("[USBSID] Unhandled exception occured: " + E);
      E.printStackTrace();
    }
    return pcbversion;
  }

  @Override
  public byte[] USBSID_getfwversion()
  { /* TODO: Convert to string */
    byte[] result = new byte[64];
    try {
      result = rwConfigCommand(Cfg.USBSID_VERSION.get(), 64);
    } catch (Exception E) {
      System.err.println("[USBSID] Unhandled exception occured: " + E);
      E.printStackTrace();
    }
    byte[] fwversion = Arrays.copyOfRange(result, 2, (int)result[1]);
    return fwversion;
  }

  @Override
  public void USBSID_delay(short cycles)
  {
    final long cpu_cycle_in_ns = (long)((float)(1.0 / cpufrequency) * 1000000000);
    long now = (System.nanoTime() - start_time);
    long duration = (cycles * cpu_cycle_in_ns);
    long target_time = (last_time + duration);
    long target_delta = (target_time - now);

    /* System.out.printf("[CYC]%04d [DUR]%06d [NS]%06d [NOW]%06d [LT]%06d [TT]%06d [TD]%06d [CPU]%d\n",
       (cycles & 0xFFFF),
       (duration & 0xFFFFFFFFL), delayNs,
       (now & 0xFFFFFFFFL), (last_time & 0xFFFFFFFFL),
       (target_time & 0xFFFFFFFFL), target_delta, cpu_cycle_in_ns); */

    nsleep(target_delta);
    last_time = target_time;
  }

  private void nsleep(long delayNs) {
    long start = System.nanoTime();
    long end = 0;
    do {
      end = System.nanoTime();
      /* System.out.printf("%d %d %d\n", start, end, delayNs); */
    } while (start + delayNs >= end);
  }

}
