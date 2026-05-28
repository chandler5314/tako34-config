# EC calibration research notes

Date: 2026-05-28

This branch focuses on the first milestone: make the Tako34 electro-capacitive matrix stable in firmware without changing the PCB.

## Summary

The current firmware uses the old Tako EC scanner in `config/tako_drivers/kscan/kscan_gpio_ec.c`. Each key has fixed `actuation_threshold` and `release_threshold` values. This works when the raw capacitance range is stable, but it is fragile when EC values drift.

Pete Johanson's newer EC support lives in `petejohanson/ec-support-zmk-module`. The important pieces are:

- `zmk,kscan-ec-matrix`
- `zmk,gpio-mux`
- shell calibration commands under `ec <device> calibration ...`
- flash-backed calibration storage through Zephyr settings
- DTS export through `calibration export`

This module is not part of `zmkfirmware/zmk` mainline yet, so the branch adds it through `config/west.yml`.

## Mapping from old driver to new module

| Old Tako property | New EC module property |
| --- | --- |
| `row-gpios` | `strobe-gpios` |
| `mux-sel-gpios` + `col-channels` | `zmk,gpio-mux` + ordered `input-gpios` |
| `mux-en-gpios` | `zmk,gpio-mux` `en-gpios` |
| `discharge-gpios` | `drain-gpios` |
| `power-gpios` | `power-gpios` |
| `io-channels` | `io-channels` |
| hardcoded C thresholds | saved calibration low/high/noise values |

The old column order `<3 0 1 2 6 7 5>` is preserved in `input-gpios`.

The physical matrix is still 6 rows by 7 columns. Empty positions are skipped with:

```dts
strobe-input-masks = <0 0 0 71 5 72>;
```

## Expected workflow

1. Flash `tako_left_ec_calibrator.uf2`.
2. Open the USB CDC serial shell.
3. Run `ec` to find the matrix device name.
4. Run `ec <device-name> calibration start`.
5. Do not touch keys during low sampling.
6. Slowly press each real key once during high sampling.
7. Run `ec <device-name> calibration save`.
8. Optional: run `ec <device-name> calibration export` and paste exported values into the overlay as `precalib-avg-lows` and `precalib-avg-highs`.
9. Flash `tako_left_ec.uf2` for normal use.

## Known risk

Pete's calibrator currently treats a key as pressed during high sampling only when the raw ADC value exceeds half the ADC range:

```c
uint16_t high_threshold = (1 << (cfg->adc_channel.resolution - 1));
```

With 12-bit ADC, that is `2048`. The old Tako34 thresholds were around `700-800`, so this may be too high for this PCB and analog front end. If high sampling never prints an asterisk after pressing a key, the next patch should make the raw high-sampling threshold configurable in DTS, for example:

```dts
calibration-press-threshold-raw = <750>;
```

This is preferable to changing the PCB and probably safer than increasing ADC gain before measuring the OPA350 output range.

## Xorlink firmware note

The downloaded Xorlink HHKB DFU packages were checked as static binaries. The `.bin` payloads have near-max entropy and do not start with a Cortex-M/nRF vector table. A normal nRF52840 image should begin with a stack pointer near `0x200xxxxx` and a reset handler address. These files look encrypted or otherwise transformed, so they are not directly useful as disassembly sources for Logi Bolt or Unifying.

Bolt/Unifying should remain a later research branch. The practical order should be:

1. Stabilize EC scan and calibration.
2. Confirm battery and power behavior.
3. Start a separate receiver-compatibility branch.

Unifying has open-source research such as `decrazyo/unifying`, but it is a proprietary 2.4 GHz protocol and would need a separate radio/ESB integration. Logi Bolt is BLE-based but proprietary and should not block EC stabilization.
