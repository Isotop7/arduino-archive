#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_ADS1015.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <PCF8574.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

/*
Define statements for sensors
*/
#define FAN_PIN D5
#define TEMP_PIN D6
#define I2C_PCF8574AN 0x38
#define I2C_ADS1115 0x48
#define FAN_PWM_FREQ 25000
#define FS_FORMAT false
#define FAN_OFF_STATE 0
#define FAN_ANALOG_MIN_VALUE 50
#define FAN_ANALOG_MAX_VALUE 1024
#define FAN_ANALOG_DEFAULT_VALUE 256
#define FAN_TOPIC "sbcrack/fan/speed"
#define FAN_MAX_SPEED 3000
#define FAN_MIN_SPEED 550
#define FAN_READ_DELAY 15000
#define FAN_ADJUST_DELAY 60000
#define FAN_TARGET_TEMPERATURE 25
#define FAN_MAX_TEMPERATURE 60
#define TEMP_TOPIC "sbcrack/sbc/X/temp"
#define TEMP_READ_DELAY 5000
#define POWER_GAIN_VALUE 0.015625
#define POWER_READ_DELAY 10000
#define POWER_CURRENT_MULTIPLIER 10
#define POWER_REF_VOLTAGE 5
#define POWER_TOPIC_CURRENT "sbcrack/sbc/X/current"
#define POWER_TOPIC_CONSUMPTION "sbcrack/sbc/X/power"
#define SBC_COUNT 1
#define SBC_TOPIC_GET "sbcrack/sbc/X/get"
#define SBC_TOPIC_SET "sbcrack/sbc/X/set"

/*
Define statements for network
*/
#define SSID ""
#define PASSWORD ""
#define MQTT_SERVER ""
#define MQTT_USER ""
#define MQTT_PASSWORD ""
#define MQTT_RECONNECT_DELAY 5000

/*
Create needed sensor objects
*/
PCF8574 pcf(I2C_PCF8574AN);
Adafruit_ADS1115 ads(I2C_ADS1115);
OneWire tempBus (TEMP_PIN);
DallasTemperature tempSensors(&tempBus);
DeviceAddress _tempDeviceAddress;

/*
Create needed network objects
*/
WiFiClient espClient;
PubSubClient mqttClient(espClient);

/*
Global hepler variables
*/
int tempSensorCount = 0;
char sensorAdresses[4][17] = {"05893A30308003E", "1029A2A203080057", "107FD7A40308007D", "107FD7A40308007D"};
long lastTempRead = 0;
long lastPowerRead = 0;
long lastFanRead = 0;
long lastFanAdjust = 0;
int fanPWMValue = FAN_ANALOG_MIN_VALUE;
const size_t jsonBufferSize = JSON_OBJECT_SIZE(2) + 100;
const char* configFilename = "/sbc_config.txt";
boolean readOnlyFilesystem = false;

/*
Config struct
*/
struct Config {
  int sbc1;
  int sbc2;
  int sbc3;
  int sbc4;
};
Config config;

//TODO: Check if needed
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++){
    if (deviceAddress[i] < 16) Serial.print("0");
      Serial.print(deviceAddress[i], HEX);
  }
}

/*
Save config to filesystem and apply
*/
void saveConfigSettings() {
  StaticJsonDocument<128> tempConfig;
  // Check if filesystem was mounted in read only mode
  if (readOnlyFilesystem) {
    return;
  }
  tempConfig["sbc1"] = config.sbc1;
  tempConfig["sbc2"] = config.sbc2;
  tempConfig["sbc3"] = config.sbc3;
  tempConfig["sbc4"] = config.sbc4;
  // Save new config to file
  File w = LittleFS.open(configFilename, "w");
  serializeJson(tempConfig, w);
  w.flush();
  w.close();
}

/*
Sets the state of an connected SBC
var isbc = Index of connected SBC
var istate = New state of SBC; 0 = off, 1 = on
*/
void setSBCState(int isbc, int istate) {
  //char arrays for converted parameters
  char state[2];
  char sbc[1];
  //Convert int to char array
  sprintf(state, "%d", istate);
  sprintf(sbc, "%d", (isbc) );
  char topic[] = SBC_TOPIC_GET;
  //Replace X value in topic with SBC
  topic[12] = sbc[0];

  // Writing new SBC state to pcf
  pcf.write( (isbc-1) , istate);
  // Serial output
  Serial.print("+ MQTT->PUB | ");
  Serial.print(topic);
  Serial.print(" | ");
  Serial.println(state);
  // Push state to mqtt broker
  mqttClient.publish(topic, state);
  if (isbc == 1) { config.sbc1 = 1;}
  if (isbc == 2) { config.sbc2 = 1;}
  if (isbc == 3) { config.sbc3 = 1;}
  if (isbc == 4) { config.sbc4 = 1;}
  saveConfigSettings();
}

