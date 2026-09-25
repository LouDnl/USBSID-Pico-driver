/*
 * USBSID-Pico is a RPi Pico (RP2040/RP2350) based board for interfacing one
 * or two MOS SID chips and/or hardware SID emulators over (WEB)USB with your
 * computer, phone or ASID supporting player.
 *
 * usbsid-worker.js (Javascript driver)
 * This file is part of USBSID-Pico-driver (https://github.com/LouDnl/USBSID-Pico-driver)
 * File author: LouD
 *
 * USBSID-Pico-driver
 * Copyright (C) 2025-2026  LouD
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

const _URL = globalThis.location.origin + '/websid';
const _PATH = self.location.pathname.replace(self.location.pathname.match().input, "/SID/");
const workerchannel = new BroadcastChannel('player')

/* WebUSB constants */
const CDC_CLASS     = 0x0A; /* Interface 1 */
const DEVICE_CLASS  = 0xFF; /* Interface 4 */
const CTRL_TRANSFER = 0x22;
const CTRL_ENABLE   = 0x01;
const CTRL_DISABLE  = 0x00;

/* USBSID constants */
const USBSID_VID = 0xcafe;
const USBSID_PID = 0x4011;;

/* Variables */
self.debug = true;
self.interfaces;
self.device = {};
self.classtype = DEVICE_CLASS;
self.async_await = true;

function _debug_worker(...message)
{
  if (self.debug) {
    console.info('%cWORKER:', 'background: #ff9100ff; color: #000000ff; font-weight: bold;', ...message);
  }
};

async function webusb_findinterface(connectType) {
  self.interfaces = self.device.configuration.interfaces;
  self.interfaces.forEach((element) => {
    element.alternates.forEach((elementalt) => {
      if (elementalt.interfaceClass == connectType) {
        self.device.interfaceNumber = element.interfaceNumber;
        elementalt.endpoints.forEach((elementendpoint) => {
          if (elementendpoint.direction == "out") {
            self.device.endpointOut = elementendpoint.endpointNumber;
          }
          if (elementendpoint.direction == "in") {
            self.device.endpointIn = elementendpoint.endpointNumber;
          }
        });
      }
    });
  });
}

async function write(buffer) {
  const result = self.device.transferOut(self.device.endpointOut, buffer);
  if (self.async_await) {
    const r = await Promise.resolve(result);
    return r;
  }
}

async function initusb() {
  // dev = undefined;
  try {
    // if (typeof dev !== "undefined") {
    //   self.device = dev;
    // } else {
    //   self.device = await self.usb.requestDevice({
    //     filters: [
    //       {
    //         vendorId: USBSID_VID,
    //         productId: USBSID_PID,
    //       },
    //     ],
    //   });

    //   if (self.device == null) {
    //     throw new Error("Could not find USBSID-Pico");
    //   }
    //   await self.device.open();
    //   _debug_worker("USBSID-Pico opened:", self.device.opened);
    // }
    const devices = await navigator.usb.getDevices(); // Or use navigator.usb.getDevices()
    _debug_worker("Devices:", devices);
    for (const device of devices) {
      _debug_worker("Device:", device);
      await device.open();
      // Perform operations on the device here
      self.device = device;
    }
    // _debug_worker("?" + self.device);

    self.isClosing = false;
    if (typeof self.device !== "undefined") {
      _debug_worker("configuration found:", self.classtype);
      _debug_worker("configuration found:", self.device.configuration);
      if (self.device.configuration === null) {
        _debug_worker("selectConfiguration");
        await self.device.selectConfiguration(1);
      }
      _debug_worker("Start find interface");
      await self.webusb_findinterface(self.classtype);
      _debug_worker("interfaces found:", self.interfaces);
      _debug_worker("interfaceNumber found:", self.device.interfaceNumber);
      _debug_worker("endpointIn found:", self.device.endpointIn);
      _debug_worker("endpointOut found:", self.device.endpointOut);
      _debug_worker("Start device claim");
      await self.device.claimInterface(self.device.interfaceNumber);
      await self.device.selectConfiguration(1);
      await self.device.selectAlternateInterface(self.device.interfaceNumber, 0);
      /* enable the interface */
      if (self.classtype === DEVICE_CLASS) {
        await self.device.controlTransferOut({
          requestType: "class",
          recipient: "interface",
          request: CTRL_TRANSFER,
          value: CTRL_ENABLE,
          index: self.device.interfaceNumber,
        });
      }
      workerchannel.postMessage({connected: true});
    }
  } catch (err) {
    _debug_worker(err);
  }
}

async function closeusb() {
  try {
    await self.device.releaseInterface(self.device.interfaceNumber);
    await self.device.close();
    _debug_worker("Closed device");
  } catch (err) {
    _debug_worker("Error:", err);
  }
}

workerchannel.onmessage = async function (m) {
  // _debug_worker('Received', m.data);

  if(m.data.write == true) {
    await write(m.data.buffer);
  }

  if(m.data.init == true) {
    // self.device = m.data.device;
    _debug_worker("Init command received");
    // initusb(m.data.device);
    initusb();
    // _debug_worker("Device is", self.device);
  }

  if(m.data.deinit == true) {
    _debug_worker("Deinit command received");
    closeusb();
  }
};
