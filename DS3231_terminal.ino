#include <Wire.h>

#define DS3231_ADDR 0x68

String line;

uint8_t bcdToDec(uint8_t v) {
  return ((v >> 4) * 10) + (v & 0x0F);
}

uint8_t decToBcd(uint8_t v) {
  return ((v / 10) << 4) | (v % 10);
}

void printHex2(uint8_t v) {
  if (v < 16) Serial.print('0');
  Serial.print(v, HEX);
}

void printHex4(uint16_t v) {
  if (v < 0x1000) Serial.print('0');
  if (v < 0x0100) Serial.print('0');
  if (v < 0x0010) Serial.print('0');
  Serial.print(v, HEX);
}

long parseNumber(const String &s) {
  String t = s;
  t.trim();
  if (t.startsWith("0x") || t.startsWith("0X")) {
    return strtol(t.c_str(), nullptr, 16);
  }

  // Treat values containing A-F as hex.
  bool hasHexLetter = false;
  for (unsigned int i = 0; i < t.length(); i++) {
    char c = t.charAt(i);
    if ((c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
      hasHexLetter = true;
      break;
    }
  }

  if (hasHexLetter) {
    return strtol(t.c_str(), nullptr, 16);
  }

  return strtol(t.c_str(), nullptr, 10);
}

int splitTokens(String input, String tokens[], int maxTokens) {
  input.trim();
  int count = 0;

  while (input.length() > 0 && count < maxTokens) {
    input.trim();
    int idx = input.indexOf(' ');
    if (idx < 0) {
      tokens[count++] = input;
      break;
    }

    String token = input.substring(0, idx);
    token.trim();
    if (token.length() > 0) {
      tokens[count++] = token;
    }

    input = input.substring(idx + 1);
  }

  return count;
}

bool i2cReadReg(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  uint8_t err = Wire.endTransmission(false); // repeated start
  if (err != 0) return false;

  uint8_t got = Wire.requestFrom(addr, len);
  if (got != len) return false;

  for (uint8_t i = 0; i < len; i++) {
    buf[i] = Wire.read();
  }
  return true;
}

bool i2cWriteReg(uint8_t addr, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool eepromRead(uint8_t devAddr, uint16_t memAddr, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(devAddr);
  Wire.write((uint8_t)(memAddr >> 8));
  Wire.write((uint8_t)(memAddr & 0xFF));
  uint8_t err = Wire.endTransmission(false); // repeated start
  if (err != 0) return false;

  uint8_t got = Wire.requestFrom(devAddr, len);
  if (got != len) return false;

  for (uint8_t i = 0; i < len; i++) {
    buf[i] = Wire.read();
  }
  return true;
}

bool eepromWriteByte(uint8_t devAddr, uint16_t memAddr, uint8_t value) {
  Wire.beginTransmission(devAddr);
  Wire.write((uint8_t)(memAddr >> 8));
  Wire.write((uint8_t)(memAddr & 0xFF));
  Wire.write(value);
  uint8_t err = Wire.endTransmission();

  // Typical EEPROM write cycle is a few milliseconds. FRAM ignores this delay.
  delay(6);
  return err == 0;
}

void cmdScan() {
  Serial.println(F("Scanning I2C bus..."));
  int found = 0;

  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();

    if (err == 0) {
      Serial.print(F("Found 0x"));
      printHex2(addr);

      if (addr == 0x68) Serial.print(F("  DS3231/RTC?"));
      if (addr >= 0x50 && addr <= 0x57) Serial.print(F("  EEPROM/FRAM?"));
      if (addr == 0x27 || addr == 0x3F) Serial.print(F("  I2C LCD?"));
      if (addr == 0x3C || addr == 0x3D) Serial.print(F("  OLED?"));

      Serial.println();
      found++;
    }
  }

  if (found == 0) Serial.println(F("No devices found."));
}

void cmdTime() {
  uint8_t b[7];
  if (!i2cReadReg(DS3231_ADDR, 0x00, b, 7)) {
    Serial.println(F("ERROR: Could not read DS3231 at 0x68"));
    return;
  }

  uint8_t sec   = bcdToDec(b[0] & 0x7F);
  uint8_t min   = bcdToDec(b[1] & 0x7F);
  uint8_t hour  = bcdToDec(b[2] & 0x3F);
  uint8_t dow   = bcdToDec(b[3] & 0x07);
  uint8_t date  = bcdToDec(b[4] & 0x3F);
  uint8_t month = bcdToDec(b[5] & 0x1F);
  uint16_t year = 2000 + bcdToDec(b[6]);

  Serial.print(F("TIME "));
  Serial.print(year);
  Serial.print('-');
  if (month < 10) Serial.print('0');
  Serial.print(month);
  Serial.print('-');
  if (date < 10) Serial.print('0');
  Serial.print(date);
  Serial.print(' ');
  if (hour < 10) Serial.print('0');
  Serial.print(hour);
  Serial.print(':');
  if (min < 10) Serial.print('0');
  Serial.print(min);
  Serial.print(':');
  if (sec < 10) Serial.print('0');
  Serial.print(sec);
  Serial.print(F("  DOW="));
  Serial.println(dow);
}

void cmdDump() {
  uint8_t b[0x13];
  if (!i2cReadReg(DS3231_ADDR, 0x00, b, sizeof(b))) {
    Serial.println(F("ERROR: Could not read DS3231 registers"));
    return;
  }

  Serial.println(F("DS3231 register dump:"));
  for (uint8_t i = 0; i < sizeof(b); i++) {
    Serial.print(F("0x"));
    printHex2(i);
    Serial.print(F(": 0x"));
    printHex2(b[i]);

    if (i == 0x00) Serial.print(F(" seconds"));
    if (i == 0x01) Serial.print(F(" minutes"));
    if (i == 0x02) Serial.print(F(" hours"));
    if (i == 0x0E) Serial.print(F(" control"));
    if (i == 0x0F) Serial.print(F(" status"));
    if (i == 0x10) Serial.print(F(" aging offset"));
    if (i == 0x11) Serial.print(F(" temp MSB"));
    if (i == 0x12) Serial.print(F(" temp LSB"));

    Serial.println();
  }
}

void cmdTemp() {
  uint8_t b[2];
  if (!i2cReadReg(DS3231_ADDR, 0x11, b, 2)) {
    Serial.println(F("ERROR: Could not read temperature registers"));
    return;
  }

  int8_t msb = (int8_t)b[0];
  float temp = msb + ((b[1] >> 6) * 0.25);

  Serial.print(F("TEMP "));
  Serial.print(temp, 2);
  Serial.println(F(" C"));
}

void cmdSet(String tokens[], int n) {
  if (n != 7) {
    Serial.println(F("Usage: set YYYY MM DD HH MM SS"));
    return;
  }

  int year  = parseNumber(tokens[1]);
  int month = parseNumber(tokens[2]);
  int day   = parseNumber(tokens[3]);
  int hour  = parseNumber(tokens[4]);
  int min   = parseNumber(tokens[5]);
  int sec   = parseNumber(tokens[6]);

  if (year < 2000 || year > 2099 || month < 1 || month > 12 ||
      day < 1 || day > 31 || hour < 0 || hour > 23 ||
      min < 0 || min > 59 || sec < 0 || sec > 59) {
    Serial.println(F("ERROR: Invalid date/time"));
    return;
  }

  Wire.beginTransmission(DS3231_ADDR);
  Wire.write(0x00);
  Wire.write(decToBcd(sec));
  Wire.write(decToBcd(min));
  Wire.write(decToBcd(hour));
  Wire.write(decToBcd(1)); // day of week placeholder
  Wire.write(decToBcd(day));
  Wire.write(decToBcd(month));
  Wire.write(decToBcd(year - 2000));
  uint8_t err = Wire.endTransmission();

  if (err == 0) {
    Serial.println(F("OK: DS3231 time set"));
  } else {
    Serial.print(F("ERROR: I2C write failed, code "));
    Serial.println(err);
  }
}

void cmdRead(String tokens[], int n) {
  if (n != 4) {
    Serial.println(F("Usage: r addr reg len"));
    Serial.println(F("Example: r 68 00 07"));
    return;
  }

  uint8_t addr = parseNumber(tokens[1]);
  uint8_t reg  = parseNumber(tokens[2]);
  uint8_t len  = parseNumber(tokens[3]);

  if (len == 0 || len > 32) {
    Serial.println(F("ERROR: len must be 1..32"));
    return;
  }

  uint8_t buf[32];
  if (!i2cReadReg(addr, reg, buf, len)) {
    Serial.println(F("ERROR: I2C read failed"));
    return;
  }

  Serial.print(F("READ 0x"));
  printHex2(addr);
  Serial.print(F(" reg 0x"));
  printHex2(reg);
  Serial.print(F(": "));

  for (uint8_t i = 0; i < len; i++) {
    Serial.print(F("0x"));
    printHex2(buf[i]);
    if (i + 1 < len) Serial.print(' ');
  }
  Serial.println();
}

void cmdWrite(String tokens[], int n) {
  if (n != 4) {
    Serial.println(F("Usage: w addr reg value"));
    Serial.println(F("Example: w 68 0E 00"));
    return;
  }

  uint8_t addr  = parseNumber(tokens[1]);
  uint8_t reg   = parseNumber(tokens[2]);
  uint8_t value = parseNumber(tokens[3]);

  if (i2cWriteReg(addr, reg, value)) {
    Serial.println(F("OK"));
  } else {
    Serial.println(F("ERROR: I2C write failed"));
  }
}

void cmdStatus() {
  uint8_t s;
  if (!i2cReadReg(DS3231_ADDR, 0x0F, &s, 1)) {
    Serial.println(F("ERROR: Could not read status register"));
    return;
  }

  Serial.print(F("STATUS 0x"));
  printHex2(s);
  Serial.println();

  Serial.print(F("  OSF     = ")); Serial.println((s & 0x80) ? F("1 oscillator stopped/time may be invalid") : F("0 time likely valid"));
  Serial.print(F("  EN32kHz = ")); Serial.println((s & 0x08) ? F("1 enabled") : F("0 disabled"));
  Serial.print(F("  BSY     = ")); Serial.println((s & 0x04) ? F("1 busy") : F("0 idle"));
  Serial.print(F("  A2F     = ")); Serial.println((s & 0x02) ? F("1 alarm2 flag") : F("0"));
  Serial.print(F("  A1F     = ")); Serial.println((s & 0x01) ? F("1 alarm1 flag") : F("0"));
}

void cmdCtrl() {
  uint8_t c;
  if (!i2cReadReg(DS3231_ADDR, 0x0E, &c, 1)) {
    Serial.println(F("ERROR: Could not read control register"));
    return;
  }

  Serial.print(F("CTRL 0x"));
  printHex2(c);
  Serial.println();

  Serial.print(F("  EOSC  = ")); Serial.println((c & 0x80) ? F("1 stop oscillator on battery") : F("0 oscillator enabled"));
  Serial.print(F("  BBSQW = ")); Serial.println((c & 0x40) ? F("1 battery square-wave enabled") : F("0 disabled"));
  Serial.print(F("  CONV  = ")); Serial.println((c & 0x20) ? F("1 temperature conversion in progress/requested") : F("0"));
  Serial.print(F("  RS2   = ")); Serial.println((c & 0x10) ? F("1") : F("0"));
  Serial.print(F("  RS1   = ")); Serial.println((c & 0x08) ? F("1") : F("0"));
  Serial.print(F("  INTCN = ")); Serial.println((c & 0x04) ? F("1 interrupt/alarm mode") : F("0 square-wave mode"));
  Serial.print(F("  A2IE  = ")); Serial.println((c & 0x02) ? F("1") : F("0"));
  Serial.print(F("  A1IE  = ")); Serial.println((c & 0x01) ? F("1") : F("0"));

  if ((c & 0x04) == 0) {
    uint8_t rs = (c >> 3) & 0x03;
    Serial.print(F("  SQW frequency = "));
    if (rs == 0) Serial.println(F("1 Hz"));
    if (rs == 1) Serial.println(F("1.024 kHz"));
    if (rs == 2) Serial.println(F("4.096 kHz"));
    if (rs == 3) Serial.println(F("8.192 kHz"));
  }
}

void cmdClearFlags() {
  uint8_t s;
  if (!i2cReadReg(DS3231_ADDR, 0x0F, &s, 1)) {
    Serial.println(F("ERROR: Could not read status register"));
    return;
  }

  // Clear OSF, A2F, and A1F. Preserve EN32kHz. BSY is read-only.
  s &= ~0x83;

  if (i2cWriteReg(DS3231_ADDR, 0x0F, s)) {
    Serial.println(F("OK: OSF/A2F/A1F cleared"));
  } else {
    Serial.println(F("ERROR: Could not write status register"));
  }
}

void cmdSqw(String tokens[], int n) {
  if (n != 2) {
    Serial.println(F("Usage: sqw 1|1024|4096|8192|off"));
    return;
  }

  String f = tokens[1];
  f.toLowerCase();

  uint8_t c;
  if (!i2cReadReg(DS3231_ADDR, 0x0E, &c, 1)) {
    Serial.println(F("ERROR: Could not read control register"));
    return;
  }

  if (f == "off") {
    c |= 0x04;   // INTCN = 1, interrupt mode
    c &= ~0x03;  // disable alarm interrupt output
  } else {
    c &= ~0x04;  // INTCN = 0, square-wave mode
    c &= ~0x18;  // clear RS2/RS1

    long hz = parseNumber(f);
    if (hz == 1) {
      c |= 0x00;
    } else if (hz == 1024) {
      c |= 0x08;
    } else if (hz == 4096) {
      c |= 0x10;
    } else if (hz == 8192) {
      c |= 0x18;
    } else {
      Serial.println(F("ERROR: valid values are 1, 1024, 4096, 8192, off"));
      return;
    }
  }

  if (i2cWriteReg(DS3231_ADDR, 0x0E, c)) {
    Serial.println(F("OK: SQW/INT mode updated"));
  } else {
    Serial.println(F("ERROR: Could not write control register"));
  }
}

void cmd32k(String tokens[], int n) {
  if (n != 2) {
    Serial.println(F("Usage: 32k on|off"));
    return;
  }

  uint8_t s;
  if (!i2cReadReg(DS3231_ADDR, 0x0F, &s, 1)) {
    Serial.println(F("ERROR: Could not read status register"));
    return;
  }

  tokens[1].toLowerCase();
  if (tokens[1] == "on") {
    s |= 0x08;
  } else if (tokens[1] == "off") {
    s &= ~0x08;
  } else {
    Serial.println(F("Usage: 32k on|off"));
    return;
  }

  if (i2cWriteReg(DS3231_ADDR, 0x0F, s)) {
    Serial.println(F("OK: 32kHz output updated"));
  } else {
    Serial.println(F("ERROR: Could not write status register"));
  }
}

void cmdBbsqw(String tokens[], int n) {
  if (n != 2) {
    Serial.println(F("Usage: bbsqw on|off"));
    Serial.println(F("Meaning: enable/disable battery-backed INT/SQW output when VCC is absent."));
    return;
  }

  uint8_t c;
  if (!i2cReadReg(DS3231_ADDR, 0x0E, &c, 1)) {
    Serial.println(F("ERROR: Could not read control register"));
    return;
  }

  tokens[1].toLowerCase();
  if (tokens[1] == "on") {
    c |= 0x40;   // BBSQW = 1
  } else if (tokens[1] == "off") {
    c &= ~0x40;  // BBSQW = 0
  } else {
    Serial.println(F("Usage: bbsqw on|off"));
    return;
  }

  if (i2cWriteReg(DS3231_ADDR, 0x0E, c)) {
    Serial.println(F("OK: BBSQW updated"));
    cmdCtrl();
  } else {
    Serial.println(F("ERROR: Could not write control register"));
  }
}

void cmdConvTemp() {
  uint8_t c;
  if (!i2cReadReg(DS3231_ADDR, 0x0E, &c, 1)) {
    Serial.println(F("ERROR: Could not read control register"));
    return;
  }

  c |= 0x20; // CONV bit

  if (!i2cWriteReg(DS3231_ADDR, 0x0E, c)) {
    Serial.println(F("ERROR: Could not start conversion"));
    return;
  }

  Serial.println(F("OK: conversion started. Waiting..."));

  for (int i = 0; i < 50; i++) {
    uint8_t status;
    if (i2cReadReg(DS3231_ADDR, 0x0F, &status, 1) && ((status & 0x04) == 0)) {
      break;
    }
    delay(10);
  }

  cmdTemp();
}

void cmdAging(String tokens[], int n) {
  if (n == 1) {
    uint8_t raw;
    if (!i2cReadReg(DS3231_ADDR, 0x10, &raw, 1)) {
      Serial.println(F("ERROR: Could not read aging register"));
      return;
    }

    int8_t signedValue = (int8_t)raw;
    Serial.print(F("AGING raw=0x"));
    printHex2(raw);
    Serial.print(F(" signed="));
    Serial.println(signedValue);
    return;
  }

  if (n != 2) {
    Serial.println(F("Usage: aging [signed_value]"));
    Serial.println(F("Example: aging -5"));
    return;
  }

  int value = parseNumber(tokens[1]);
  if (value < -128 || value > 127) {
    Serial.println(F("ERROR: aging value must be -128..127"));
    return;
  }

  uint8_t raw = (uint8_t)((int8_t)value);
  if (!i2cWriteReg(DS3231_ADDR, 0x10, raw)) {
    Serial.println(F("ERROR: Could not write aging register"));
    return;
  }

  Serial.print(F("OK: aging set to "));
  Serial.print(value);
  Serial.print(F(" raw=0x"));
  printHex2(raw);
  Serial.println();

  // Start a manual conversion so the new aging value takes effect immediately.
  cmdConvTemp();
}

void cmdRtcAdd(String tokens[], int n) {
  if (n != 3) {
    Serial.println(F("Usage: rtcadd A B"));
    Serial.println(F("Example: rtcadd 10 7"));
    Serial.println(F("Meaning: set RTC seconds to A, wait B seconds, read A+B."));
    return;
  }

  int a = parseNumber(tokens[1]);
  int b = parseNumber(tokens[2]);

  if (a < 0 || a > 59 || b < 0 || b > 120) {
    Serial.println(F("ERROR: A must be 0..59, B must be 0..120"));
    return;
  }

  Wire.beginTransmission(DS3231_ADDR);
  Wire.write(0x00);
  Wire.write(decToBcd(a));  // seconds = A
  Wire.write(decToBcd(0));  // minutes
  Wire.write(decToBcd(0));  // hours
  Wire.write(decToBcd(1));  // day of week
  Wire.write(decToBcd(1));  // day
  Wire.write(decToBcd(1));  // month
  Wire.write(decToBcd(26)); // year
  uint8_t err = Wire.endTransmission();

  if (err != 0) {
    Serial.print(F("ERROR: Could not initialize RTC, I2C error "));
    Serial.println(err);
    return;
  }

  Serial.print(F("RTC set to 00:00:"));
  if (a < 10) Serial.print('0');
  Serial.println(a);

  Serial.print(F("Waiting "));
  Serial.print(b);
  Serial.println(F(" seconds..."));

  delay((unsigned long)b * 1000UL);

  uint8_t t[3];
  if (!i2cReadReg(DS3231_ADDR, 0x00, t, 3)) {
    Serial.println(F("ERROR: Could not read RTC"));
    return;
  }

  int sec = bcdToDec(t[0] & 0x7F);
  int min = bcdToDec(t[1] & 0x7F);
  int hour = bcdToDec(t[2] & 0x3F);
  int total = hour * 3600 + min * 60 + sec;

  Serial.print(F("RTC result time: "));
  if (hour < 10) Serial.print('0');
  Serial.print(hour);
  Serial.print(':');
  if (min < 10) Serial.print('0');
  Serial.print(min);
  Serial.print(':');
  if (sec < 10) Serial.print('0');
  Serial.println(sec);

  Serial.print(F("Interpreted result A+B = "));
  Serial.println(total);
}

void writeFixedTimeForRtcExperiment(uint8_t sec, uint8_t min, uint8_t hour) {
  Wire.beginTransmission(DS3231_ADDR);
  Wire.write(0x00);
  Wire.write(decToBcd(sec));
  Wire.write(decToBcd(min));
  Wire.write(decToBcd(hour));
  Wire.write(decToBcd(1));  // day of week placeholder
  Wire.write(decToBcd(1));  // day
  Wire.write(decToBcd(1));  // month
  Wire.write(decToBcd(26)); // year
  Wire.endTransmission();
}

bool clearAlarmFlagsOnly() {
  uint8_t s;
  if (!i2cReadReg(DS3231_ADDR, 0x0F, &s, 1)) return false;

  // Clear A2F and A1F. Preserve OSF, EN32kHz and other bits.
  s &= ~0x03;
  return i2cWriteReg(DS3231_ADDR, 0x0F, s);
}

bool enableAlarm1Interrupt() {
  uint8_t c;
  if (!i2cReadReg(DS3231_ADDR, 0x0E, &c, 1)) return false;

  c |= 0x04;  // INTCN = 1, interrupt/alarm mode
  c |= 0x01;  // A1IE = 1
  return i2cWriteReg(DS3231_ADDR, 0x0E, c);
}

bool setAlarm1Registers(uint8_t secReg, uint8_t minReg, uint8_t hourReg, uint8_t dayDateReg) {
  Wire.beginTransmission(DS3231_ADDR);
  Wire.write(0x07);
  Wire.write(secReg);
  Wire.write(minReg);
  Wire.write(hourReg);
  Wire.write(dayDateReg);
  return Wire.endTransmission() == 0;
}

void cmdAlarm(String tokens[], int n) {
  if (n < 2) {
    Serial.println(F("Usage:"));
    Serial.println(F("  alarm everysec"));
    Serial.println(F("  alarm sec S"));
    Serial.println(F("  alarm minsec M S"));
    Serial.println(F("  alarm hms H M S"));
    return;
  }

  tokens[1].toLowerCase();

  bool ok = false;

  if (tokens[1] == "everysec") {
    // A1M1..A1M4 = 1: alarm once per second.
    ok = setAlarm1Registers(0x80, 0x80, 0x80, 0x80);
  } else if (tokens[1] == "sec") {
    if (n != 3) {
      Serial.println(F("Usage: alarm sec S"));
      return;
    }
    int s = parseNumber(tokens[2]);
    if (s < 0 || s > 59) {
      Serial.println(F("ERROR: seconds must be 0..59"));
      return;
    }
    // A1M1=0, A1M2..A1M4=1: alarm when seconds match.
    ok = setAlarm1Registers(decToBcd(s), 0x80, 0x80, 0x80);
  } else if (tokens[1] == "minsec") {
    if (n != 4) {
      Serial.println(F("Usage: alarm minsec M S"));
      return;
    }
    int m = parseNumber(tokens[2]);
    int s = parseNumber(tokens[3]);
    if (m < 0 || m > 59 || s < 0 || s > 59) {
      Serial.println(F("ERROR: minutes and seconds must be 0..59"));
      return;
    }
    // A1M1=0, A1M2=0, A1M3/A1M4=1: alarm when minutes and seconds match.
    ok = setAlarm1Registers(decToBcd(s), decToBcd(m), 0x80, 0x80);
  } else if (tokens[1] == "hms") {
    if (n != 5) {
      Serial.println(F("Usage: alarm hms H M S"));
      return;
    }
    int h = parseNumber(tokens[2]);
    int m = parseNumber(tokens[3]);
    int s = parseNumber(tokens[4]);
    if (h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 59) {
      Serial.println(F("ERROR: hour must be 0..23; minutes/seconds must be 0..59"));
      return;
    }
    // A1M1=0, A1M2=0, A1M3=0, A1M4=1: alarm when hours, minutes and seconds match.
    ok = setAlarm1Registers(decToBcd(s), decToBcd(m), decToBcd(h), 0x80);
  } else {
    Serial.println(F("ERROR: unknown alarm mode. Use everysec, sec, minsec or hms."));
    return;
  }

  if (!ok) {
    Serial.println(F("ERROR: could not write Alarm 1 registers"));
    return;
  }

  if (!clearAlarmFlagsOnly()) {
    Serial.println(F("ERROR: could not clear alarm flags"));
    return;
  }

  if (!enableAlarm1Interrupt()) {
    Serial.println(F("ERROR: could not enable Alarm 1 interrupt mode"));
    return;
  }

  Serial.println(F("OK: Alarm 1 configured and A1IE/INTCN enabled"));
}

void cmdRtCmp(String tokens[], int n) {
  if (n != 3) {
    Serial.println(F("Usage: rtcmp A B"));
    Serial.println(F("Meaning: alarm at A seconds, wait B seconds, report whether B >= A."));
    return;
  }

  int a = parseNumber(tokens[1]);
  int b = parseNumber(tokens[2]);

  if (a < 0 || a > 59 || b < 0 || b > 120) {
    Serial.println(F("ERROR: A must be 0..59, B must be 0..120"));
    return;
  }

  writeFixedTimeForRtcExperiment(0, 0, 0);

  // Alarm 1: match seconds == A.
  if (!setAlarm1Registers(decToBcd(a), 0x80, 0x80, 0x80)) {
    Serial.println(F("ERROR: could not set Alarm 1 registers"));
    return;
  }
  if (!clearAlarmFlagsOnly()) {
    Serial.println(F("ERROR: could not clear alarm flags"));
    return;
  }
  if (!enableAlarm1Interrupt()) {
    Serial.println(F("ERROR: could not enable Alarm 1 interrupt mode"));
    return;
  }

  Serial.print(F("RTC comparator: alarm at "));
  Serial.print(a);
  Serial.print(F(" seconds; waiting "));
  Serial.print(b);
  Serial.println(F(" seconds..."));

  delay((unsigned long)b * 1000UL);

  uint8_t status;
  if (!i2cReadReg(DS3231_ADDR, 0x0F, &status, 1)) {
    Serial.println(F("ERROR: could not read status register"));
    return;
  }

  bool matched = (status & 0x01) != 0;
  Serial.print(F("A1F="));
  Serial.println(matched ? F("1") : F("0"));

  if (matched) {
    Serial.println(F("RESULT: B >= A"));
  } else {
    Serial.println(F("RESULT: B < A"));
  }
}

void cmdRtcSub(String tokens[], int n) {
  if (n != 3) {
    Serial.println(F("Usage: rtcsub A B"));
    Serial.println(F("Meaning: set RTC to 00:00:00, wait B seconds, return A - elapsed seconds."));
    return;
  }

  int a = parseNumber(tokens[1]);
  int b = parseNumber(tokens[2]);

  if (a < 0 || a > 3600 || b < 0 || b > 3600) {
    Serial.println(F("ERROR: A and B must be 0..3600"));
    return;
  }

  writeFixedTimeForRtcExperiment(0, 0, 0);

  Serial.print(F("RTC subtraction: A="));
  Serial.print(a);
  Serial.print(F(", waiting B="));
  Serial.print(b);
  Serial.println(F(" seconds..."));

  delay((unsigned long)b * 1000UL);

  uint8_t t[3];
  if (!i2cReadReg(DS3231_ADDR, 0x00, t, 3)) {
    Serial.println(F("ERROR: Could not read RTC"));
    return;
  }

  int sec = bcdToDec(t[0] & 0x7F);
  int min = bcdToDec(t[1] & 0x7F);
  int hour = bcdToDec(t[2] & 0x3F);
  int elapsed = hour * 3600 + min * 60 + sec;
  int result = a - elapsed;

  Serial.print(F("Elapsed by RTC = "));
  Serial.print(elapsed);
  Serial.println(F(" seconds"));
  Serial.print(F("RESULT A-B = "));
  Serial.println(result);
}

void cmdEeProbe() {
  Serial.println(F("Probing EEPROM/FRAM range 0x50..0x57..."));

  int found = 0;
  for (uint8_t addr = 0x50; addr <= 0x57; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();

    if (err == 0) {
      Serial.print(F("Found possible EEPROM/FRAM at 0x"));
      printHex2(addr);
      Serial.println();
      found++;
    }
  }

  if (found == 0) {
    Serial.println(F("No EEPROM/FRAM found in 0x50..0x57."));
  }
}

void cmdEeGet(String tokens[], int n) {
  if (n != 4) {
    Serial.println(F("Usage: eeget dev mem len"));
    Serial.println(F("Example: eeget 57 0 16"));
    return;
  }

  uint8_t dev = parseNumber(tokens[1]);
  uint16_t mem = parseNumber(tokens[2]);
  uint8_t len = parseNumber(tokens[3]);

  if (len == 0 || len > 32) {
    Serial.println(F("ERROR: len must be 1..32"));
    return;
  }

  uint8_t buf[32];
  if (!eepromRead(dev, mem, buf, len)) {
    Serial.println(F("ERROR: EEPROM/FRAM read failed"));
    return;
  }

  Serial.print(F("EEPROM/FRAM 0x"));
  printHex2(dev);
  Serial.print(F(" @0x"));
  printHex4(mem);
  Serial.print(F(": "));

  for (uint8_t i = 0; i < len; i++) {
    Serial.print(F("0x"));
    printHex2(buf[i]);
    if (i + 1 < len) Serial.print(' ');
  }
  Serial.println();
}

void cmdEePut(String tokens[], int n) {
  if (n != 4) {
    Serial.println(F("Usage: eeput dev mem value"));
    Serial.println(F("Example: eeput 57 0 42"));
    return;
  }

  uint8_t dev = parseNumber(tokens[1]);
  uint16_t mem = parseNumber(tokens[2]);
  uint8_t value = parseNumber(tokens[3]);

  if (eepromWriteByte(dev, mem, value)) {
    Serial.println(F("OK: EEPROM/FRAM byte written"));
  } else {
    Serial.println(F("ERROR: EEPROM/FRAM write failed"));
  }
}

void printHelp() {
  Serial.println(F("Nano I2C/DS3231 Interface - commands"));
  Serial.println();

  Serial.println(F("General I2C:"));
  Serial.println(F("  help"));
  Serial.println(F("    Show this help text."));
  Serial.println(F("  scan"));
  Serial.println(F("    Scan the I2C bus for devices. Expected DS3231 address: 0x68."));
  Serial.println(F("  r addr reg len"));
  Serial.println(F("    Read len bytes from register reg of I2C device addr."));
  Serial.println(F("    Example: r 68 00 07"));
  Serial.println(F("  w addr reg value"));
  Serial.println(F("    Write one byte value to register reg of I2C device addr."));
  Serial.println(F("    Example: w 68 0E 00"));
  Serial.println();

  Serial.println(F("DS3231 time/date:"));
  Serial.println(F("  time"));
  Serial.println(F("    Read and print the current RTC date/time."));
  Serial.println(F("  set YYYY MM DD HH MM SS"));
  Serial.println(F("    Set the RTC date/time. Day-of-week is set to 1 internally."));
  Serial.println(F("    Example: set 2026 04 29 23 30 00"));
  Serial.println(F("  dump"));
  Serial.println(F("    Dump DS3231 registers 0x00..0x12 in hex."));
  Serial.println();

  Serial.println(F("Temperature / TCXO:"));
  Serial.println(F("  temp"));
  Serial.println(F("    Read the DS3231 internal temperature registers."));
  Serial.println(F("  convtemp"));
  Serial.println(F("    Force a manual temperature conversion, then print temperature."));
  Serial.println(F("  aging"));
  Serial.println(F("    Read the signed aging-offset register 0x10."));
  Serial.println(F("  aging -5"));
  Serial.println(F("    Set aging-offset to -5 and trigger a conversion."));
  Serial.println(F("    Use with care; note the old value before changing it."));
  Serial.println();

  Serial.println(F("Status / control:"));
  Serial.println(F("  status"));
  Serial.println(F("    Decode status register 0x0F: OSF, EN32kHz, BSY, A2F, A1F."));
  Serial.println(F("  ctrl"));
  Serial.println(F("    Decode control register 0x0E: EOSC, BBSQW, CONV, RS2/RS1, INTCN, A2IE, A1IE."));
  Serial.println(F("  clearflags"));
  Serial.println(F("    Clear OSF, A2F and A1F in the status register."));
  Serial.println();

  Serial.println(F("Clock outputs:"));
  Serial.println(F("  sqw 1"));
  Serial.println(F("    Set INT/SQW pin to 1 Hz square-wave output."));
  Serial.println(F("  sqw 1024"));
  Serial.println(F("    Set INT/SQW pin to 1.024 kHz square-wave output."));
  Serial.println(F("  sqw 4096"));
  Serial.println(F("    Set INT/SQW pin to 4.096 kHz square-wave output."));
  Serial.println(F("  sqw 8192"));
  Serial.println(F("    Set INT/SQW pin to 8.192 kHz square-wave output."));
  Serial.println(F("  sqw off"));
  Serial.println(F("    Return INT/SQW to interrupt/alarm mode and disable alarm interrupt output bits."));
  Serial.println(F("  bbsqw on"));
  Serial.println(F("    Enable battery-backed INT/SQW output when VCC is absent."));
  Serial.println(F("    Useful only if your module exposes INT/SQW and is wired appropriately."));
  Serial.println(F("  bbsqw off"));
  Serial.println(F("    Disable battery-backed INT/SQW output when VCC is absent."));
  Serial.println(F("  32k on"));
  Serial.println(F("    Enable the 32.768 kHz output pin, if your module exposes it."));
  Serial.println(F("  32k off"));
  Serial.println(F("    Disable the 32.768 kHz output pin."));
  Serial.println();

  Serial.println(F("Alarm 1 patterns:"));
  Serial.println(F("  alarm everysec"));
  Serial.println(F("    Configure Alarm 1 to match once per second."));
  Serial.println(F("  alarm sec S"));
  Serial.println(F("    Configure Alarm 1 to match when seconds == S."));
  Serial.println(F("    Example: alarm sec 30"));
  Serial.println(F("  alarm minsec M S"));
  Serial.println(F("    Configure Alarm 1 to match when minutes == M and seconds == S."));
  Serial.println(F("    Example: alarm minsec 12 30"));
  Serial.println(F("  alarm hms H M S"));
  Serial.println(F("    Configure Alarm 1 to match when hours == H, minutes == M, seconds == S."));
  Serial.println(F("    Example: alarm hms 8 15 0"));
  Serial.println(F("  After an alarm match, use status to see A1F and clearflags to clear it."));
  Serial.println();

  Serial.println(F("RTC-as-computation experiments:"));
  Serial.println(F("  rtcadd A B"));
  Serial.println(F("    Set RTC to 00:00:A, wait B seconds, read A+B as elapsed RTC time."));
  Serial.println(F("    Example: rtcadd 10 7"));
  Serial.println(F("  rtcsub A B"));
  Serial.println(F("    Set RTC to 00:00:00, wait B seconds, print A - elapsed seconds."));
  Serial.println(F("    Example: rtcsub 20 7"));
  Serial.println(F("  rtcmp A B"));
  Serial.println(F("    Set an alarm at A seconds, wait B seconds, report whether B >= A using A1F."));
  Serial.println(F("    Example: rtcmp 10 12"));
  Serial.println();

  Serial.println(F("EEPROM/FRAM at 0x50..0x57:"));
  Serial.println(F("  eeprobe"));
  Serial.println(F("    Probe addresses 0x50..0x57 for EEPROM/FRAM, e.g. AT24C32 on some DS3231 modules."));
  Serial.println(F("  eeget dev mem len"));
  Serial.println(F("    Read len bytes from EEPROM/FRAM dev at 16-bit memory address mem."));
  Serial.println(F("    Example: eeget 57 0 16"));
  Serial.println(F("  eeput dev mem val"));
  Serial.println(F("    Write one byte val to EEPROM/FRAM dev at 16-bit memory address mem."));
  Serial.println(F("    Example: eeput 57 0 42"));
  Serial.println(F("    EEPROM writes wear the chip; avoid tight write loops."));
  Serial.println();

  Serial.println(F("Number format:"));
  Serial.println(F("  Numbers may be decimal or hex. Examples: 68, 0x68, FF, 0xFF."));
  Serial.println(F("  Most DS3231 register values are shown in hex; time/date values are BCD internally."));
}

void handleLine(String input) {
  input.trim();
  if (input.length() == 0) return;

  String tokens[10];
  int n = splitTokens(input, tokens, 10);
  if (n == 0) return;

  tokens[0].toLowerCase();

  if (tokens[0] == "help" || tokens[0] == "?") {
    printHelp();
  } else if (tokens[0] == "scan") {
    cmdScan();
  } else if (tokens[0] == "time") {
    cmdTime();
  } else if (tokens[0] == "temp") {
    cmdTemp();
  } else if (tokens[0] == "convtemp") {
    cmdConvTemp();
  } else if (tokens[0] == "dump") {
    cmdDump();
  } else if (tokens[0] == "status") {
    cmdStatus();
  } else if (tokens[0] == "ctrl") {
    cmdCtrl();
  } else if (tokens[0] == "clearflags") {
    cmdClearFlags();
  } else if (tokens[0] == "sqw") {
    cmdSqw(tokens, n);
  } else if (tokens[0] == "32k") {
    cmd32k(tokens, n);
  } else if (tokens[0] == "bbsqw") {
    cmdBbsqw(tokens, n);
  } else if (tokens[0] == "aging") {
    cmdAging(tokens, n);
  } else if (tokens[0] == "alarm") {
    cmdAlarm(tokens, n);
  } else if (tokens[0] == "rtcmp") {
    cmdRtCmp(tokens, n);
  } else if (tokens[0] == "rtcsub") {
    cmdRtcSub(tokens, n);
  } else if (tokens[0] == "rtcadd") {
    cmdRtcAdd(tokens, n);
  } else if (tokens[0] == "eeprobe") {
    cmdEeProbe();
  } else if (tokens[0] == "eeget") {
    cmdEeGet(tokens, n);
  } else if (tokens[0] == "eeput") {
    cmdEePut(tokens, n);
  } else if (tokens[0] == "set") {
    cmdSet(tokens, n);
  } else if (tokens[0] == "r" || tokens[0] == "read") {
    cmdRead(tokens, n);
  } else if (tokens[0] == "w" || tokens[0] == "write") {
    cmdWrite(tokens, n);
  } else {
    Serial.println(F("Unknown command. Type help."));
  }
}

void setup() {
  Wire.begin();
  Serial.begin(115200);

  delay(500);
  Serial.println();
  Serial.println(F("Nano I2C/DS3231 Interface"));
  Serial.println(F("Type help."));
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      handleLine(line);
      line = "";
    } else if (c >= 32 && c <= 126) {
      line += c;
      if (line.length() > 96) {
        Serial.println(F("ERROR: line too long"));
        line = "";
      }
    }
  }
}
