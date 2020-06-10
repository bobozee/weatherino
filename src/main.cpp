#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <TimeLib.h>

const char* ssid = "ErpixLAN";
const char* password = "***REMOVED***";

const uint8_t powerLED = D4;
const uint8_t connectionLED = D2;
const uint8_t waterLED = D1;
const uint8_t errorLED = D5;
const uint8_t bridge = D1;
const uint8_t signalR = D8;
const uint8_t signalG = D7;
const uint8_t signalB = D6;

int timecount = 0;
WiFiClient client;
HTTPClient http;
String responseData;
DynamicJsonDocument weatherDoc(15360);
JsonArray reports;
StaticJsonDocument<1024> filter;
int failcount = 0;

void setup() {
  Serial.begin(115200);
  pinMode(powerLED, OUTPUT);
  pinMode(connectionLED, OUTPUT);
  pinMode(waterLED, OUTPUT);
  pinMode(errorLED, OUTPUT);
  pinMode(bridge, OUTPUT);
  pinMode(signalR, OUTPUT);
  pinMode(signalG, OUTPUT);
  pinMode(signalB, OUTPUT);
  Serial.println("~~~~~~~~~~~~~~~~~");
  Serial.println("Startup complete.");
  Serial.println("~~~~~~~~~~~~~~~~~");
  digitalWrite(powerLED, HIGH);
}

void errorHandler(String error) {
  timecount = 0;
  WiFi.disconnect();
  digitalWrite(powerLED, LOW);
  digitalWrite(connectionLED, LOW);
  digitalWrite(waterLED, LOW);
  digitalWrite(bridge, LOW);
  Serial.println(error);
  Serial.println("An Error has occured. This Task will be terminated. Restart in 10 minutes; Press the 'Reset' button to restart the Program instantly.");
  digitalWrite(errorLED, HIGH);
  delay(1000 * 60 * 10);
  digitalWrite(errorLED, LOW);
  ESP.reset();
}

void rgbBlink (boolean red, boolean green, boolean blue, int duration) {
  if (red) {
    digitalWrite(signalR, HIGH);
  }
  if (green) {
    digitalWrite(signalG, HIGH);
  }
  if (blue) {
    digitalWrite(signalB, HIGH);
  }
  delay(duration);
  digitalWrite(signalR, LOW);
  digitalWrite(signalG, LOW);
  digitalWrite(signalB, LOW);
}