/*
Gets the state of an connected SBC
var isbc = Index of connected SBC
*/
void getSBCState(int isbc) {
  //char arrays for converted parameters
  char state[2];
  char sbc[1];
    //Read state from pcf
  int istate = pcf.read(isbc-1);
  //Convert int to char array
  sprintf(state, "%d", istate);
  sprintf(sbc, "%d", isbc);
  char topic[] = SBC_TOPIC_GET;
  //Replace X value in topic with SBC
  topic[12] = sbc[0];

  // Serial output
  Serial.print("+ MQTT->PUB | ");
  Serial.print(topic);
  Serial.print(" | ");
  Serial.println(state);
  // Push state to mqtt broker
  mqttClient.publish(topic, state);
}

/*
Read  config from filesystem and apply
*/
void readConfigSettings() {
  StaticJsonDocument<128> tempConfig;
  // Read config and deserialize
  File r = LittleFS.open(configFilename, "r");
  DeserializationError err = deserializeJson(tempConfig, r);
  r.close();
  Serial.println("+ FS | Read settings:");
  // Set sbc states according to config
  if (!err) {
    serializeJsonPretty(tempConfig, Serial);
    Serial.println();
    config.sbc1 = tempConfig["sbc1"] | 0;
    config.sbc2 = tempConfig["sbc2"] | 0;
    config.sbc3 = tempConfig["sbc3"] | 0;
    config.sbc4 = tempConfig["sbc4"] | 0;
    setSBCState(1, config.sbc1);
    setSBCState(2, config.sbc2);
    setSBCState(3, config.sbc3);
    setSBCState(4, config.sbc4);
  } else {
    Serial.println("+ FS | Cant read settings.");
  }
}

/*
Outputs the given error to Serial
var char* topic = Pointer to char array containing topic
var const char* exception = Pointer to char array containing additional error message
*/
void throwMqttError(char* topic, const char* exception) {
  Serial.print("+ MQTT | Can't match incoming payload topic ");
  Serial.print(topic);
  Serial.print("; rc=");
  Serial.println(exception);
}

/*
Connected callback method of mqttClient, gets called on incoming message
var char* topic = Pointer to char array containing topic
var byte* payload = Pointer to byte array containing the payload of the incoming message
var u_int length = length of incoming message
*/
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  //Int variables for sbc and messagePayload
  int sbc;
  int messagePayload = (char)payload[0] - '0';
  Serial.print("+ MQTT->CALLBACK | ");
  Serial.print(topic);
  Serial.print(" | ");
  Serial.println(messagePayload);
  //Char variable for tokenizing string
  char * token;
  //Split topic on next /
  token = strtok(topic, "/");
  //If token is "sbcrack"
  if (strcmp(token, "sbcrack") == 0) {
    //Split topic on next /
    token = strtok(NULL, "/");
    //If token is "sbc"
    if (strcmp(token, "sbc") == 0) {
      //Split topic on next /
      token = strtok(NULL, "/");
      //Convert splitted token to index for SBC
      sbc = atoi(token);
      //Check if topic contains valid SBC index
      if (sbc <= SBC_COUNT) {
        //Split topic on next /
        token = strtok(NULL, "/");
        //If token is "set"
        if (strcmp(token, "set") == 0) {
          //Check for valid new state of given SBC
          if (messagePayload == 0 || messagePayload == 1) {
            //Call method to set SBC
            setSBCState(sbc, messagePayload);
          } else {
            //Throw error on invalid given state
            throwMqttError(topic, "INVALID_SBC_STATE");
          }
        //If token is "get"
        } else if (strcmp(token, "get") == 0) {
          //Call method to get current SBC state
          getSBCState(sbc);
        } else {
          //Throw error for invalid SBC command
          throwMqttError(topic, "INVALID_SBC_COMMAND");
        }
      } else {
        //Throw error for invalid SBC
        throwMqttError(topic, "INVALID_SBC");
      }
    } else {
      //Throw error for invalid topic
      throwMqttError(topic, "INVALID_TOPIC_SUB");
    }
  } else {
    //Throw error for invalid topic
    throwMqttError(topic, "INVALID_TOPIC_START");
  }
}

