# Changelog

## 1.0.1

- Fix Aim Only gyro for Alt-Fire assigned to a separate button by reading the current player's native Alt-Fire input request alongside trigger commands.
- Hip-fire Only and Flick Stick's outside-aim mode use the same input state. Releasing hold-to-aim/Alt-Fire takes effect before exit animations finish; the game's toggle behavior is respected.
- Observe the game's controls without changing button mappings or triggering aim/Alt-Fire actions. Ordinary hip fire does not enable Aim Only gyro.
- Add regression coverage for independent Alt-Fire press/release, missing trigger data, aim handoffs, sensitivity multipliers, gameplay guards and queued camera motion.

## 1.0.0

- First public release packaging and minimal source distribution.
- Consolidated mod/dependency license texts into LICENSES.txt.
- Includes the gyro, mixed-input, Flick Stick, native menu and controller contact support developed through 0.7.4.
- Includes the orientation recovery correction described below; no additional gameplay changes in this release.

## 0.7.4

- Preserve orientation during a brief reconnect of the same Steam controller (up to two seconds).
- Initialize gravity from valid acceleration after a true reset, removing the offline-reproduced horizontal recovery delay.
- Keep Steam calibration ownership, discard missing motion and log reset reasons. Offline regression tests added; reported gameplay symptom awaits confirmation.

## 0.7.3

- Recover Steam motion/controller association when SDL initially lacks the Steam handle, using verified XInput slot ownership.

## 0.7.0 - 0.7.2

- Separate gyro control families with left/right/either/both selections where supported.
- Hide unsupported rows from rendering and navigation.
- Analog stick deflection controls gyro; L3/R3 remain button choices.

## 0.6.0 - 0.6.1

- Steam Controller 2 trackpad, stick-touch and grip-touch inputs.
- Original Steam Controller trackpad touch support.

## 0.5.0 - 0.5.2

- Steam Input motion support with Steam-owned calibration.
- Aim Only/Hip-fire Only follow the actual aim command.

## 0.4.0 - 0.4.4

- Native calibration footer and countdown; legacy F8 menu removed.
- 200 ms short-tap game actions with immediate gyro response and native hold-interaction exceptions.
- Preserve controller rumble during mixed mouse/controller input.

## 0.3.0 - 0.3.4

- Flick Stick, 150 ms default spin, immediate aim-release handoff and circular rotation without a new flick when already deflected.

## 0.2.x

- Native localized Gyro Configuration page, settings persistence and controller/mouse navigation.