void loop() {
  Serial.print("Beginning to connect to network with SSID ");
  Serial.print(ssid);
  Serial.print(" and password ");
  Serial.println(password);
  Serial.print("Connecting....");
  WiFi.begin(ssid, password);
  int rate = 250; // rate of blinking for the connection led
  int threshold = 20; // amount of time to pass for timeout of connection attempt
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(connectionLED, HIGH);
    delay(rate);
    digitalWrite(connectionLED, LOW);
    delay(rate);
    counter++;
    if (counter == threshold) {
      errorHandler("Timeout");
    }
  }
  digitalWrite(connectionLED, HIGH);
  Serial.println("Connected!");
  Serial.println();
  
  int trycount = 0;
  while (true) {
    if (trycount == 3) {
      errorHandler("Failed to establish connection to website.");
    }
    Serial.print("Contacting Weather Website.... ");
    http.begin(client, "https://api.openweathermap.org/data/2.5/onecall?lat=51.603170&lon=6.917150&exclude=minutely,daily&appid=a02aeb3716cc0681332cb38fe5625bab");
    int httpCode = http.GET(); // fetch the GET code of the HTTP request
    if (httpCode >= 200 && httpCode <= 400) {
      responseData = http.getString(); // if the code is good, get the fetchable data
      Serial.println("Done!");
      Serial.println();
      return; // escape from the fail-repeating loop with requested information
    } else {
      Serial.print(httpCode);
      Serial.print(", ");
      Serial.println(http.errorToString(httpCode).c_str());
      Serial.println();
      digitalWrite(errorLED, HIGH);
      delay(500);
      digitalWrite(errorLED, LOW);
      trycount++;
    }
    http.end();
    delay(1000);
  }
  
  Serial.print("Processing Data....");
  filter["current"]["dt"] = true;
  filter["current"]["wind_speed"] = true;
  filter["current"]["weather"]["0"]["id"] = true;
  int forecastrange = 2; // amount of hours to be forecasted, max = 48, 0 means last hour
  for (int i = 0; i < forecastrange; i++) {
    filter["hourly"][i]["weather"]["id"] = true;
  }
  DeserializationError error = deserializeJson(weatherDoc, responseData, DeserializationOption::Filter(filter));
  if (error) {
    String errorMsg("Parsing Error: ");
    errorMsg.concat(error.c_str());
    errorHandler(errorMsg);
  }
  float wind = weatherDoc["wind"]["speed"];
  unsigned long unixtime = weatherDoc["dt"];
  int timeM = month(unixtime);
  int timeH = hour(unixtime) + 2;
  int uppermax = 0;
  switch (timeM) { // dynamically change shutdown hour depending on month
    case 4: uppermax = 19.5; break;
    case 5: uppermax = 20; break;
    case 6: uppermax = 20.5; break;
    case 7: uppermax = 21; break;
    case 8: uppermax = 21; break;
    case 9: uppermax = 20; break;
    case 10: uppermax = 19.5; break;
  }
  int weather = weatherDoc["current"]["weather"]["0"]["id"];
  int weatherids[forecastrange];
  for (int i = 1; i < 2; i++) { // INFO: the loop starts with 1 due to hour 0 being the latest hour, which is not important in a forecast
    weatherids[i - 1] = weatherDoc["hourly"][i]["weather"][0]["id"]; // save the hourly weather ids in an int array
  }
  boolean goodTime = false;
  boolean goodWeather = false;
  boolean goodWind = false;
  boolean goodEstimate = false;
  if (weather >= 800) {
    goodWeather = true;
  }
  if (timeM >= 4 && timeM <= 10 && timeH >= 8 && timeH < uppermax) {
    goodTime = true;
  }
  if (wind < 30) {
    goodWind = true;
  }
  goodEstimate = true;
  for (int id : weatherids) {
    if (id < 800) {
      goodEstimate = false;
    }
  }
  Serial.println("Done!");
  Serial.println();
  
  int signalduration = 1000; // amount of milliseconds for the signal led
  //red = time, blue = weather, green = wind, white = forecast
  if (goodTime) {
    Serial.println("Time is optimal.");
  } else {
    Serial.println("Time isn't optimal.");
    rgbBlink(true, false, false, signalduration);
  }
  if (goodWind) {
    Serial.println("Wind is optimal.");
  } else {
    Serial.println("Wind isn't optimal.");
    rgbBlink(true, true, false, signalduration);
  }
  if (goodWeather) {
    Serial.println("Weather is optimal.");
  } else {
    Serial.println("Weather isn't optimal.");
    rgbBlink(false, false, true, signalduration);
  }
  if (goodEstimate) {
    Serial.println("Forecast is optimal.");
  } else {
    Serial.println("Forecast isn't optimal.");
    rgbBlink(true, true, true, signalduration);
  }
  if (goodTime && goodWeather && goodEstimate && goodWind) {
    Serial.println("Outcome: Environment fits requirements. Pump is on.");
    digitalWrite(waterLED, HIGH);
    digitalWrite(bridge, HIGH);
  } else {
    Serial.println("Outcome: Environment does not fit requirements. Pump is off.");
    digitalWrite(waterLED, LOW);
    digitalWrite(bridge, LOW);
  }

  Serial.println();
  WiFi.disconnect();
  digitalWrite(connectionLED, LOW);
  unsigned long timeoutTime = (60 - minute(unixtime)) * 60000;
  Serial.println("All done! Disconnecting and sleeping until the next hour. 'Till then!");
  delay(timeoutTime + 10000);
}