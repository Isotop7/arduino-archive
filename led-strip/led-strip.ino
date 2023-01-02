#include <ESP8266WiFi.h>
#include <aREST.h>
#include <FS.h>
#include <FastLED.h>

#define LISTEN_PORT 80
#define RED_LED D5     //GPIO14, D5
#define BLUE_LED D1    //GPIO5, D1
#define GREEN_LED D2   //GPIO4, D2

aREST rest = aREST();
const char* ssid = "";
const char* password = "";
const char* filename = "/boot_config.txt";
WiFiServer server(LISTEN_PORT);

int setLEDStripState(String command);
int setLEDStripBrightness(String command);
int setLEDStripRGB(String command);
boolean readOnlyMode = false;

void setup() {  
  Serial.begin(115200);
  Serial.println();

  /*rest.set_id("001");
  rest.set_name("NodeMCU-LEDStrip");
  rest.function("setLEDState", setLEDStripState);
  rest.function("setLEDBrightness", setLEDStripBrightness);
  rest.function("setLEDRGB", setLEDStripRGB);

  WiFi.begin(ssid, password);
  Serial.print("+ Connecting to wifi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("+ Connected, IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();

  //Initialize File System
  if(!SPIFFS.begin()) {
    Serial.println("+ Error while mounting SPIFFS");
    readOnlyMode = true;
  } else {
    Serial.println("+ FS mounted. Settings will be saved");
  }

  File r = SPIFFS.open(filename, "r");
  String bootConfig = r.readString();
  Serial.print("+ boot_config: ");
  Serial.println(bootConfig);
  r.close();
  applySettings(bootConfig);

  Serial.println("+ Initialization complete. Waiting for incoming connections...");*/
}

void loop() {
  /*WiFiClient client = server.available();
  if (!client) {
    return;
  }
  while(!client.available()){
    delay(1);
  }
  rest.handle(client);*/
  static uint8_t hue;
  hue = hue + 1;
  showAnalogRGB(CHSV(hue,255,255));
  delay(1000);
}

void showAnalogRGB(const CRGB& rgb) {
  Serial.print("Red: " + rgb.r);
  Serial.print("Green: " + rgb.g);
  Serial.print("Blue: " + rgb.b);
  Serial.println();
  analogWrite(RED_LED, rgb.r);
  analogWrite(GREEN_LED, rgb.g);
  analogWrite(BLUE_LED, rgb.b);
}

int setLEDStripState(String command) {
  if (command == "on" ||  command.toInt() == 1) {
    pinMode(GREEN_LED, OUTPUT);
    pinMode(RED_LED, OUTPUT);
    pinMode(BLUE_LED, OUTPUT);
    setLEDStripBrightness("255");
    Serial.println("+ Setting led strip state to " + command);
    saveSettings("on", "");
    return 1;
  } else if (command == "off" ||  command.toInt() == 0) {
    digitalWrite(GREEN_LED, 0);
    digitalWrite(RED_LED, 0);
    digitalWrite(BLUE_LED, 0);
    Serial.println("+ Setting led strip state to " + command);
    saveSettings("off", "");
    return 0;
  } else {
    return 2;
  }
}

int setLEDStripBrightness(String command) {
  int brightness = command.toInt();
  if(brightness > 0 && brightness < 256) {
    analogWrite(GREEN_LED, brightness);
    analogWrite(RED_LED, brightness);
    analogWrite(BLUE_LED, brightness);
    String sColors = colorsToString(brightness, brightness, brightness);
    Serial.println("+ Setting led strip colors to " + sColors);
    saveSettings("on", sColors);
    return 0;
  } else {
    return 1;
  }
}

int setLEDStripRGB(String command) {
  // get colors from command; format has to be '?params={red},{green},{blue}
  int cRed = command.substring(0, command.indexOf(",")).toInt();;
  int cGreen = command.substring(command.indexOf(",")+1, command.lastIndexOf(",")).toInt();
  int cBlue = command.substring(command.lastIndexOf(",")+1, command.length()).toInt();

  // check for values out of bounds
  if ( 
    (cRed < 0 || cRed > 255) ||
    (cGreen < 0 || cGreen > 255) ||
    (cBlue < 0 || cBlue > 255)
  ) { 
    return 1; 
  } else {
    analogWrite(RED_LED, cRed);
    analogWrite(GREEN_LED, cGreen);
    analogWrite(BLUE_LED, cBlue);
    String sColors = colorsToString(cRed, cGreen, cBlue);
    Serial.println("+ Setting led strip colors to " + sColors);
    saveSettings("on", sColors);
    return 0;
  }
}

void saveSettings(String led_state, String color) {
  if (readOnlyMode) {
    return;
  }
  File w = SPIFFS.open(filename, "w");
  w.print("led=" + led_state);
  w.print(";");
  w.print("color=" + color);
  w.flush();
  w.close();
}

void applySettings(String settings) {
  String state = "";
  String color = "";
  int nextIndex = 0;
  int lastIndex = 0;
  String nextSetting = "";
  String key = "";
  
  while(nextIndex != -1) {
    nextIndex = settings.indexOf(";", lastIndex);
    nextSetting = settings.substring(lastIndex, nextIndex);
    lastIndex = nextIndex + 1;
    key=nextSetting.substring(0, nextSetting.indexOf("="));
    if(key == "led") {
      state = nextSetting.substring(nextSetting.indexOf("=") + 1);
      Serial.println("+ Found setting 'led' in boot_config with value '" + state + "'");
    } else if(key == "color") {
      color = nextSetting.substring(nextSetting.indexOf("=") + 1);
      Serial.println("+ Found setting 'color' in boot_config with value '" + color + "'");
    }
  }

  // apply saved settings
  if(color == "") {
    setLEDStripState(state);
  } else {
    setLEDStripRGB(color);
  }
}

String colorsToString(int red, int green, int blue) {
  String color = (String)red + "," + (String)green + "," + (String)blue;
  return color;
}
