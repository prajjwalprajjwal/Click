#pragma once

#include <Adafruit_SSD1306.h>

// -------------------------------------------------------------
// Hardware I2C Pin Definitions for ESP32
// SDA: GPIO 21
// SCL: GPIO 22 (NEVER use GPIO 36/VP - it is INPUT ONLY and cannot drive SCL)
// -------------------------------------------------------------
#ifndef I2C_SDA
#define I2C_SDA 21
#endif

#ifndef I2C_SCL
#define I2C_SCL 22
#endif

#ifndef OLED_SDA_PIN
#define OLED_SDA_PIN I2C_SDA
#endif

#ifndef OLED_SCL_PIN
#define OLED_SCL_PIN I2C_SCL
#endif

// If display has a dedicated Hardware Reset / Enable pin connected to GPIO:
#ifndef OLED_RST_PIN
#define OLED_RST_PIN -1
#endif

#ifndef OLED_EN_PIN
#define OLED_EN_PIN -1
#endif

extern Adafruit_SSD1306 display;