/*
Called method when wemos is not connected to mqtt broker
*/
void reconnect() {
  while (!mqttClient.connected()) {
    Serial.println("+ MQTT | Attempting connection ...");
    //Try connection to broker and check for returned value; true means connected
    if (mqttClient.connect("sbcrack-wemos", MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("+ MQTT | Connected to broker");
      //Loop through connected sbcs and subscribe
      for (int i = 0; i < SBC_COUNT; i++) {
        //char array for converted parameters
        char sbc[1];
        //Convert int to char array
        sprintf(sbc, "%d", (i+1));
        char topic[] = SBC_TOPIC_SET;
        //Replace X value in topic with SBC
        topic[12] = sbc[0];

        getSBCState(i+1);

        // Serial output
        Serial.print("+ MQTT->SUB | ");
        Serial.println(topic);
        //Subscribe to mqtt topic
        mqttClient.subscribe(topic);
      }
    } else {
      //Output error to Serial
      Serial.print("+ MQTT | Connection attempt failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" ; try again in 5 seconds");
      //Wait for given timeout
      delay(MQTT_RECONNECT_DELAY);
    }
  }
}

/*
Reads temperatures from temperature bus and pushes them to mqtt broker
var int isbc = index of SBC and therefore the corresponding temperature sensor
*/
void readTemperature(int isbc) {
  //Check if any temperature sensors are connected
  if (tempSensorCount == 0) {
    Serial.println("+ TEMP | No temperature sensors connected.");
  } else {
    //TODO: Check for valid sensors
    // Send the command to the bus to get temperatures
    tempSensors.requestTemperatures();
    
    // Search the wire for address on given index
    if(tempSensors.getAddress(_tempDeviceAddress, isbc)){
      //Char arrays for topic and SBC
      char topic[] = TEMP_TOPIC;
      char sbc[1];
      //Convert int value of SBC to Char
      sprintf(sbc, "%d", isbc+1);
      //Replace X value in topic with SBC
      topic[12] = sbc[0];
      //Get read temperature from sensor
      float tempC = tempSensors.getTempC(_tempDeviceAddress);
      //Output to Serial
      Serial.print("+ MQTT->PUB | ");
      Serial.print(topic);
      Serial.print(" | ");
      Serial.println(tempC);
      //Push temperature to mqtt broker
      mqttClient.publish(topic, String(tempC).c_str(), true);
    }
  }
}

/*
Reads temperatures from temperature bus and returns mean
*/
float getMeanTemperature() {
  float meanTemperature = 0;
  //Check if any temperature sensors are connected
  if (tempSensorCount == 0) {
    Serial.println("+ TEMP | No temperature sensors connected.");
  } else {
    // Send the command to the bus to get temperatures
    tempSensors.requestTemperatures();
    
    for (int i = 0; i < tempSensorCount; i++) {
      //Search the wire for address on given index
      if(tempSensors.getAddress(_tempDeviceAddress, i)){
        //Add temperature of sensor to variable
        meanTemperature += tempSensors.getTempC(_tempDeviceAddress);
      }
    }
    //Get mean temperature by dividing to temperature sensors count
    meanTemperature = meanTemperature / tempSensorCount;
  }
  //Return mean Temperature
  return meanTemperature;
}

/*
Read, convert and push fan speed to mqtt broker
*/
void readFanSpeed() {
  //Get mapped fan speed
  int fanSpeed = map(fanPWMValue, FAN_ANALOG_MIN_VALUE, FAN_ANALOG_MAX_VALUE, FAN_MIN_SPEED, FAN_MAX_SPEED);
  int fanLoad = map(fanPWMValue, FAN_ANALOG_MIN_VALUE, FAN_ANALOG_MAX_VALUE, 0, 100);
  //Output fan speed
  Serial.print("+ FAN | Speed is ");
  Serial.println(fanSpeed);

  Serial.print("+ MQTT->PUB | ");
  Serial.print(FAN_TOPIC);
  Serial.print(" | ");
  Serial.println(fanLoad);
  //Push fan speed to mqtt broker
  mqttClient.publish(FAN_TOPIC, String(fanLoad).c_str());
}

/*
Adjusts fan speed according to thermal conditions
*/
void adjustFanSpeed() {
  //Get mean temperature and store to variable
  float meanTemperature = getMeanTemperature();
  //Calculate temperature offset
  float tempOffset = meanTemperature - FAN_TARGET_TEMPERATURE;

  //Check if mean temperature could be calculated
  if(meanTemperature > 0) {
    //Check if target temperature is higher than mean temperature; if this is the case we don't need to use the fan and could turn it off
    if (tempOffset < 0) {
      //Set fan to minimum value
      Serial.print("+ FAN | Temperature is okay. Turning fan off.");
      Serial.println(FAN_OFF_STATE);
      fanPWMValue = FAN_OFF_STATE;
    } else {
      //Map meanTemperature between bounds to analog fan pwm value and set it to variable
      fanPWMValue = map(meanTemperature, FAN_TARGET_TEMPERATURE, FAN_MAX_TEMPERATURE, FAN_ANALOG_MIN_VALUE, FAN_ANALOG_MAX_VALUE);
    }
  } else {
    //If no mean temperature could be calculated, we set the fan to a medium default speed
    Serial.print("+ FAN | Error calculating mean temperature. Setting to default value ");
    Serial.println(FAN_ANALOG_DEFAULT_VALUE);
    fanPWMValue = FAN_ANALOG_DEFAULT_VALUE;
  }

  //Set fan to new speed
  analogWrite(FAN_PIN, fanPWMValue);
  //Output to Serial
  Serial.print("+ FAN | Set speed to ");
  Serial.println(fanPWMValue);
}

/*
Returns shunt voltage of SBC
var int isbc = Index of SBC
*/
float readShuntVoltage(int isbc) {
    //initialize variables
  int16_t adc0;
  float shuntVoltage;

  //read from ads on index of SBC
  adc0 = ads.readADC_SingleEnded(isbc);
  //multiply read value with power gain value to drawn voltage of shunt in mV
  shuntVoltage = (adc0 * POWER_GAIN_VALUE);

  //Check for negative shunt voltage which indicates no load
  if(shuntVoltage < 0) {
    return 0;
  } else {
    return shuntVoltage;
  }
}

/*
Returns measured current of SBC in unit mA
var int isbc = Index of SBC
*/
void readSBCCurrent(int isbc) {
  //Char arrays for topic and SBC
  char topic[] = POWER_TOPIC_CURRENT;
  char sbc[1];
  //Convert int value of SBC to Char
  sprintf(sbc, "%d", isbc+1);
  //Replace X value in topic with SBC
  topic[12] = sbc[0];

  //Calculate current based on shunt Voltage; mA
  float current = readShuntVoltage(isbc) * POWER_CURRENT_MULTIPLIER;

  //Output to Serial
  Serial.print("+ MQTT->PUB | ");
  Serial.print(topic);
  Serial.print(" | ");
  Serial.println(current);
  //Push drawn current to mqtt broker
  mqttClient.publish(topic, String(current).c_str());
}

/*
Returns measured power consumption of SBC in unit mA
var int isbc = Index of SBC
*/
void readSBCPowerConsumption(int isbc) {
  //Char arrays for topic and SBC
  char topic[] = POWER_TOPIC_CONSUMPTION;
  char sbc[1];
  //Convert int value of SBC to Char
  sprintf(sbc, "%d", isbc+1);
  //Replace X value in topic with SBC
  topic[12] = sbc[0];

  //Calculate power consumption based on shunt Voltage; W
  float power = readShuntVoltage(isbc) * POWER_CURRENT_MULTIPLIER * POWER_REF_VOLTAGE / 1000;

  //Output to Serial
  Serial.print("+ MQTT->PUB | ");
  Serial.print(topic);
  Serial.print(" | ");
  Serial.println(power);
  //Push power consumption to mqtt broker
  mqttClient.publish(topic, String(power).c_str());
}

/*
Arduino setup routine
*/
void setup() {
  //  +++ SERIAL CONSOLE +++
  //Setup serial monitor
  Serial.begin(9600);

  // +++ FILESYSTEM +++
  //Format filesystem if format mode is enabled
  if(FS_FORMAT) { 
    LittleFS.format(); 
    Serial.println("+ FS | Formatting file system done");
  }
  //Initialize File System
  if(!LittleFS.begin()) {
    Serial.println("+ FS | Error mounting filesystem! Settings won't be saved");
    readOnlyFilesystem = true;
  } else {
    Serial.println("+ FS | Filesystem mounted");
  }

  //  +++ NETWORK +++
  //Connect to wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  Serial.print("+ WIFI | Connecting ");

  //check if connection to wifi was established
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println(" --> connected!");
  Serial.print("+ WIFI | Got IP: ");  
  Serial.println(WiFi.localIP());

  //Set httpClient settings
  //httpClient.begin(espClient, LOG_URL);
  //httpClient.addHeader("ContentType", "text/json");

  //Set mqttClient settings
  mqttClient.setServer(MQTT_SERVER, 1883);
  mqttClient.setCallback(mqttCallback);

  //  +++ FAN +++
  //Setup fan pin
  pinMode(FAN_PIN, OUTPUT);
  //Setup fan with correct pwm frequency
  analogWriteFreq(FAN_PWM_FREQ);
  Serial.println("+ FAN | Setting pwm frequency");
  //Setting fan to minimal speed
  analogWrite(FAN_PIN, FAN_OFF_STATE);

  //  +++ POWER CONSUMPTION +++
  //Setup ads and set gain
  ads.begin();
  ads.setGain(GAIN_EIGHT);

  //  +++ TEMPERATURE SENSORS +++
  //Setup temperature sensors
  tempSensors.begin();
  Serial.println("+ TEMP | Counting devices on bus...");
  //Counting sensors on bus
  tempSensorCount = tempSensors.getDeviceCount();
  Serial.print("+ TEMP | Found ");
  Serial.print(tempSensorCount, DEC);
  Serial.println(" temperature sensors on the bus");

  // Loop through each device, print out address
  for(int i=0;i<tempSensorCount; i++){
    // Search the wire for address
    if(tempSensors.getAddress(_tempDeviceAddress, i)){
      Serial.print("+ TEMP | Found device ");
      Serial.print(i, DEC);
      Serial.print(" with address: ");
      printAddress(_tempDeviceAddress);
      Serial.println();
    }
  }

  //  +++ SBC +++
  //Check for connected pcf
  if (pcf.isConnected()) {
    // Setup pcf with initial value of zero (all sbcs off)
    pcf.begin(0x00);
    // Read settings from config file and apply
    readConfigSettings();
  } else {
    Serial.println("+ STATE | Can't connect to PCF and therefore can't switch sbcs");
  }
}

/*
Arduino loop routine
*/
void loop() {
  //Check if wemos is not connected to mqtt broker
  if (!mqttClient.connected()) {
    //Reconnect to mqtt broker
    reconnect();
  }
  //mqtt loop; needed to check for incoming messages
  mqttClient.loop();

  //Set timestamp 'now'
  long now = millis();
  //Check if delay for last temperature read is reached
  if ((now - lastTempRead) > TEMP_READ_DELAY) {
    //Loop through all connected temperature sensors and read temperature
    for (int i = 0; i < tempSensorCount; i++) {
      readTemperature(i);
    }
    //Set timestamp of last execution to now
    lastTempRead = now;
  }

  //Set timestamp 'now'
  now = millis();
  //Check if delay for last power read is reached
  if ((now - lastPowerRead) > POWER_READ_DELAY) {
    //Loop through all connected SBCs and read current and power consumption
    for (int i = 0; i < SBC_COUNT; i++) {
      readSBCPowerConsumption(i);
      readSBCCurrent(i);
    }
    //Set timestamp of last execution to now
    lastPowerRead = now;
  }

  //Set timestamp 'now'
  now = millis();
  //Check if delay for last fan speed read is reached
  if ((now - lastFanRead) > FAN_READ_DELAY) {
    //Read fan speed
    readFanSpeed();
    //Set timestamp of last execution to now
    lastFanRead = now;
  }

  //Set timestamp 'now'
  now = millis();
  //Check if delay for last fan speed read is reached
  if ((now - lastFanAdjust) > FAN_ADJUST_DELAY) {
    //Adjust fan speed
    adjustFanSpeed();
    //Set timestamp of last execution to now
    lastFanAdjust = now;
  }
}