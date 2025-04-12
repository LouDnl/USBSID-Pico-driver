package usbsid;

import java.util.Arrays;
import java.util.Collections;
import java.util.Map;
import java.util.function.Function;
import java.util.stream.Collectors;

public enum Command {
  /* BYTE 0 - top 2 bits */
  WRITE(0),   /*        0b0 ~ 0x00 */
  READ(1),   /*        0b1 ~ 0x40 */
  CYCLED_WRITE(2),   /*       0b10 ~ 0x80 */
  COMMAND(3),   /*       0b11 ~ 0xC0 */
  /* BYTE 0 - lower 6 bits for byte count */
  /* BYTE 0 - lower 6 bits for Commands */
  PAUSE(10),   /*     0b1010 ~ 0x0A */
  UNPAUSE(11),   /*     0b1011 ~ 0x0B */
  MUTE(12),   /*     0b1100 ~ 0x0C */
  UNMUTE(13),   /*     0b1101 ~ 0x0D */
  RESET_SID(14),   /*     0b1110 ~ 0x0E */
  DISABLE_SID(15),   /*     0b1111 ~ 0x0F */
  ENABLE_SID(16),   /*    0b10000 ~ 0x10 */
  CLEAR_BUS(17),   /*    0b10001 ~ 0x11 */
  CONFIG(18),   /*    0b10010 ~ 0x12 */
  RESET_MCU(19),   /*    0b10011 ~ 0x13 */
  BOOTLOADER(20),;   /*    0b10100 ~ 0x14 */

  private int cmd;

  private Command(int cmd) {
    this.cmd = cmd;
  }

  public int getCommand() {
    return cmd;
  }

  private static final Map<Integer, Command> lookup = Collections.unmodifiableMap(
      Arrays.asList(Command.values()).stream().collect(Collectors.toMap(Command::getCommand, Function.identity())));

  public static Command get(int cmd) {
    return lookup.get(cmd);
  }
}
