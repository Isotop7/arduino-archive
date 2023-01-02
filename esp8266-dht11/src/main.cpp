#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"

#define DHT_PIN D6
#define DHTTYPE DHT11
#define ssid ""
#define password ""
#define mqtt_server ""
#define mqtt_user ""
#define mqtt_password ""
#define humidity_topic "/humidity"
#define temperature_topic "/temperature"

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHT_PIN, DHTTYPE);

long lastMsg = 0;

void setup() {
  Serial.begin(9600);

  pinMode(DHT_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(2, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(2, HIGH);

  dht.begin();

  Serial.println("Connecting to ");
  Serial.println(ssid);

  //connect to your local wi-fi network
  WiFi.begin(ssid, password);

  //check wi-fi is connected to wi-fi network
  while (WiFi.status() != WL_CONNECTED) {
  delay(1000);
  Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected..!");
  Serial.print("Got IP: ");  Serial.println(WiFi.localIP());

  client.setServer(mqtt_server, 1883);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP8266Client", mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;

    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    client.publish(temperature_topic, String(temp).c_str(), true);
    Serial.print("Published temperature: ");
    Serial.println(temp);
    Serial.print("Published humidity: ");
    Serial.println(hum);
    client.subscribe(temperature_topic);
    client.publish(humidity_topic, String(hum).c_str(), true);
    client.subscribe(humidity_topic);
  }
}