package usbsid;

import java.io.*;
import java.nio.ByteBuffer;
import java.nio.IntBuffer;
import java.util.Arrays;
import java.util.List;
import java.util.logging.Logger;

import org.usb4java.BufferUtils;
// import org.usb4java;
import org.usb4java.Context;
import org.usb4java.DeviceList;
import org.usb4java.LibUsb;
import org.usb4java.Device;
import org.usb4java.DeviceDescriptor;
import org.usb4java.DeviceHandle;
import org.usb4java.Interface;
import org.usb4java.EndpointDescriptor;
import org.usb4java.LibUsbException;
import org.usb4java.Transfer;
import org.usb4java.TransferCallback;

import javax.usb.UsbPipe;
import javax.usb.UsbDevice;
import javax.usb.UsbDeviceDescriptor;
import javax.usb.UsbConfiguration;
import javax.usb.UsbInterface;
import javax.usb.UsbInterfacePolicy;
import javax.usb.UsbEndpoint;
import javax.usb.UsbException;
import javax.usb.UsbHostManager;
import javax.usb.UsbHub;

// import javax.usb.util.*;


import org.apache.commons.lang3.ObjectUtils.Null;


public class USBSIDDevice {

  /* Device constants */
  private static byte US_CFG = 1;
  private static byte US_ITF = 1;
  private static byte US_EPOUT = (byte)0x02;
  private static byte US_EPIN = (byte)0x82;
  private static short VENDOR_ID = (short)0xCAFE;
  private static short PRODUCT_ID = (short)0x4011;

  /* Mutable */
  public static Object device = null;

  /* Device variables */
  private static boolean us_isUSBX = false;
  private static boolean us_isLIBUSB = false;
  private static boolean us_isAvailable = false;
  private static boolean us_isOpen = false;

  private class USBX {

    /* javax.usb */
    private static UsbPipe pipe_out = null;
    private static UsbPipe pipe_in = null;
    private static UsbDevice xdevice = null;
    private static UsbConfiguration config = null;
    private static UsbInterface iface = null;
    private static UsbEndpoint ep_out = null;
    private static UsbEndpoint ep_in = null;

    private static void openUSBX()
      throws UsbException
    {
      xdevice = findUSBSIDX(
          UsbHostManager.getUsbServices().getRootUsbHub());
      if (xdevice == null)
      {
          System.err.printf("USBSID-Pico not found\n");
          return;
      }
      device = xdevice;
      config = xdevice.getUsbConfiguration(US_CFG);
      iface = config.getUsbInterface(US_ITF);
      iface.claim(new UsbInterfacePolicy()
      {
        @Override
        public boolean forceClaim(UsbInterface usbInterface)
        {
          return true;
        }
      });

      ep_out = iface.getUsbEndpoint(US_EPOUT);
      pipe_out = ep_out.getUsbPipe();
      pipe_out.open();
      ep_in = iface.getUsbEndpoint(US_EPIN);
      pipe_in = ep_in.getUsbPipe();
      pipe_in.open();
      us_isOpen = true;
    }

    private static void closeUSBX()
      throws UsbException
    {
      if (pipe_in != null) pipe_in.close();
      if (pipe_out != null) pipe_out.close();
      try {
        if (iface.isClaimed()) iface.release();
      } catch (javax.usb.UsbPlatformException uPE) {
        System.out.printf("Device not found for re-attaching kernel, skipping.\n");
      }
    }

    private static UsbDevice findUSBSIDX(UsbHub hub)
      throws UsbException
    {
      UsbDevice usbsidpico = null;
      for (UsbDevice xdevice: (List<UsbDevice>) hub.getAttachedUsbDevices())
        {
          if (xdevice.isUsbHub())
          {
            usbsidpico = findUSBSIDX((UsbHub) xdevice);
            if (usbsidpico != null) return usbsidpico;
          }
          else
          {
            UsbDeviceDescriptor desc = xdevice.getUsbDeviceDescriptor();
            if (desc.idVendor() == VENDOR_ID &&
                desc.idProduct() == PRODUCT_ID) return xdevice;
          }
        }
        return null;
    }

