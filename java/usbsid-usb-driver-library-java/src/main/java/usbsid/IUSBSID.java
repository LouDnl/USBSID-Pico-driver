/*
 * USBSID-Pico is a RPi Pico (RP2040/RP2350) based board for interfacing one
 * or two MOS SID chips and/or hardware SID emulators over (WEB)USB with your
 * computer, phone or ASID supporting player.
 *
 * IUSBSID.java (Java driver)
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
 * Additional permission under GNU GPL version 3 section 7
 *
 * If you modify this Program, or any covered work, by linking or combining it
 * with Clojure or any other library licensed under the Eclipse Public License
 * 1.0 or 2.0 (or a modified version of such a library), containing parts
 * covered by the terms of the Eclipse Public License, the licensors of this
 * Program grant you additional permission to convey the resulting work.
 * Corresponding Source for a non-source form of such a combination shall
 * include the source code for the parts of those libraries used as well as
 * that of the covered work.
 *
 */

package usbsid;

public interface IUSBSID {

  int USBSID_init(Integer...buffsize);
  int USBSID_init();
  int USBSID_init(String driver, int ringsize, int diffsize);

  void USBSID_exit();

  void USBSID_clkdwrite(byte addr, byte data, short cycles);

  void USBSID_writeclkdbuffer(byte addr, byte data, short cycles);

  void USBSID_setflush();

  public void USBSID_resetringbuffer();

  public void USBSID_resetallregisters();

  void USBSID_reset(byte volume);

  void USBSID_sendconfigcommand(int command, Byte...args);

  byte[] USBSID_rwconfigcommand(int command, int len, Byte...args);

  void USBSID_setclock(double CpuClock);

  int USBSID_setstereo(int stereo);

  byte[] USBSID_getsocketconfig();

  int[] USBSID_parsesocketconfig(byte[] socketcfg);

  int USBSID_getsocketsidtype(int socket, int sidno, byte[] socketcfg);

  int USBSID_sidtypebysidno(int sidno, byte[] socketcfg);

  int USBSID_getnumsids();

  String USBSID_getpcbversion();

  String USBSID_getfwversion();

  void USBSID_delay(short cycles);

}
