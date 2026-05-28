# Tako34 ZMK EC config

This branch migrates the Tako34 EC scanner from the old hardcoded-threshold driver to Pete Johanson's calibrated EC matrix module:

- Module: <https://github.com/petejohanson/ec-support-zmk-module>
- Driver: `zmk,kscan-ec-matrix`
- Calibration storage: ZMK/Zephyr settings in flash
- Calibration shell: enabled by the `enable_ec_calibrator` snippet

## Build artifacts

GitHub Actions builds three firmware artifacts:

| Artifact | Purpose |
| --- | --- |
| `tako_left_ec.uf2` | Normal daily firmware. It auto-loads saved calibration settings. |
| `tako_left_ec_calibrator.uf2` | Calibration firmware with USB shell enabled. |
| `tako_left_ec_calibrator_no_load.uf2` | Calibration firmware that ignores previously saved calibration on boot. Use this if bad settings were saved. |

## Hardware mapping

| Function | Pins |
| --- | --- |
| Strobes / rows | P0.3, P1.10, P1.11, P0.24, P0.9, P0.10 |
| 74HC4051 select | P0.5, P0.30, P0.31 |
| 74HC4051 enable | P1.9, active-low |
| EC ADC read | P0.28 / AIN4 |
| Discharge / drain | P0.2 |
| Matrix power | P1.13 |
| Battery ADC | P0.29 / AIN5 |

The old column mux order `<3 0 1 2 6 7 5>` is preserved through the new `input-gpios` order.

## Calibration steps

1. Flash `tako_left_ec_calibrator.uf2`.
2. Connect the keyboard over USB.
3. Open the USB CDC serial shell.
4. Run `ec` to list the EC matrix device name.
5. Run:

```text
ec <device-name> calibration start
```

During low sampling, do not touch any keys. During high sampling, slowly press each real key once and release after the shell prints an asterisk.

After calibration completes, run:

```text
ec <device-name> calibration save
ec <device-name> calibration export
```

`save` writes calibration to flash. `export` prints DTS arrays that can be pasted into `config/boards/shields/tako/tako_left.overlay` as `precalib-avg-lows` and `precalib-avg-highs` later.

Finally flash `tako_left_ec.uf2` for normal use.

## Troubleshooting

Pete's current calibrator detects a pressed key when the raw ADC value exceeds half the ADC range. With 12-bit ADC, that threshold is `2048`. The previous Tako34 hardcoded thresholds were around `700-800`, so if high sampling never records a key press, the next firmware change should make this raw calibration threshold configurable for this board.
