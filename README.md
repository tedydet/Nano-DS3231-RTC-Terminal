# Nano DS3231 RTC Terminal

An interactive **Arduino Nano serial terminal for DS3231 RTC modules**.

This sketch turns an Arduino Nano into a small USB-to-I²C bridge with DS3231-aware commands. You can inspect and configure a DS3231 RTC directly from a serial terminal without reflashing a new sketch for every experiment.

It is useful for:

- testing DS3231 RTC modules
- setting and reading time/date
- decoding control and status registers
- testing alarms
- enabling square-wave and 32.768 kHz outputs
- reading the internal temperature sensor
- experimenting with the aging-offset register
- probing optional EEPROM/FRAM chips at `0x50..0x57`
- educational “RTC-as-computation” experiments such as `rtcadd`, `rtcsub`, and `rtcmp`

Many cheap DS3231 breakout boards also include an AT24C32 EEPROM. This terminal can help you find and test it.

---

## Hardware

### Required

- Arduino Nano or compatible ATmega328P board
- DS3231 RTC module with I²C interface
- USB cable
- Serial terminal or Arduino IDE Serial Monitor

### Optional

- LED + resistor for `INT/SQW` square-wave experiments
- Pull-up resistor for `INT/SQW` or `32kHz`, if not already present on your module
- CR2032 or other backup battery for the RTC module
- EEPROM/FRAM on the I²C bus, for example AT24C32, 24LC256, MB85RC256V

---

## Wiring

For a classic Arduino Nano:

| Arduino Nano | DS3231 module |
|---|---|
| `5V` | `VCC` |
| `GND` | `GND` |
| `A4` | `SDA` |
| `A5` | `SCL` |

The DS3231 normally uses I²C address:

```text
0x68
```

If your module has an onboard EEPROM, it often appears at:

```text
0x57
```

Some modules expose additional pins:

| DS3231 pin | Meaning |
|---|---|
| `INT/SQW` | Alarm interrupt or programmable square-wave output |
| `32K` / `32kHz` | 32.768 kHz output, if exposed |

Both outputs are open-drain/open-collector style and need a pull-up to be useful.

---

## Uploading

1. Open the sketch in the Arduino IDE.
2. Select the correct board, usually **Arduino Nano**.
3. Select the correct processor variant if needed, for example **ATmega328P (Old Bootloader)** for many clone boards.
4. Upload the sketch.
5. Open the Serial Monitor at:

```text
115200 baud
```

Use `Newline` or `Both NL & CR` as line ending.

You should see:

```text
Nano I2C/DS3231 Interface
Type help.
```

---

## Quick start

Type:

```text
help
scan
time
status
ctrl
temp
```

Expected `scan` output with a DS3231 module:

```text
Scanning I2C bus...
Found 0x68  DS3231/RTC?
```

If your module has onboard EEPROM:

```text
Found 0x57  EEPROM/FRAM?
```

---

## Command reference

### General I²C

#### `help`

Show the command list.

#### `scan`

Scan the I²C bus for devices.

Example:

```text
scan
```

Typical output:

```text
Found 0x68  DS3231/RTC?
Found 0x57  EEPROM/FRAM?
```

#### `r addr reg len`

Read `len` bytes from register `reg` of I²C device `addr`.

Example:

```text
r 68 00 07
```

This reads the DS3231 time/date registers from `0x00` to `0x06`.

#### `w addr reg value`

Write one byte to a device register.

Example:

```text
w 68 0E 00
```

This writes `0x00` to the DS3231 control register.

Be careful with direct register writes.

---

## DS3231 time and date

#### `time`

Read and print the current RTC date/time.

Example:

```text
time
```

Output:

```text
TIME 2026-04-29 23:30:12  DOW=1
```

#### `set YYYY MM DD HH MM SS`

Set the RTC date/time.

Example:

```text
set 2026 04 29 23 30 00
```