    private static void asyncWriteX(byte[] buffer)
      throws UsbException
    {
      try {
        pipe_out.asyncSubmit(buffer);
      } catch (javax.usb.UsbDisconnectedException UDE) {
        System.out.printf("[USBSID] was already disconnected\n");
      }
    }

    private static void syncWriteX(byte[] buffer)
      throws UsbException
    {
      try {
        pipe_out.syncSubmit(buffer);
      } catch (javax.usb.UsbDisconnectedException UDE) {
        System.out.printf("[USBSID] was already disconnected\n");
      }
    }

    private static byte[] syncReadX()
      throws UsbException
    {
      byte[] data = new byte[64];
      try {
        /* int received =  */
        pipe_in.syncSubmit(data);
        /* if (received == expected_bytes)  */
      } catch (javax.usb.UsbDisconnectedException UDE) {
        System.out.printf("[USBSID] was already disconnected\n");
      }
      return data;
    }

  }

  private class USBL {  /* TODO: FINISH */

    /* usb4java.libusb */
    private static Context ctx_lusb = null;
    private static DeviceList list_lusb = null;
    private static Device device_lusb = null;
    private static DeviceHandle handle_lusb = null;
    private static Transfer transfer_out = null;
    private static ByteBuffer out_buffer = null;

    private static boolean kernel_isDetached = false;

    private static void openLIBUSB()
      throws LibUsbException
    {
      ctx_lusb = new Context();
      int result = LibUsb.init(ctx_lusb);
      if (result != LibUsb.SUCCESS) throw new LibUsbException("[USBSID] Unable to initialize libusb.", result);
      final int LIBUSB_OPTION_LOG_LEVEL = 0;
      final int LIBUSB_OPTION_USE_USBDK = 1;
      LibUsb.setOption(ctx_lusb, LIBUSB_OPTION_LOG_LEVEL, 0); /* 4 for max verbose logging */
      LibUsb.setOption(ctx_lusb, LIBUSB_OPTION_USE_USBDK, 1);

      device_lusb = findLUSBSID(VENDOR_ID, PRODUCT_ID);
      // System.out.println("DEVICE: " + device_lusb);
      // System.out.println("us_isAvailable: " + us_isAvailable);
      if ((device_lusb != null) && us_isAvailable) {
        try {
          handle_lusb = LibUsb.openDeviceWithVidPid(ctx_lusb, VENDOR_ID, PRODUCT_ID);
          // handle_lusb = new DeviceHandle();
          // result = LibUsb.open(device_lusb, handle_lusb);
          // if (result != LibUsb.SUCCESS) throw (new LibUsbException("Unable to open USB device", result));
          // System.out.println("HANDLE A: " + handle_lusb);
          // System.out.println("HANDLE E: " + new LibUsbException("Unable to open USB device", result));
        } catch (LibUsbException LUE) {
          System.out.println(new LibUsbException("[USBSID] Opening device unsuccessful, trying a different method", result));
        }
      } else {
        device_lusb = null;
        us_isAvailable = false;
        return;
      }
      // if (handle_lusb == null) handle_lusb = LibUsb.openDeviceWithVidPid(ctx_lusb, VENDOR_ID, PRODUCT_ID);
      // System.out.println("HANDLE: " + handle_lusb);
      // handle_lusb = new DeviceHandle();
      // result = LibUsb.open(device_lusb, handle_lusb);
      // if (result != LibUsb.SUCCESS) throw new LibUsbException("[USBSID] Unable to open USB device", result);

      // checkKernel(); /* TODO: Move back to this */

      if (LibUsb.kernelDriverActive(handle_lusb, 0) == 1) {
        try {
          result = LibUsb.detachKernelDriver(handle_lusb, 1);
          if (result != LibUsb.SUCCESS || result != LibUsb.ERROR_NOT_SUPPORTED) throw new LibUsbException("[USBSID] Unable to claim interface", result);
        } catch (LibUsbException LUE) {
          System.out.println("[USBSID] Detaching kerneldriver for interface 0 result: " + LUE.getMessage());
        }
      }
      try {
        result = LibUsb.claimInterface(handle_lusb, 0);
        if (result != LibUsb.SUCCESS) throw new LibUsbException("[USBSID] Unable to claim interface", result);
      } catch (LibUsbException LUE) {
        System.out.println("[USBSID] Claiming interface 0 result: " + LUE.getMessage());
      }
      if (LibUsb.kernelDriverActive(handle_lusb, 1) == 1) {
        try {
          result = LibUsb.detachKernelDriver(handle_lusb, 1);
          if (result != LibUsb.SUCCESS) throw new LibUsbException("[USBSID] Unable to claim interface", result);
        } catch (LibUsbException LUE) {
          System.out.println("[USBSID] Detaching kerneldriver for interface 1 result: " + LUE.getMessage());
        }
      }
      try {
        result = LibUsb.claimInterface(handle_lusb, 1);
        if ((result != LibUsb.SUCCESS)) throw new LibUsbException("[USBSID] Unable to claim interface", result);
      } catch (LibUsbException LUE) {
        System.out.println("[USBSID] Claiming interface 1 result: " + LUE.getMessage());
      }

      try {
        short ACM_CTRL_DTR = 0x01;
        short ACM_CTRL_RTS = 0x02;
        ByteBuffer buffer = ByteBuffer.allocateDirect(0);
        result = LibUsb.controlTransfer(handle_lusb, (byte)0x21, (byte)0x22, (short)(ACM_CTRL_DTR | ACM_CTRL_RTS), (short)0, buffer, (long)100);
        // System.out.println("CONFIG 1: " + result);
        // ByteBuffer config = ByteBuffer.allocateDirect(7);
        // config.put(new byte[] { 0x40, 0x54, (byte)0x89, 0x00, 0x00, 0x00, 0x08 });
        // result = LibUsb.controlTransfer(handle_lusb, (byte)0x21, (byte)0x22, (short)0, (short)0, config, (long)100);
        // System.out.println("CONFIG 2: " + result);
      } catch (LibUsbException LUE) {
        System.out.println("CONFIG ERROR: " + result);

      }

      initOutBuffer();
      us_isOpen = true;
      device = device_lusb;

      // System.out.println("handle_lusb:" + handle_lusb.equals(handle_lusb));
      // System.out.println("device_lusb:" + device_lusb.equals(device_lusb));
      // System.out.println("device:" + device.equals(device_lusb));

    }

