# Backlog and handover

Where the work stands, what is blocked and on what, and — most importantly — **which merged
changes have never been run on real hardware.** Written 20 Aug 2026, last touched after the
QMC5883P bench session of 9 Sep 2026.

If you are picking this up cold, read [README.md](README.md) for what the app is, then this
file for what is left.

## State

- `master`, everything merged, no open PRs on this repo.
- CI (`.github/workflows/checks.yml`) runs five checks on every PR: `clang-format`,
  `SUPPORTED_CHIPS.md matches chip_db.c`, and a build against each of the three firmware
  SDKs (official, Unleashed, Momentum). The builds are the only compile check that exists —
  `ufbt lint` runs clang-format and nothing more.
- 82 I²C parts and 15 1-Wire families in the database. 16 live tests.
- The bench is an `ssh notebook` (Windows) with a Flipper on COM5 running Unleashed. The app
  is installed at `/ext/apps/GPIO/Programmers/fake_chip_detector.fap`; `tools/flipper_rpc.py`
  drives it over the RPC channel for screenshots and key presses.

## The honest list: merged but never exercised on hardware

None of these are known broken. They are simply unproven — they compile, their logic was
traced, and no one has watched them run. Anything on this list should be the first thing
tried when a Flipper and the right parts are next in the same room.

| What | Landed | What has never happened |
|---|---|---|
| LPUART transport and the automatic listen after an empty sweep | #31 | No byte has been sent or received on a real LPUART. Test plan steps 7a–7e all outstanding. |
| UART self-test screen | #34 | The screen has never been drawn; the loopback has never executed. |
| Bare SDA-to-SCL short detection | #36 | Never reproduced with an actual jumper. This is `TESTING.md` Step 2, which the app could not satisfy at all until #36. |
| Strap-and-blink power-cycle ladder, and the pad meter | #20 | The rail blink, and the automatic rescan behind it, unwatched. The pad meter has never been checked against a known level (pin 8 must read LOW, pin 9 HIGH, open air FLOATING). |
| Live-test verdict wording | #21 | Never seen on a screen. |
| Chip `kind` renames | #33 | Text only. Widths were measured exactly (see below), not photographed. |
| AK09911 saturation screen | #44 | The overflow branch was rewritten to publish its own frame instead of freezing the display, and no magnet has been held against a part to watch it. Everything else in this test has now run. |
| QMC5883P live test thresholds | #46, #47 | The test itself has now run and passed twice on a GY-271 board, most recently on 0.11 — but both of its thresholds are still derived rather than measured: the coil floor from the datasheet's noise figure, the 300-count movement from its sensitivity figure. The still part's noise floor has never been recorded, and one pass came after only two reads, which is few enough that a sensor reconnecting mid-test could supply the swing on its own. Measure the still part, and consider requiring more than two samples before a pass. |
| QMC5883L live test | #48 | Written from the datasheet and never run: no QMC5883L has been on this bench at all, only a P. Its single threshold — 240 counts of swing on two axes — is arithmetic off the sensitivity figure, and the part has no self-test bit, so there is no second proof under it. First magnetometer that turns up, run it and measure the still part's noise floor while you are there. |
| The `RST` mode pin | #40 | The `RST` pad has never been strapped; neither magnetometer board on the bench was wired for it. |
| `SEVERAL POSSIBLE` verdict | #40 | Covered by the host test in `tools/chip_db_test/`, which is real coverage of the decision but not of the screen. The summary line, the `Fits:` list on the detail screen and the report paragraph have never been drawn. |

**Run on hardware 9 Sep 2026, and no longer on the list above:** an AK09911 on a Flipper
running Unleashed identified as `GENUINE at 0x0D`, and its live test passed end to end — the
self-test coil fired and landed inside the datasheet window, and the field followed the board
when it was turned. That session is also what found #44: the screen only draws two lines under
a heading and progress boxes, so the third was silently dropped, and the movement threshold had
been calibrated against a magnet and sat above the physical maximum of the earth's field. Both
are fixed. The AK09911 is the second part ever driven end to end here.

**Also run on hardware 9 Sep 2026:** a blue board silkscreened GY-271 identified as
`QMC5883P` at 0x2C, and the question screen drew its note — `GY-271 board, not a 5883L` —
above the verdict with the layout of #45 intact, which is what that change was for. Its live
test then passed on its first run: 1036 reads, both proof boxes filled, the coil fired and the
field followed the board. A second entry into the same test did not, and that is what #46
fixes: the self-test read was waiting on a DRDY the part had already stopped producing, and
the measurement configuration was written on top of the previous run's mode instead of a mode
this test had established.

