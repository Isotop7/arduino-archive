#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

#define SD_CHIP_SELECT 4
#define PUMP1 8
#define PUMP2 9
#define PUMP3 10
#define PUMP4 11

const char * waterPumpLogFileName = "HYDRO.TXT";
boolean logToSD = false;
File root;

void pushMessage(String unit, String entity, String message)
{
  //TODO: Add timestamp
  String output = String(millis()) + " : " + unit + "\t=> " + entity + "\t-> " + message;
  Serial.println(output);
  if (logToSD) {
    File dataFile = SD.open(waterPumpLogFileName, FILE_WRITE);
    if (dataFile) {
      dataFile.println(output);
      dataFile.close();
    }
    else {
      logToSD = false;
      String errorMessage = "error writing to file " + String(waterPumpLogFileName);
      pushMessage("arduino", "SD", errorMessage);
    }
  }
}

void printSDHeader() {
  //TODO: Add timestamp
  String output = "\n##################################################";
  File dataFile = SD.open(waterPumpLogFileName, FILE_WRITE);
    if (dataFile) {
      dataFile.println(output);
      dataFile.close();
    }
}

void switchPump(int pump, int duration)
{
  String message = "on for " + String(duration) + " milliseconds";
  pushMessage("relais", "Pump 1", message);
  digitalWrite(pump, 0);
  delay(duration);
  digitalWrite(pump, 1);
}

void setup()
{
  Serial.begin(9600);
  while (!Serial)
  {
    ;
  }
  
  pushMessage("arduino", "SD", "starting SD init");
  if (!SD.begin(SD_CHIP_SELECT)) {
    logToSD = false;
    pushMessage("arduino", "SD", "initialization failed");
  }
  else {
    logToSD = true;
    if(SD.exists( (char*)waterPumpLogFileName) ) {
      printSDHeader();
    }
    pushMessage("arduino", "SD", "initialization done");
    pushMessage("arduino", "env", "logToSD = true");
  }

  pinMode(PUMP1, OUTPUT);
  pinMode(PUMP2, OUTPUT);
  pinMode(PUMP3, OUTPUT);
  pinMode(PUMP4, OUTPUT);

  digitalWrite(PUMP1, 1);
  digitalWrite(PUMP2, 1);
  digitalWrite(PUMP3, 1);
  digitalWrite(PUMP4, 1);

  String bootupMessage = "bootup took " + String(millis()) + " milliseconds";
  pushMessage("arduino", "env", bootupMessage);
}

void loop()
{
  switchPump(PUMP1, 500);
  delay(10000);
}