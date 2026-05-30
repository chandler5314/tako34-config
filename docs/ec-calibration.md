# Tako34 EC calibration

This branch uses Pete Johanson's `ec-support-zmk-module` EC matrix driver.

## Why this branch changes ADC gain

Pete's calibrator detects a pressed key during high-value sampling when the raw 12-bit ADC value crosses half scale, `2048`. The last working Tako34 firmware used raw thresholds around `700-800`, so the calibrator could wait forever without printing `*`.

The EC ADC channel now uses `ADC_GAIN_1_2` instead of `ADC_GAIN_1_6`. That multiplies the old EC raw readings by about `3`, so Pete's `2048` high-sampling gate is equivalent to about `683` in the previous scale.

## Calibration flow

1. Flash `tako_left_ec_calibrator_no_load.uf2` first if you may already have bad calibration data in flash.
2. Open the USB serial shell.
3. Run `ec kscan calibration start`.
4. During low sampling, do not press any key.
5. During high sampling, slowly press each key in sequence and release only after `*` appears.
6. Run `ec kscan calibration save`.
7. Flash `tako_left_ec.uf2` for normal daily use.

If a specific key still never prints `*`, that key is probably below the equivalent `~683` old-scale detection level. In that case the next adjustment should be a small source patch to Pete's calibrator high threshold rather than changing `trigger-percentage`; `trigger-percentage` only affects normal scanning after calibration is already complete.
