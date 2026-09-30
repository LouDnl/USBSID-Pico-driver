/*
 * USBSID-Pico is a RPi Pico (RP2040/RP2350) based board for interfacing one
 * or two MOS SID chips and/or hardware SID emulators over (WEB)USB with your
 * computer, phone or ASID supporting player.
 *
 * Cmd.java (Java driver)
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

import java.util.Arrays;
import java.util.Collections;
import java.util.Map;
import java.util.function.Function;
import java.util.stream.Collectors;

public enum Cmd {
  /* BYTE 0 - top 2 bits */
  WRITE((byte)0),   /*        0b0 ~ 0x00 */
  READ((byte)1),   /*        0b1 ~ 0x40 */
  CYCLED_WRITE((byte)2),   /*       0b10 ~ 0x80 */
  COMMAND((byte)3),   /*       0b11 ~ 0xC0 */
  /* BYTE 0 - lower 6 bits for byte count */
  /* BYTE 0 - lower 6 bits for Commands */
  CYCLED_READ((byte)4),   /*      0b100 ~ 0x04 */
  DELAY_CYCLES((byte)5),   /*      0b101 ~ 0x05 */
  PAUSE((byte)10),   /*     0b1010 ~ 0x0A */
  UNPAUSE((byte)11),   /*     0b1011 ~ 0x0B */
  MUTE((byte)12),   /*     0b1100 ~ 0x0C */
  UNMUTE((byte)13),   /*     0b1101 ~ 0x0D */
  RESET_SID((byte)14),   /*     0b1110 ~ 0x0E */
  DISABLE_SID((byte)15),   /*     0b1111 ~ 0x0F */
  ENABLE_SID((byte)16),   /*    0b10000 ~ 0x10 */
  CLEAR_BUS((byte)17),   /*    0b10001 ~ 0x11 */
  CONFIG((byte)18),   /*    0b10010 ~ 0x12 */
  RESET_MCU((byte)19),   /*    0b10011 ~ 0x13 */
  BOOTLOADER((byte)20),;   /*    0b10100 ~ 0x14 */

  private byte cmd;
  private Cmd(byte cmd) { this.cmd = cmd; }
  private static final Map<Byte, Cmd> lookup = Collections.unmodifiableMap(
      Arrays.asList(Cmd.values()).stream().collect(Collectors.toMap(Cmd::get, Function.identity())));
  public byte get() { return cmd; }
  public static Cmd getCommand(byte cmd) { return lookup.get(cmd); }
}
