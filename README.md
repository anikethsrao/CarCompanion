# Car Companion

A cute companion that lives on the car dashboard and reacts to the way you drive.

## Firmware

`firmware/CarCompanion/CarCompanion.ino` is the Arduino sketch; the face is drawn procedurally by `CarFace.h` next to it.

- Board: Seeed XIAO
- Display: SSD1306 128x64 OLED (I2C, address 0x3C)
- Sensor: GY-85 (only the ADXL345 accelerometer is used so far)
- Libraries: Adafruit SSD1306, Adafruit GFX, an ADXL345 library providing `getAcceleration(double*)` (Seeed Grove ADXL345)
