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
const int rate = 250;
const int threshold = 20;

int timecount = 0;
boolean goodTime = false;
boolean goodWeather = false;
boolean goodWind = false;
boolean goodEstimate = false;
WiFiClient client;
HTTPClient http;
String responseData;
DynamicJsonDocument forecastDoc(15360);
StaticJsonDocument<1024> weatherDoc;
JsonArray reports;
JsonArray weathers;
StaticJsonDocument<1024> filterforecast;
StaticJsonDocument<100> filterweather;
boolean escape = false;
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
  Serial.println("\n\n~~~~~~~~~~~~~~~~~");
  Serial.println("Startup complete.");
  Serial.println("~~~~~~~~~~~~~~~~~\n");
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

void download(String address, String name, int tryCount = 0) {

  if (tryCount == 3) {
    errorHandler("[HTTP] Max try count exceeded. I am giving up!");
    return;
  }

  Serial.print("Contacting ");
  Serial.print(name);
  Serial.print("....");

  if( http.begin(client, address) ) {
    int httpCode = http.GET();
    String payloadData;
    if (httpCode >= 200 && httpCode <= 400) {
      payloadData = http.getString();
      Serial.println("Done!");
    } else {
      Serial.printf("[HTTP] GET... failed, error: %s", http.errorToString(httpCode).c_str());
      digitalWrite(errorLED, HIGH);
      delay(500);
      digitalWrite(errorLED, LOW);
    }
    http.end();
    if (!payloadData.isNull()) {
      responseData = payloadData;
      return;
    }
    // wait one second and try again
    delay(1000);
    download(address, name, ++tryCount);
  } else {
    errorHandler("[HTTP] GET...Unable to connect");
  }
}

void loop() {
  Serial.print("Beginning to connect to network with SSID ");
  Serial.print(ssid);
  Serial.print(" and password ");
  Serial.println(password);
  Serial.print("Connecting....");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(connectionLED, HIGH);
    delay(rate);
    digitalWrite(connectionLED, LOW);
    delay(rate);
    if (WiFi.status() == WL_CONNECT_FAILED) {
      errorHandler("Connection Failed");
    }
  }
  digitalWrite(connectionLED, HIGH);
  Serial.println("Connected!\n");

  download("http://api.openweathermap.org/data/2.5/weather?lat=51.6&lon=6.92&appid=a02aeb3716cc0681332cb38fe5625bab", "Weather Website");

  Serial.print("Processing Data....");
  filterweather["dt"] = true;
  filterweather["weather"] = true;
  DeserializationError error = deserializeJson(weatherDoc, responseData, DeserializationOption::Filter(filterweather));
  if (error) {
    String errorMsg("Parsing Error: ");
    errorMsg.concat(error.c_str());
    errorHandler(errorMsg);
  }
  float wind = weatherDoc["wind"]["speed"];
  unsigned long unixtime = weatherDoc["dt"];
  int timeM = month(unixtime);
  int timeH = hour(unixtime);
  if (timeM >= 3 && timeM < 10) {
    timeH += 2;
  } else {
    timeH += 1;
  }
  int uppermax = 0;
  switch (timeM) {
    case 4: uppermax = 19.5; break;
    case 5: uppermax = 20; break;
    case 6: uppermax = 20.5; break;
    case 7: uppermax = 21; break;
    case 8: uppermax = 21; break;
    case 9: uppermax = 20; break;
    case 10: uppermax = 19.5; break;
  }
  weathers = weatherDoc["weather"].as<JsonArray>();
  int count = 0;
  int weatherids[weathers.size()];
  for(JsonObject v : weathers) {
    const int id = v["id"].as<int>();
    weatherids[count] = id;
    count++;
  }
  goodWeather = true;
  for (int obj : weatherids) {
    if (obj < 800) {
      goodWeather = false;
    }
  }
  if (timeM >= 4 && timeM <= 10 && timeH >= 8 && timeH < uppermax) {
    goodTime = true;
  }
  if (wind < 30) {
    goodWind = true;
  }
  Serial.println("Done!");
  Serial.println();

  download("http://api.openweathermap.org/data/2.5/forecast?q=Kirchhellen,de&appid=a02aeb3716cc0681332cb38fe5625bab", "Forecast Website");

  Serial.print("Processing Data....");
  for (int i = 0; i < 3; i++) {
    filterforecast["list"][i]["dt"] = true;
    filterforecast["list"][i]["weather"] = true;
    filterforecast["list"][i]["wind"] = true;
  }
  error = deserializeJson(forecastDoc, responseData, DeserializationOption::Filter(filterforecast));
  reports = forecastDoc["list"].as<JsonArray>();
  if (error) {
    String errorMsg("Parsing Error: ");
    errorMsg.concat(error.c_str());
    errorHandler(errorMsg);
  }
  reports = forecastDoc["list"].as<JsonArray>();
  for (int i = 1; i < 3; i++) {
    weathers = reports[i]["weather"].as<JsonArray>();
    int count = 0;
    int weatherids[weathers.size()];
    for(JsonObject v : weathers) {
      const int id = v["id"].as<int>();
      weatherids[count] = id;
      count++;
    }
    goodEstimate = true;
    for (int obj : weatherids) {
      if (obj < 800) {
        goodEstimate = false;
      }
    }
  }
  Serial.println("Done!");
  Serial.println();

  int duration = 1000;
  //red = time, blue = weather, green = wind, white = forecast
  if (goodTime) {
    Serial.println("Time is optimal.");
  } else {
    Serial.println("Time isn't optimal.");
    rgbBlink(true, false, false, duration);
  }
  if (goodWind) {
    Serial.println("Wind is optimal.");
  } else {
    Serial.println("Wind isn't optimal.");
    rgbBlink(true, true, false, duration);
  }
  if (goodWeather) {
    Serial.println("Weather is optimal.");
  } else {
    Serial.println("Weather isn't optimal.");
    rgbBlink(false, false, true, duration);
  }
  if (goodEstimate) {
    Serial.println("Forecast is optimal.");
  } else {
    Serial.println("Forecast isn't optimal.");
    rgbBlink(true, true, true, duration);
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