# Returnal Native Gyro + Mixed Input

Native gyro aiming, Flick Stick and simultaneous mouse/controller input for Returnal on Windows. Version **1.0.3**.

## Install

Close Returnal. Extract the Nexus archive into the game installation folder, merging its `Returnal` folder. `version.dll`, `ReturnalGyro.dll` and `SDL3.dll` must be beside `Returnal-Win64-Shipping.exe` in `Returnal/Binaries/Win64`. Do not overwrite DLLs belonging to another mod.

Open **Settings > Controls > Gyro Configuration**. Default: gyro enabled, **Aim Only**, Player Space, X/Y sensitivity 2.50. Hold aim to test it. Aim Only also includes Alt-Fire preparation/firing, even when assigned to a separate button; Hip-fire Only excludes both. Flick Stick is off by default (150 ms spin duration).

Existing `ReturnalGyro.ini` settings survive updates; defaults are created when no INI exists. To uninstall, close the game and remove only the three mod DLLs. Keep the INI to retain settings.

## Controllers

- With Steam Input: use a **Gamepad** layout and set Steam's gyro behavior to **None**. The mod reads Steam motion; Steam owns calibration. Normal game inputs still follow the game's input system.
- Without Steam Input: automatic selection falls back to passive Sony HID or SDL motion sensors. For direct sensors, the native settings footer offers recalibration after a five-second countdown. Place the controller on a stable surface.
- Physically tested during development: DualSense Edge over USB, original Steam Controller over USB, and Steam Controller 2 via its Puck. Other transports/controllers, including Steam Deck built-in controls, are not certified.

Only supported control families are shown: buttons, touchpads, capacitive stick/grip contacts and analog stick deflection. Left/right/either/both are available where supported. In automatic activation modes, the selected controls suspend gyro; Hold to Enable and Toggle use them to activate it.

Selected game buttons respond to a tap shorter than 200 ms on release; gyro responds immediately. Native hold interactions, item scans and timed pickups keep their full hold, including remapped interaction buttons. Touch contact and analog deflection are separate from clicks.

## Features

- Native angular camera input, with no gyro-to-mouse emulation.
- Player Space, World Space, Local Yaw and Local Roll.
- Aim-command activation, independent sensitivity axes and aim/Alt-Fire multipliers.
- Optional smoothing/acceleration presets and Flick Stick.
- Mixed-input guards for controller presentation and mouse-triggered rumble interruption.
- Native game menu in English, French, German, Spanish, Italian and Portuguese.

## Compatibility and reporting

Targets the Windows Steam build **11083317 / UE 4.25.1** with matching modules. Other builds are rejected by validation. This is an unofficial mod; no game files are included.

Sixteen automated test suites pass. The orientation recovery fix introduced in 0.7.4 is validated offline; confirmation of the originally reported intermittent symptom in gameplay is still pending.

Some antivirus engines flagged the 0.7.4 runtime. These reports have not been cleared by the vendors; a false positive is suspected, not confirmed. Report the exact file hash, engine and detection label. Do not disable antivirus protection to install the mod.

For gameplay issues, include the mod version, controller/connection, Steam Input configuration and reproduction steps. Review logs/settings for personal information before sharing them.

## Source and changes

See [CHANGELOG.md](CHANGELOG.md) and [BUILDING.md](BUILDING.md). This repository contains runtime sources, translations, tests and the build/package tools. It excludes development probes, dumps, personal settings and compiled binaries. SDL is downloaded separately with SHA-256 verification. The modified GamepadMotionHelpers header and required MinHook source are included for review.

All license texts and dependency credits are collected in [LICENSES.txt](LICENSES.txt). Original mod code is MIT licensed; dependencies retain their own terms.