    private static void closeLIBUSB()
      throws LibUsbException
    {
      us_isOpen = false;
      deinitOutBuffer();
      int result = LibUsb.releaseInterface(handle_lusb, US_ITF);
      if (result != LibUsb.SUCCESS) throw new LibUsbException("[USBSID] Unable to release interface", result);
      if (kernel_isDetached) {
        attachKernel();
      }
      LibUsb.exit(ctx_lusb);
    }

    private static Device findLUSBSID(short vendorId, short productId)
      throws LibUsbException
    {
      System.out.printf("FIND %04X %04X\n", (vendorId & 0xFFFF), (productId & 0xFFFF));
      // Read the USB device list
      list_lusb = new DeviceList();
      int result = LibUsb.getDeviceList(ctx_lusb, list_lusb);
      if (result < 0) throw new LibUsbException("Unable to get device list", result);

      try
      {
        // Iterate over all devices and scan for the right one
        for (Device d: list_lusb)
        {
          DeviceDescriptor descriptor = new DeviceDescriptor();
          result = LibUsb.getDeviceDescriptor(d, descriptor);
          System.out.printf("%04X %04X\n", (descriptor.idVendor() & 0xFFFF), (descriptor.idProduct() & 0xFFFF));
          if (result != LibUsb.SUCCESS) throw new LibUsbException("Unable to read device descriptor", result);
          if (descriptor.idVendor() == vendorId && descriptor.idProduct() == productId) {
            System.out.printf("FOUND %04X %04X\n", (descriptor.idVendor() & 0xFFFF), (descriptor.idProduct() & 0xFFFF));
            us_isAvailable = true;
            return d;
          }
        }
      }
      finally
      {
      //   // Ensure the allocated device list is freed
        LibUsb.freeDeviceList(list_lusb, true);
        // System.out.println("Poo");
      }
      return null;
    }

