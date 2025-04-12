package usbsid;

import java.util.Arrays;
import java.util.Collections;
import java.util.Map;
import java.util.function.Function;
import java.util.stream.Collectors;

public enum CfgCommand {
  RESET_USBSID(0x20),  /* Resets the MCU including the USB connection */

  READ_CONFIG(0x30),  /* Read full config as bytes */
  APPLY_CONFIG(0x31),  /* Apply config from memory */
  SET_CONFIG(0x32),  /* Set single config item */
  SAVE_CONFIG(0x33),  /* Save and load config and then reboot */
  SAVE_NORESET(0x34),  /* Save, load and apply config */
  RESET_CONFIG(0x35),  /* Reset to default settings */
  WRITE_CONFIG(0x36),  /* Write full config as bytes */
  READ_SOCKETCFG(0x37),  /* Read socket config as bytes */
  RELOAD_CONFIG(0x38),  /* Reload and apply stored config from flash */
  READ_NUMSIDS(0x39),  /* Returns the number of SIDs in byte 0 */
  READ_FMOPLSID(0x3A),  /* Returns the sidno for FMOpl 1~4, 0 is disable  */

  SINGLE_SID(0x40),  /* Single SID Socket One */
  DUAL_SID(0x41),  /* Dual SID Socket One */
  QUAD_SID(0x42),  /* Four SID's in 2 sockets */
  TRIPLE_SID(0x43),  /* Two SID's in socket One, One SID in socket two */
  TRIPLE_SID_TWO(0x44),  /* One SID in Socket One, Two SID's in socket two */
  MIRRORED_SID(0x45),  /* Socket Two is linked to Socket One */
  DUAL_SOCKET1(0x46),  /* Two SID's in socket One, Socket Two disabled */
  DUAL_SOCKET2(0x47),  /* Two SID's in socket Two, Socket One disabled */

  SET_CLOCK(0x50),  /* Change SID clock frequency by array id */
  DETECT_SIDS(0x51),  /* Try to detect the SID types per socket */
  TEST_ALLSIDS(0x52),  /* Runs a very long test on all SID's */
  TEST_SID1(0x53),  /* Runs a very long test on SID 1 */
  TEST_SID2(0x54),  /* Runs a very long test on SID 2 */
  TEST_SID3(0x55),  /* Runs a very long test on SID 3 */
  TEST_SID4(0x56),  /* Runs a very long test on SID 4 */
  GET_CLOCK(0x57),  /* Returns the clockrate as array id in byte 0 */
  LOCK_CLOCK(0x58),  /* Locks the clockrate from being changed, saved in config */
  STOP_TESTS(0x59),  /* Interrupt any running SID tests */

  LOAD_MIDI_STATE(0x60),
  SAVE_MIDI_STATE(0x61),
  RESET_MIDI_STATE(0x63),

  USBSID_VERSION(0x80),  /* Read version identifier as uint32_t */
  US_PCB_VERSION(0x81),  /* Read PCB version */

  RESTART_BUS(0x85),  /* Restart DMA & PIO */
  RESTART_BUS_CLK(0x86),  /* Restart PIO clocks */
  SYNC_PIOS(0x87),  /* Sync PIO clocks */
  TOGGLE_AUDIO(0x88),  /* Toggle mono <-> stereo (v1.3+ boards only) */
  SET_AUDIO(0x89),;  /* Set mono <-> stereo (v1.3+ boards only) */


  private int cmd;

  private CfgCommand(int cmd) {
    this.cmd = cmd;
  }

  public int getCfgCommand() {
    return cmd;
  }

  private static final Map<Integer, CfgCommand> lookup = Collections.unmodifiableMap(
      Arrays.asList(CfgCommand.values()).stream().collect(Collectors.toMap(CfgCommand::getCfgCommand, Function.identity())));

  public static CfgCommand get(int cmd) {
    return lookup.get(cmd);
  }

}