The sketch currently sets the day-of-week register to `1` internally. The DS3231 does not calculate day-of-week from the date.

#### `dump`

Dump DS3231 registers `0x00..0x12` in hex.

Example:

```text
dump
```

Useful for inspecting raw BCD time/date values, alarm registers, control/status bits, aging offset, and temperature registers.

---

## Temperature and TCXO

#### `temp`

Read the internal DS3231 temperature registers.

Example:

```text
temp
```

Output:

```text
TEMP 24.25 C
```

The DS3231 temperature sensor is mainly used internally for crystal compensation. It is useful for experiments, but it is not a precision laboratory thermometer.

#### `convtemp`

Force a manual temperature conversion and print the updated temperature.

Example:

```text
convtemp
```

This can also be useful after changing the aging-offset register.

#### `aging`

Read the signed aging-offset register `0x10`.

Example:

```text
aging
```

Output:

```text
AGING raw=0x00 signed=0
```

#### `aging value`

Set the aging-offset register.

Example:

```text
aging -5
```

Positive values slow the oscillator, negative values speed it up. The sketch triggers a manual temperature conversion after writing the value so the new correction is applied.

Use this carefully and note the original value before changing it.

---

## Status and control

#### `status`

Decode status register `0x0F`.

Shows:

- `OSF` — Oscillator Stop Flag. If `1`, time may be invalid.
- `EN32kHz` — 32.768 kHz output enabled.
- `BSY` — TCXO/temperature conversion busy.
- `A2F` — Alarm 2 flag.
- `A1F` — Alarm 1 flag.

Example:

```text
status
```

#### `ctrl`

Decode control register `0x0E`.

Shows:

- `EOSC` — oscillator stop control in battery mode
- `BBSQW` — battery-backed square-wave enable
- `CONV` — manual temperature conversion bit
- `RS2/RS1` — square-wave frequency select
- `INTCN` — interrupt mode vs square-wave mode
- `A2IE` — Alarm 2 interrupt enable
- `A1IE` — Alarm 1 interrupt enable

Example:

```text
ctrl
```

#### `clearflags`

Clear `OSF`, `A2F`, and `A1F` in the status register.

Example:

```text
clearflags
```

Recommended after setting the time if `OSF = 1`.

---

## Clock outputs

### `INT/SQW` square-wave output

The DS3231 `INT/SQW` pin can be configured as a square-wave output.

```text
sqw 1
sqw 1024
sqw 4096
sqw 8192
sqw off
```

Meaning:

| Command | Output |
|---|---|
| `sqw 1` | 1 Hz |
| `sqw 1024` | 1.024 kHz |
| `sqw 4096` | 4.096 kHz |
| `sqw 8192` | 8.192 kHz |
| `sqw off` | return to interrupt/alarm mode |

Example:

```text
sqw 1
ctrl
```

This is useful for LED blinkers, counters, clock experiments, and wakeup logic.

The `INT/SQW` pin is open-drain and needs a pull-up.

### Battery-backed square-wave

#### `bbsqw on`

Enable battery-backed `INT/SQW` output when `VCC` is absent.

```text
bbsqw on
```

#### `bbsqw off`

Disable battery-backed `INT/SQW` output when `VCC` is absent.

```text
bbsqw off
```

This only matters if your module exposes `INT/SQW` and the external circuit is wired appropriately. A blinking LED powered from a coin cell will drain it quickly.

### 32.768 kHz output

```text
32k on
32k off
```

Enable or disable the separate `32kHz` output pin, if your module exposes it.

The `32kHz` pin is also open-drain and needs a pull-up.

---

## Alarm 1 patterns

The DS3231 has two alarms. This sketch currently configures **Alarm 1**.

#### `alarm everysec`

Configure Alarm 1 to match once per second.

```text
alarm everysec
```

#### `alarm sec S`

Configure Alarm 1 to match when seconds equal `S`.

