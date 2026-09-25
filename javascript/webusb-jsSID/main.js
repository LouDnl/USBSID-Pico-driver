/* 
 * USBSID-Pico is a RPi Pico (RP2040/RP2350) based board for interfacing one
 * or two MOS SID chips and/or hardware SID emulators over (WEB)USB with your
 * computer, phone or ASID supporting player.
 *
 * main.js (Javascript driver)
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

let saveddevice;
let hermit = null;
document.addEventListener('DOMContentLoaded', event => {
  saveddevice = localStorage.getItem("usbsidconnection");

  if (saveddevice != 'null') {
    USBSID_init(saveddevice);
    localStorage.setItem("usbsidconnection", saveddevice);
  }

  let connectButton = document.querySelector("#device-connect");
  let loadSIDButton = document.querySelector("#load-file")
  let playButton = document.querySelector("#play-pause");
  let stopButton = document.querySelector("#stop");
  hermit = new jsSID(0, 0, false, true);
  /* Connect */
  connectButton.addEventListener('click', function () {
    if (connectButton.textContent == 'Connect') {
      USBSID_init();
      connectButton.textContent = 'Disconnect';
      // if (saveddevice == 'null' || saveddevice != usbsid.device) {
      localStorage.setItem("usbsidconnection", saveddevice);
      // }
    } else if (connectButton.textContent == 'Disconnect') {
      USBSID_deinit();
      connectButton.textContent = 'Connect';
    };
  });
  loadSIDButton.addEventListener('click', function () {
    hermit.setvolume(1);
    hermit.playcont();
    hermit.loadinit("Bubble_Bobble.sid", 0);
  });
  /* Play / Pause */
  playButton.addEventListener('click', function () {
    // hermit.loadstart("Bubble_Bobble.sid", 0);
    hermit.setvolume(1);
    hermit.start(0);

  });
  stopButton.addEventListener('click', function () {
    hermit.setvolume(0);
    hermit.stop();
  });
});
