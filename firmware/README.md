# Firmware

RT1011 firmware for the proto board: USB Audio Class 2 (async, with feedback) to the AK4493 over
SAI1, and the power sequencing of the analog path. MIT licensed.

## Build

```sh
./build.sh        # pin check, host tests, firmware -> build/out/openmusicplayer.{elf,hex,bin,map}
./build.sh test   # host tests only
```

Docker does the building (`docker/Dockerfile`: Debian's arm-none-eabi-gcc 12.2, CMake, Ninja). `deps.sh`
fetches TinyUSB 0.21.0, NXP's MCUXpresso SDK (core and RT devices) and CMSIS 6 at pinned commits into
`deps/`, which isn't committed.

Flash the `.hex`. The `.bin` starts at 0x60000400 (the FlexSPI configuration block), not at the
start of flash, so a tool that takes a raw binary needs that address.

## First flash on a new board

A blank RT1011 leaves the boost off, and the USB PHY runs from the boost. Either:

- **SWD** on the Tag-Connect TC2030 footprint, or
- **USB serial downloader:** bridge JP1 and JP2 (boot mode) and JP3 (boost on), then NXP's
  `blhost`/MCUXpresso Secure Provisioning tool. Open all three afterwards.

The firmware drives BOOST_EN high from its first instruction, so JP3 never fights the pin.

## Layout

| Path | What |
|---|---|
| `src/output.c` | The analog path's state machine: supplies, oscillators, DAC, amp rails, drivers, gain, rate changes |
| `src/ak4493.c` | AK4493SEQ registers |
| `src/usb_audio.c`, `usb_descriptors.c` | UAC2 controls (rate, volume onto the DAC's attenuator, mute) and descriptors |
| `src/hal.h` | What the logic needs from the board |
| `board/` | RT1011 side: clocks, generated pins, SAI + eDMA, LPI2C, boot header for the W25Q32JV, linker script |
| `test/` | Host tests: `fake_board.c` simulates the pins, the AK4493 and the LM27762, and fails on any broken hardware rule |
| `tools/gen_pins.py` | Generates `board/pins.[ch]` from the MCU pin map in `hardware/proto/circuit.py`, checking every mux against NXP's `fsl_iomuxc.h` |

## Hardware rules the tests enforce

From the datasheets; `test/fake_board.c` flags a break at the simulated millisecond it happens.

- Only one oscillator drives MCLK at a time.
- No MCLK while the AK4493's PDN is low (datasheet Figure 61, note 9); PDN goes high only after
  VREFH has settled (LT3042 soft start: 4.7 µF × 47.5k = 223 ms, so 1.2 s).
- DFS changes only with RSTN = 0, and DFS must match MCLK/fs (Tables 11 to 13).
- BCLK = 64fs is a whole division of MCLK, at most MCLK / 2.
- OPA1622s enable only with AMP_PG high and the DAC's VCM settled. They disable only once muted,
  and before the rails drop.
- The gain switch moves only while muted.

## Limits found so far

- **192 kHz is the top rate.** The RT1011's SAI divides MCLK by at least 2 for the bit clock
  (no bypass: `FSL_FEATURE_SAI_HAS_BCLK_BYPASS = 0`), so 64fs at 24.576 MHz stops at 192 kHz.
- **The boot header uses single-wire reads** (0x0B). Quad reads need the flash's QE bit, which
  the W25Q32JVSSIQ is said to ship set; confirm on the first board, then switch the boot header to 0xEB.
- **The USB IDs are pid.codes' test pair** (0x1209:0x0001) until the project has its own.

## Not done yet

TPS25750 patch bundle (needs TI's configuration tool output), MAX17055 setup, display and buttons,
gain and filter UI, the bit-perfect test mode, HAB signing.
