# Tako34 EC calibration

This branch uses Pete Johanson's `ec-support-zmk-module` EC matrix driver, plus a small Tako helper shell command that saves calibration automatically after high-value sampling finishes.

## Why this branch changes ADC gain

Pete's calibrator detects a pressed key during high-value sampling when the raw 12-bit ADC value crosses half scale, `2048`. The last working Tako34 firmware used raw thresholds around `700-800`, so the calibrator could wait forever without printing `*`.

The EC ADC channel now uses `ADC_GAIN_1_2` instead of `ADC_GAIN_1_6`. That multiplies the old EC raw readings by about `3`, so Pete's `2048` high-sampling gate is equivalent to about `683` in the previous scale.

## Why trigger-percentage is 20

Pete's normal scanning trigger uses a percentage between the calibrated low and high values. The default `50` means midpoint, which is too sensitive for this Tako34 and can leave a key falsely pressed after calibration. `20` puts the trigger point close to the calibrated high value, approximating the old `750` actuation and `700` release behavior when the high value is around `800` in the original scale.

## Calibration flow

1. Flash `tako_left_ec_calibrator_autosave_no_load.uf2` first if you may already have bad calibration data in flash.
2. Open the USB serial shell.
3. Run `ecauto kscan calibration start`.
4. During low sampling, do not press any key.
5. During high sampling, slowly press each key in sequence and release only after `*` appears.
6. Wait for `Calibration saved to flash.` before resetting or unplugging.
7. Flash `tako_left_ec.uf2` for normal daily use.

The old Pete command `ec kscan calibration start` is still present, but it still requires a manual `ec kscan calibration save`. Use `ecauto kscan calibration start` for this keyboard so the save happens automatically.

If a specific key still never prints `*`, that key is probably below the equivalent `~683` old-scale detection level. The next adjustment should be lowering the calibration press gate in the EC driver itself while keeping the old ADC gain.