    private static void initOutBuffer()
    {
      try {
        out_buffer = LibUsb.devMemAlloc(handle_lusb, 64);
        System.out.println("devMemAlloc out_buffer: " + out_buffer);
        if (out_buffer == null) throw new Exception("[USBSID] LibUsb.devMemAlloc failed", null);
      } catch (Exception e) {
        System.out.println("[USBSID] Unable to use devMemAlloc, allocating default");
        out_buffer = BufferUtils.allocateByteBuffer(64);
        System.out.println("BufferUtils out_buffer: " + out_buffer);
      }
      transfer_out = LibUsb.allocTransfer();
      System.out.println("transfer_out: " + transfer_out);
      LibUsb.fillBulkTransfer(transfer_out, handle_lusb, US_EPOUT, out_buffer, usb_out, null, 0);
      System.out.println("transfer_out: " + transfer_out);
    }

    private static void deinitOutBuffer()
    {
      LibUsb.cancelTransfer(transfer_out);
      LibUsb.freeTransfer(transfer_out);
      try {
        LibUsb.devMemFree(handle_lusb, out_buffer, 64);
      } catch (Exception e) {
        System.out.println("[USBSID] Unable to use devMemFree, freeing default");
        out_buffer = null;
      }
    }

    private static void detachKernel(int itf)
    {
      int result = LibUsb.detachKernelDriver(handle_lusb, itf);
      if (result != LibUsb.SUCCESS || result != LibUsb.ERROR_NOT_SUPPORTED) throw new LibUsbException("[USBSID] Unable to detach kernel driver", result);
      System.out.println("DETACH: " + result);
      kernel_isDetached = true;
    }

    private static void attachKernel()
    {
      int result = LibUsb.attachKernelDriver(handle_lusb, US_ITF);
      if (result != LibUsb.SUCCESS) throw new LibUsbException("[USBSID] Unable to re-attach kernel driver", result);
      kernel_isDetached = false;
    }

    private static boolean checkKernel()
    {
      // Check if kernel driver must be detached
      boolean cap = LibUsb.hasCapability(LibUsb.CAP_SUPPORTS_DETACH_KERNEL_DRIVER);
      int result = 0;
      try {
        result = LibUsb.kernelDriverActive(handle_lusb, 0);
        detachKernel(0);
        if (result != LibUsb.SUCCESS || result != LibUsb.ERROR_NOT_SUPPORTED) throw new LibUsbException("[USBSID] Unable to check for active kernel driver", result);
      } catch (LibUsbException LUE) {
        System.out.println(cap + " " + result);
      }
      try {
        result = LibUsb.kernelDriverActive(handle_lusb, 1);
        detachKernel(1);
        if (result != LibUsb.SUCCESS || result != LibUsb.ERROR_NOT_SUPPORTED) throw new LibUsbException("[USBSID] Unable to check for active kernel driver", result);
      } catch (LibUsbException LUE) {
        System.out.println(cap + " " + result);
      }
      boolean check = (cap && result == 0);

      return check;
    }

    private static void asyncLWrite(byte[] buffer)
    {
      // ByteBuffer b = BufferUtils.allocateByteBuffer(buffer.length);
      // System.out.println("B: " + out_buffer.capacity() + " " + buffer.length);
      // System.out.println("P2: " + out_buffer.limit() + " " + out_buffer.position());
      // out_buffer.put(buffer, 0, buffer.length);
      out_buffer.put(buffer);
      // System.out.println("P2: " + out_buffer.limit() + " " + out_buffer.position());
      // Transfer transfer = LibUsb.allocTransfer();
      // LibUsb.fillBulkTransfer(transfer, handle_lusb, US_EPOUT, b, callback, null, 0);
      int result = LibUsb.submitTransfer(transfer_out);
      LibUsb.handleEventsCompleted(ctx_lusb, null);
      // if (result != LibUsb.SUCCESS) throw new LibUsbException("[USBSID] Transfer failed", result);
      if (result != LibUsb.SUCCESS) System.err.println(new LibUsbException("[USBSID] Transfer failed", result));
      out_buffer.position(0);
    }