**And again on 0.11, after #47:** the test was re-entered on the same board and passed — the
coil fired, the field followed, and the field magnitude read 27 µT, inside the earth's 25 to 65.
That run also confirmed the pacing fix: screenshots over the RPC channel work during the test
now, where 0.10 flooded the screen stream until USB writes timed out.

Screen widths in #33 and #34 were measured with
[`tools/screen_width.py`](tools/screen_width.py), which decodes the real `FontSecondary`
out of the SDK's `firmware.elf` and reimplements `u8g2_string_width`. That is exact, but it
is arithmetic, not a photograph.

## Blocked on the bench

Needs a Flipper plus the named part. Nothing here can be closed by reading code.

- **Test plan steps 7a–7e** — the LPUART transport, end to end. `TESTING.md` Step 2 now has
  a self-test paragraph; start there, it needs one jumper and no sensor.
- **Stage C: BNO055 interrogation over UART.** Deliberately **not** in `master` — the
  inter-byte timing and the reset-to-ready delay cannot be learned without the part, and
  shipping it marked "unverified" was rejected. Needs a live BNO055 strapped into UART mode.
- **The 1-Wire busy screen** — does it need a spinner? Needs a real 1-Wire part on pin 17 to
  judge whether the pause is long enough to look wedged.
- **Power cycle and rescan**, on the VL6180X: the wiring screen's live lines should visibly
  go dark and come back, and the automatic rescan should find the part again.

Note the standing limitation: the app **cannot** switch the external 3V3 rail on its own —
that was measured, not assumed. The ladder works around it by asking the user.

## Blocked on datasheets

- **21 mode-pin rows** are drafted but unverified. Every row in `chip_mode_pins` must carry a
  datasheet quote in a comment, because a wrong `i2c_high` sends someone to strap a pin the
  wrong way — worse than having no row at all. Bosch, ST, Analog Devices and TDK PDFs are
  unreachable from the machine this was developed on; they need fetching some other way.

## Needs a decision from the maintainer

- **A paced, captioned demo GIF** for the catalog pull requests — half-built, and it is not
  clear it is still wanted. Decide, then either finish it or drop the idea.

## Getting the app into the firmware catalogs

Four outbound pull requests, none of them on this repo. Status as of 18 Aug 2026:

| Catalog | PR | State |
|---|---|---|
| Unleashed | `xMasterX/all-the-plugins#254` | **Merged**, and confirmed by eye in the catalog. |
| Official | `flipperdevices/flipper-application-catalog#1183` | Changes requested. Both review items addressed 12 Aug, branch bumped 16 Aug, no reply since. The `fixes needed` label is still on. Its `task-list-completed` check sits at 6/10 and cannot go green here: three of the four remaining boxes belong to the reviewer, not the submitter. |
| Momentum | `Next-Flip/Momentum-Apps#82` | Untouched since 11 Aug. |
| Curated list | `djsime1/awesome-flipperzero#172` | Untouched since 11 Aug. |

## Things that will trip you up

- **Run `ufbt` from `fake_chip_detector/`**, never from the repo root, or it cannot find
  `application.fam`.
- **The minor API version is not a trap, and it is not checked.** Read
  `lib/flipper_application/application_manifest.c` before believing otherwise: the loader
  compares the API **major** and the hardware target, and the minor comparison sits inside a
  `/* */` in official 1.4.3, `unlshd-092` and `mntm-012` alike. An app built against 88.4 does
  start on an 88.2 firmware. What can still break is a symbol: the ELF loader resolves imports
  against the firmware's table, so an app using something added after 88.2 fails to load there
  for that reason, not for its version number. To check before shipping, diff the app's
  undefined symbols against the older SDK's `targets/f7/api_symbols.csv` — on 9 Sep 2026 all
  196 imports of the 88.4 build were present in `unlshd-090`.
- `~/.ufbt/current` is **machine-wide shared state.** Changing the SDK affects anything else
  building on that machine.
- **`python tools/gen_supported_chips.py --check` must pass.** It regenerates
  `SUPPORTED_CHIPS.md` from `chip_db.c` and enforces a 21-character cap on `kind` strings.
  That cap is a *tripwire, not a guarantee* — characters do not predict pixels. When it
  fires, measure with `tools/screen_width.py` and raise it if the string genuinely fits.
- **Every ID register must trace to a datasheet.** A wrong expected value makes the app call
  a genuine part counterfeit, which is the one failure this app must never have. Dropping a
  chip is always better than guessing at it.
- **A strap is configuration, not fraud.** Nothing in the mode-pin work is allowed to
  produce a counterfeit verdict, and an ID read over UART is exactly as forgeable as one
  read over I²C.