```text
alarm sec 30
```

#### `alarm minsec M S`

Configure Alarm 1 to match when minutes equal `M` and seconds equal `S`.

```text
alarm minsec 12 30
```

#### `alarm hms H M S`

Configure Alarm 1 to match when hours, minutes, and seconds match.

```text
alarm hms 8 15 0
```

After a match, use:

```text
status
```

to see `A1F = 1`, and:

```text
clearflags
```

to clear it.

---

## RTC-as-computation experiments

These commands are intentionally playful and educational. They treat the RTC as a very slow physical counter/comparator.

#### `rtcadd A B`

Set the RTC to `00:00:A`, wait `B` seconds, then read the elapsed RTC time as `A+B`.

```text
rtcadd 10 7
```

Expected result:

```text
Interpreted result A+B = 17
```

#### `rtadd A B`

Alias for `rtcadd` if included in your sketch version.

#### `rtcsub A B`

Set the RTC to `00:00:00`, wait `B` seconds, then print `A - elapsed_seconds`.

```text
rtcsub 20 7
```

Expected result:

```text
RESULT A-B = 13
```

#### `rtcmp A B`

Use Alarm 1 as a comparator.

The sketch sets an alarm at `A` seconds, waits `B` seconds, and then checks whether Alarm 1 flag `A1F` was set.

```text
rtcmp 10 12
```

If `A1F = 1`, then:

```text
B >= A
```

If `A1F = 0`, then:

```text
B < A
```

This is a fun way to use the DS3231 as a slow hardware comparator.

---

## EEPROM/FRAM commands

Many DS3231 modules include an additional I²C EEPROM, commonly an AT24C32. This is not part of the DS3231 chip. Instead it is a separate I²C slave on the same board.

The DS3231 itself cannot write to this EEPROM because the DS3231 is an I²C slave, not a master. The Nano performs the EEPROM reads/writes.

#### `eeprobe`

Probe addresses `0x50..0x57` for EEPROM/FRAM.

```text
eeprobe
```

Typical output:

```text
Found possible EEPROM/FRAM at 0x57
```

#### `eeget dev mem len`

Read `len` bytes from EEPROM/FRAM device `dev` starting at 16-bit memory address `mem`.

```text
eeget 57 0 16
```

#### `eeput dev mem val`

Write one byte to EEPROM/FRAM.

```text
eeput 57 0 42
```

Then verify:

```text
eeget 57 0 1
```

EEPROM has limited write endurance. Avoid tight write loops. FRAM is much more tolerant of frequent writes.

---

## RTC-only LED blinker idea

After configuring:

```text
sqw 1
```

you can use `INT/SQW` as a 1 Hz blinking output.

Because `INT/SQW` is open-drain, a common LED test circuit is:

```text
VCC
 |
LED
 |
resistor, e.g. 1k to 4.7k
 |
INT/SQW
```

The DS3231 sinks current when the output is low, so the LED will blink inverted relative to the square wave.

Use a larger resistor for low current. Do not drive high-current loads directly from the DS3231 pin.

---

## Number formats

Numbers may be entered as decimal or hexadecimal.

Examples:

```text
68
0x68
FF
0xFF
```

Most DS3231 register values are shown in hexadecimal. Time and date values inside the DS3231 are stored in BCD format.

---

## Notes and limitations

- The DS3231 is an I²C slave. It cannot actively talk to EEPROMs, displays, or other I²C devices by itself.
- The Nano is the I²C master and serial terminal interface.
- The RTC can continue running on `VBAT` backup power after being configured.
- Alarm register settings are retained as long as the DS3231 remains powered by `VCC` or `VBAT`.
- Whether `INT/SQW` works during battery-only operation depends on `BBSQW`, module wiring, pull-ups, and what the external circuit is powered from.
- Cheap DS3231 modules may include power LEDs, charging circuits, and pull-ups that are not ideal for ultra-low-power operation.