    private static void syncLWrite(byte[] buffer)
      // throws LibUsbException
    {
      // try {
      // System.out.println("handle_lusb:" + handle_lusb.equals(handle_lusb));
      // System.out.println("device_lusb:" + device_lusb.equals(device_lusb));
      // System.out.println("device:" + device.equals(device_lusb));
      ByteBuffer b = ByteBuffer.allocateDirect(buffer.length);
      b.put(buffer);
      IntBuffer transfered = IntBuffer.allocate(1);
      // System.out.println("HANDLE syncLWrite: " + handle_lusb);
      // System.out.println("HANDLE b: " + b);
      // System.out.println("HANDLE transfered: " + transfered);
      int result = LibUsb.bulkTransfer(handle_lusb, US_EPOUT, b, transfered, 0);
      if (result != LibUsb.SUCCESS) throw new LibUsbException("[USBSID] Transfer failed", result);
      result = LibUsb.handleEventsTimeout(null, 0);
      if (result != LibUsb.SUCCESS) throw new LibUsbException("[USBSID] Unable to handle events", result);
      // } catch (LibUsbException LUE) {
      //   System.out.printf("[USBSID] was already disconnected\n");
      // }
    }

    private static byte[] syncLRead()
      // throws LibUsbException
    {
      // try {
      // System.out.println("handle_lusb:" + handle_lusb.equals(handle_lusb));
      // System.out.println("device_lusb:" + device_lusb.equals(device_lusb));
      // System.out.println("device:" + device.equals(device_lusb));
      ByteBuffer buffer = ByteBuffer.allocateDirect(64);
      IntBuffer transfered = IntBuffer.allocate(1);
      // System.out.println("HANDLE syncLWrite: " + handle_lusb);
      // System.out.println("HANDLE b: " + b);
      // System.out.println("HANDLE transfered: " + transfered);
      int result = LibUsb.bulkTransfer(handle_lusb, US_EPIN, buffer, transfered, 0);
      if (result != LibUsb.SUCCESS) throw new LibUsbException("[USBSID] Transfer failed", result);
      result = LibUsb.handleEventsTimeout(null, 0);
      if (result != LibUsb.SUCCESS) throw new LibUsbException("[USBSID] Unable to handle events", result);
      // } catch (LibUsbException LUE) {
      //   System.out.printf("[USBSID] was already disconnected\n");
      // }
      byte[] b = new byte[buffer.remaining()];
      return b;
    }

    private static TransferCallback usb_out = new TransferCallback()
    {
        @Override
        public void processTransfer(Transfer transfer)
        {
          if(transfer.status() != LibUsb.TRANSFER_COMPLETED) {
            int result = transfer.status();
            if (result != LibUsb.TRANSFER_CANCELLED) {
              System.err.printf("[USBSID] Warning: transfer out interrupted with status %d, %s: %s\r", result, LibUsb.errorName(result), LibUsb.strError(result));
            }
            LibUsb.freeTransfer(transfer);
            return;
          }
          if (transfer.actualLength() != 64) { /* TODO: len out buffer */
            System.err.printf("[USBSID] Sent data length %d is different from the defined buffer length: %d or actual length %d\r", transfer.length(), 64, transfer.actualLength());
          }
        }
    };

  }

  public static boolean isOpen()
  {
    return us_isOpen;
  }

  public static boolean isWinblows()
  {
    final String OS = System.getProperty("os.name").toLowerCase();
    return OS.contains("win");
  }

  private static void setDriverType()
  {
    if (!isWinblows()) {
      us_isUSBX = true;  /* Perfect for Linux! but no worky on poor Winblows */
    } else {
      us_isLIBUSB = true;  /* Winblows fallback */
    }
  }


  /* Public woohah */

  public static void open_USBSID()
  {
    setDriverType();
    if (us_isUSBX) {
      try {
        USBX.openUSBX();
      } catch (UsbException UE) {
        System.err.println("[USBSID] Unhandled exception occured: " + UE);
        UE.printStackTrace();
      }
    } else if (us_isLIBUSB) {
      try {
        USBL.openLIBUSB();
      } catch (LibUsbException LUE) {
        System.err.println("[USBSID] Unhandled exception occured: " + LUE);
        LUE.printStackTrace();
      }
    }
  }

