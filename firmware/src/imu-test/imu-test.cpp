// QMI8658 IMU smoke test — matches Waveshare ESP32-S3 1.3" LCD demo wiring:
// Technical Files/.../Arduino/Factory code/ESP32S3-1.3inch/WS_QMI8658.cpp
#include <Arduino.h>
#include <Wire.h>

#include "SensorQMI8658.hpp"

static constexpr int kI2cSda = 47;
static constexpr int kI2cScl = 48;

static SensorQMI8658 imu;
static IMUdata accel;
static IMUdata gyro;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("IMU test (QMI8658, SDA=47 SCL=48)");

  Wire.begin(kI2cSda, kI2cScl);
  if (!imu.begin(Wire, QMI8658_L_SLAVE_ADDRESS, kI2cSda, kI2cScl)) {
    Serial.println("Failed to find QMI8658 — check I2C wiring / power.");
    while (true) {
      delay(1000);
    }
  }

  Serial.printf("Chip ID: 0x%02X\n", imu.getChipID());

  // Vendor demo passes a 4th self-test bool; SensorLib 0.4.x uses 3 args only.
  imu.configAccelerometer(SensorQMI8658::ACC_RANGE_4G,
                          SensorQMI8658::ACC_ODR_1000Hz,
                          SensorQMI8658::LPF_MODE_0);
  imu.configGyroscope(SensorQMI8658::GYR_RANGE_64DPS,
                      SensorQMI8658::GYR_ODR_896_8Hz,
                      SensorQMI8658::LPF_MODE_3);

  imu.enableGyroscope();
  imu.enableAccelerometer();

  imu.dumpCtrlRegister();
  Serial.println("Reading accel / gyro / temperature (throttled to ~4 Hz)…");
}

void loop() {
  static uint32_t lastPrintMs = 0;
  constexpr uint32_t kPrintIntervalMs = 250;

  if (imu.getDataReady()) {
    imu.getAccelerometer(accel.x, accel.y, accel.z);
    imu.getGyroscope(gyro.x, gyro.y, gyro.z);
  }

  const uint32_t now = millis();
  if (now - lastPrintMs < kPrintIntervalMs) {
    delay(1);
    return;
  }
  lastPrintMs = now;

  Serial.printf(
      "A g: %6.2f %6.2f %6.2f | G dps: %6.1f %6.1f %6.1f | %5.1f C\n",
      accel.x, accel.y, accel.z, gyro.x, gyro.y, gyro.z,
      imu.getTemperature_C());
}
