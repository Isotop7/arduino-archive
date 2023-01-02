#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_I2C_BUS_ADDRESS 0x3C
#define OLED_WIDTH 128
#define OLED_HEIGHT 32
#define OLED_RESET -1

Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Serial.begin(9600);
  Serial.println("Setup Serial");
  Wire.begin();
  Serial.println("Setup Wire");

  if(!oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_BUS_ADDRESS)) {
      Serial.println("SSD1306 allocation failed");
      for(;;);
    }
  oled.invertDisplay(true);
  delay(1000);
  oled.invertDisplay(false);
  delay(2000);
  Serial.println("Clear display");
  oled.clearDisplay();
 
  Serial.println("Write Text");
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.setCursor(0, 0);
  oled.println("wemos bootup complete"); //Textzeile ausgeben
  oled.display();
  delay(3000);
  oled.clearDisplay();
  oled.display();
}

void loop() {
  oled.dim(true);

  Serial.println("Write Text");
  oled.setTextSize(1);
  oled.setCursor(6, 0);
  oled.println("Hallo!"); //Textzeile ausgeben
  oled.display();
  delay(400);
  oled.clearDisplay();
  oled.display();
  delay(400);
  
}