  public static void close_USBSID()
  {
    sendCommand(Cmd.RESET_SID.get(), (byte)0x0);
    if (us_isUSBX) {
      try {
        USBX.closeUSBX();
      } catch (UsbException UE) {
        System.err.println("[USBSID] Unhandled exception occured: " + UE);
        UE.printStackTrace();
      }
    } else if (us_isLIBUSB) {
      try {
        USBL.closeLIBUSB();
      } catch (LibUsbException LUE) {
        System.err.println("[USBSID] Unhandled exception occured: " + LUE);
        LUE.printStackTrace();
      }
    }
  }

  public static void asyncWrite(byte[] buffer)
  {
    if (us_isUSBX) {
      try {
        USBX.asyncWriteX(buffer);
      } catch (UsbException UE) {
        System.err.println("[USBSID] Unhandled exception occured: " + UE);
        UE.printStackTrace();
      }
    } else if (us_isLIBUSB) {
      try {
        USBL.asyncLWrite(buffer);
      } catch (LibUsbException LUE) {
        System.err.println("[USBSID] Unhandled exception occured: " + LUE);
        LUE.printStackTrace();
      }
    }
  }

  public static void syncWrite(byte[] buffer)
  {
    if (us_isUSBX) {
      try {
        USBX.syncWriteX(buffer);
      } catch (UsbException UE) {
        System.err.println("[USBSID] Unhandled exception occured: " + UE);
        UE.printStackTrace();
      }
    } else if (us_isLIBUSB) {
      try {
        USBL.syncLWrite(buffer);
      } catch (LibUsbException LUE) {
        System.err.println("[USBSID] Unhandled exception occured: " + LUE);
        LUE.printStackTrace();
      }
    }
  }

  public static byte[] syncRead(int len)
  {
    byte[] r = new byte[len];
    if (us_isUSBX) {
      try {
        r = USBX.syncReadX();
      } catch (UsbException UE) {
        System.err.println("[USBSID] Unhandled exception occured: " + UE);
        UE.printStackTrace();
      }
    } else if (us_isLIBUSB) {
      try {
        r = USBL.syncLRead();
      } catch (LibUsbException LUE) {
        System.err.println("[USBSID] Unhandled exception occured: " + LUE);
        LUE.printStackTrace();
      }
    }
   return r;
  }

  public static void sendCommand(byte command, Byte...subcommands)
  { /* Ignores all extra subcommands if more then 1 is supplied */
    Byte subcommand = subcommands.length > 0 ? subcommands[0] : 0;
    byte[] message = new byte[3];
    message[0] = (byte)((Cmd.COMMAND.get() << 6) | (command)); /* config command */
    message[1] = (byte)subcommand;
    syncWrite(message);
  }

  public static void sendConfigCommand(
    int command,
    Byte...args)
  {
    Byte a = args.length > 0 ? args[0] : 0;
    Byte b = args.length > 1 ? args[1] : 0;
    Byte c = args.length > 2 ? args[2] : 0;
    Byte d = args.length > 3 ? args[3] : 0;

    byte[] message = new byte[6];
    message[0] = (byte) ((Cmd.COMMAND.get() << 6) | Cmd.CONFIG.get()); /* config command */
    message[1] = (byte) (command);
    message[2] = (byte) (a);
    message[3] = (byte) (b);
    message[4] = (byte) (c);
    message[5] = (byte) (d);
    syncWrite(message);
  }

  public static byte[] rwConfigCommand(
    int command,
    int len,
    Byte...args)
  {
    Byte a = args.length > 0 ? args[0] : 0;
    Byte b = args.length > 1 ? args[1] : 0;
    Byte c = args.length > 2 ? args[2] : 0;
    Byte d = args.length > 3 ? args[3] : 0;
    byte[] message = new byte[6];
    message[0] = (byte) ((Cmd.COMMAND.get() << 6) | Cmd.CONFIG.get()); /* config command */
    message[1] = (byte) (command);
    message[2] = (byte) (a);
    message[3] = (byte) (b);
    message[4] = (byte) (c);
    message[5] = (byte) (d);
    syncWrite(message);
    return syncRead(len);
  }

}
