package usbsid;

import java.io.*;
import java.util.List;
import java.util.logging.Logger;

import javax.usb.*;
import javax.usb.util.*;

import org.apache.commons.lang3.ObjectUtils.Null;


public class Device {

  private static byte US_CFG = 1;
  private static byte US_ITF = 1;
  private static byte US_EPOUT = (byte)0x02;
  private static byte US_EPIN = (byte)0x82;
  private static short VENDOR_ID = (short)0xCAFE;
  private static short PRODUCT_ID = (short)0x4011;

  private static boolean us_isOpen = false;
  private static UsbPipe pipe = null;
  public static UsbDevice device = null;
  private static UsbConfiguration configuration = null;
  private static UsbInterface iface = null;
  private static UsbEndpoint endpoint = null;

/* BYTE 0 - top 2 bits */
  static byte WRITE        =   0;   /*        0b0 ~ 0x00 */
  static byte READ         =   1;   /*        0b1 ~ 0x40 */
  static byte CYCLED_WRITE =   2;   /*       0b10 ~ 0x80 */
  static byte COMMAND      =   3;   /*       0b11 ~ 0xC0 */
  /* BYTE 0 - lower 6 bits for byte count */
  /* BYTE 0 - lower 6 bits for Commands */
  static byte PAUSE        =  10;   /*     0b1010 ~ 0x0A */
  static byte UNPAUSE      =  11;   /*     0b1011 ~ 0x0B */
  static byte MUTE         =  12;   /*     0b1100 ~ 0x0C */
  static byte UNMUTE       =  13;   /*     0b1101 ~ 0x0D */
  static byte RESET_SID    =  14;   /*     0b1110 ~ 0x0E */
  static byte DISABLE_SID  =  15;   /*     0b1111 ~ 0x0F */
  static byte ENABLE_SID   =  16;   /*    0b10000 ~ 0x10 */
  static byte CLEAR_BUS    =  17;   /*    0b10001 ~ 0x11 */
  static byte CONFIG       =  18;   /*    0b10010 ~ 0x12 */
  static byte RESET_MCU    =  19;   /*    0b10011 ~ 0x13 */
  static byte BOOTLOADER   =  20;   /*    0b10100 ~ 0x14 */

  static long DEFAULT = 1000000;  /* 1 MHz     = 1 us */
  static long PAL     = 985248;   /* 0.985 MHz = 1.014973 us */
  static long NTSC    = 1022727;  /* 1.023 MHz = 0.977778 us */
  static long DREAN   = 1023440;  /* 1.023 MHz = 0.977097 us */
  static long NTSC2   = 1022730;  /* 1.023 MHz = 0.977778 us */

  public static boolean isOpen()
  {
    return us_isOpen;
  }

  public static void open_USBSID()
    throws UsbException
  {
    device = find_USBSID(
        UsbHostManager.getUsbServices().getRootUsbHub());
    if (device == null)
    {
        System.err.println("USBSID-Pico not found");
//        System.exit(1);
        return;
    }
    configuration = device.getUsbConfiguration(US_CFG);
    iface = configuration.getUsbInterface(US_ITF);
    // System.out.println(configuration.toString());
    // System.out.println(iface.toString());
    iface.claim(new UsbInterfacePolicy()
    {
      @Override
      public boolean forceClaim(UsbInterface usbInterface)
      {
        return true;
      }
    });
    endpoint = iface.getUsbEndpoint(US_EPOUT);
    pipe = endpoint.getUsbPipe();
    pipe.open();
    us_isOpen = true;
  }

  public static void close_USBSID()
    throws UsbException
  {
    if (pipe != null) pipe.close();
    try {
      if (iface.isClaimed()) iface.release();
    } catch (javax.usb.UsbPlatformException uPE) {
      System.out.println("Device not found for re-attaching kernel, skipping.");
    }
  }

  public static UsbDevice find_USBSID(UsbHub hub)
    throws UsbException
  {
    UsbDevice usbsidpico = null;
    for (UsbDevice device: (List<UsbDevice>) hub.getAttachedUsbDevices())
      {
        if (device.isUsbHub())
        {
          usbsidpico = find_USBSID((UsbHub) device);
          if (usbsidpico != null) return usbsidpico;
        }
        else
        {
          UsbDeviceDescriptor desc = device.getUsbDeviceDescriptor();
          if (desc.idVendor() == VENDOR_ID &&
              desc.idProduct() == PRODUCT_ID) return device;
        }
      }
      return null;
  }

  public static void asyncWrite(byte[] buffer)
    throws UsbException
  {
    try {
      pipe.asyncSubmit(buffer);
    } catch (javax.usb.UsbDisconnectedException UDE) {
      System.out.println("USBSID was already disconnected");
    }
  }

  public static void syncWrite(byte[] buffer)
    throws UsbException
  {
    // System.out.println("Computer says brrrrr...");
    pipe.syncSubmit(buffer);
  }

  public static void sendCommand(byte command)
    throws UsbException
  {
    byte[] message = new byte[3];
    message[0] = (byte)((COMMAND << 6) | (command)); /* config command */
    try {
      int sent = pipe.syncSubmit(message);
      System.out.println("Command: " + sent + " bytes sent");
    } catch (javax.usb.UsbDisconnectedException UDE) {
      System.out.println("USBSID was already disconnected");
    }
  }

  public static void sendConfigCommand(
    int command,
    int a, int b, int c, int d)
    throws UsbException
  {

    byte[] message = new byte[6];
    message[0] = (byte) ((COMMAND << 6) | CONFIG); /* config command */
    message[1] = (byte) (command);
    message[2] = (byte) (a);
    message[3] = (byte) (b);
    message[4] = (byte) (c);
    message[5] = (byte) (d);
    int sent = pipe.syncSubmit(message);
    System.out.println("Config command: " + sent + " bytes sent");
  }

}
