#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <TimeLib.h>
#include <list>

const uint8_t powerLED = D4;
const uint8_t connectionLED = D3;
const uint8_t waterLED = D2;
const uint8_t errorLED = D5;
const uint8_t bridge = D1;
const uint8_t signalR = D8;
const uint8_t signalG = D7;
const uint8_t signalB = D6;

WiFiClient client;
HTTPClient http;
String responseData;
DynamicJsonDocument weatherDoc(15360);
JsonArray reports;

const char* ssid = "Erpix";
const char* password = "lolno";

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
void ledBlink (uint8_t led, int count, int speed, boolean postdelay) {
  for (int i = 0; i < count; i++) {
    digitalWrite(led, HIGH);
    delay(speed);
    digitalWrite(led, LOW);
    delay(speed);
  }
  if (postdelay) {
    delay(500);
  }
}

void errorHandler(String error) {
  WiFi.disconnect();
  digitalWrite(powerLED, LOW);
  digitalWrite(connectionLED, LOW);
  digitalWrite(waterLED, LOW);
  digitalWrite(bridge, LOW);
  Serial.println(error);
  Serial.println("An Error occured. This Task will be terminated. Restart in 10 minutes; Press the 'Reset' button to restart the Program immediately.");
  digitalWrite(errorLED, HIGH);
  delay(1000 * 60 * 10);
  digitalWrite(errorLED, LOW);
  ESP.reset();
}

/**
 * Calculates local hour for WEST (Western europen time zone)
 * */
int localHourTime(signed long unixtime) {
  int _month = month(unixtime);
  int _hour = hour(unixtime);
  if (_month >= 3 && _month <= 10) {
    // most likely it is daily savings time, let's check the details

    // make a time as 1 of April, this year
    tmElements_t time;
    time.Month = 4; // april
    time.Day = 1;
    time.Year = year(unixtime);
    time.Hour = 0;
    time.Minute = 0;
    time.Second = 0;
    time_t firstOfApril = makeTime(time);
    time_t _previousSunday = previousSunday(firstOfApril) + 60 * 60; // last sunday in march at 01 o'clock

    if (unixtime >= _previousSunday) {
      //  ok, the last sunday in march , 01 o'clock has passed
      time.Month = 11;  // November
      time.Day = 1;
      time_t firstOfNovember = makeTime(time);
      time_t _previousSunday = previousSunday(firstOfNovember) + 60 * 60; // last sunday in october, 01 o'clock
      if (unixtime <= _previousSunday) {
        // indeed, it is daily savings time
        _hour += 3; // time offset must be manually added (+ 2)
        if (_hour >= 24) {
          return _hour - 24;
        } else {
          return _hour;
        }
      }
    }
  }
  _hour += 2;
  if (_hour >= 24) {
    return _hour - 24;
  } else {
    return _hour;
  }
}

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
  delay(250);
  Serial.println();
  Serial.println("Startup complete.");
  Serial.println("~~~~~~~~~~~~~~~~~");
  digitalWrite(powerLED, HIGH);
}

void loop() {
  int tries = 0;
  Serial.print("Beginning to connect to network with SSID ");
  Serial.print(ssid);
  Serial.print(" and password ");
  Serial.println(password);
  Serial.print("Connecting....");
  WiFi.begin(ssid, password);
  const int rate = 250; // rate of blinking for the connection led
  const int threshold = 20; // amount of time to pass for timeout of connection attempt
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(connectionLED, HIGH);
    delay(rate);
    digitalWrite(connectionLED, LOW);
    delay(rate);
    counter++;
    if (counter == threshold) {
      tries++;
      counter = 0;
      if (tries == 3) {
        errorHandler("Timeout");
      }
    }
  }
  digitalWrite(connectionLED, HIGH);
  Serial.println("Connected!");

  boolean esc = false;
  tries = 0;
  while (!esc) {
    if (tries == 3) {
      errorHandler("Failed to establish connection to website.");
    }
    Serial.print("Contacting Weather Website....");
    http.begin(client, "***REMOVED***");
    int httpCode = http.GET(); // fetch the GET code of the HTTP request, INFO: http.GET() is synchronous
    if (httpCode == 302) { //INFO: 302 occurs usually when the chip itself is denied internet access and is thus moved to the router's internet blockage website
        Serial.print("Failed! (");
        Serial.print(httpCode);
        Serial.println(") (Is the chip denied internet access?)");
        digitalWrite(errorLED, HIGH);
        delay(500);
        digitalWrite(errorLED, LOW);
        delay(500);
        tries++;
    } else {
      if (httpCode >= 200 && httpCode < 400) {
        responseData = http.getString(); // if the code is good, get the fetchable data
        Serial.print("Done! (");
        Serial.print(httpCode);
        Serial.println(")");
        esc = true; // escape from the fail-repeating loop with requested information
      } else {
        Serial.print("Failed! (");
        Serial.print(httpCode);
        Serial.print(", ");
        Serial.print(http.errorToString(httpCode).c_str());
        Serial.println(")");
        digitalWrite(errorLED, HIGH);
        delay(500);
        digitalWrite(errorLED, LOW);
        delay(500);
        tries++;
      }
    }
    http.end();
  }

  Serial.print("Processing Data....");
  DeserializationError error = deserializeJson(weatherDoc, responseData);
  if (error) {
    String errorMsg("Parsing Error: ");
    errorMsg.concat(error.c_str());
    errorHandler(errorMsg);
  }

  float wind = weatherDoc["wind"]["speed"];
  unsigned long unixtime = weatherDoc["dt"];
  int timeM = month(unixtime);
  int timeH = localHourTime(unixtime); // local hour in germany preserving daily savings time
  int uppermax = 0;
  switch (timeM) { // dynamically change shutdown hour depending on month
    case 4: uppermax = 20; break;
    case 5: uppermax = 20; break;
    case 6: uppermax = 21; break;
    case 7: uppermax = 21; break;
    case 8: uppermax = 21; break;
    case 9: uppermax = 20; break;
    case 10: uppermax = 20; break;
  }
  JsonArray weather = weatherDoc["weather"].as<JsonArray>(); // get the current weather states as an array
  int weatherids[weather.size()];
  int index = 0; // INFO: the index is explicitly for the currweatherids array
  for(JsonVariant obj : weather) {
    weatherids[index] = obj["id"].as<int>(); // save states' id in array
    index++;
  }
  boolean goodTime = false;
  boolean goodWeather = true;
  boolean goodWind = false;
  for (int id : weatherids) {
    if (id < 800) { // INFO: the logic is reversed; while by default its true, if one bad weather is seen, the boolean is false. this is to increase priority of bad weather
      goodWeather = false;
    }
  }
  if (timeM >= 4 && timeM <= 10 && timeH >= 8 && timeH < uppermax) {
    goodTime = true;
  }
  if (wind <= 2) {
    goodWind = true;
  }
  Serial.println("Done!");

  ledBlink(waterLED, 4, 50, true);
  //int signalduration = 1000;
  /*
    2 / red   = time
    3 / green = wind
    4 / blue   = weather
    5 / white = forecast 
  */
  if (goodTime) {
    Serial.print("Time is optimal.");
  } else {
    Serial.print("Time isn't optimal. ");
    //rgbBlink(true, false, false, signalduration);
    ledBlink(waterLED, 2, 250, true);
  }
  Serial.print(" (Hour: ");
  Serial.print(timeH);
  Serial.print(", Month: ");
  Serial.print(timeM);
  Serial.println(")");

  if (goodWind) {
    Serial.print("Wind is optimal.");
  } else {
    Serial.print("Wind isn't optimal.");
    //rgbBlink(true, true, false, signalduration);
    ledBlink(waterLED, 3, 250, true);
  }
  Serial.print(" (Speed: ");
  Serial.print(wind);
  Serial.println(")");

  if (goodWeather) {
    Serial.print("Weather is optimal.");
  } else {
    Serial.print("Weather isn't optimal.");
    //rgbBlink(false, false, true, signalduration);
    ledBlink(waterLED, 4, 250, true);
  }

  Serial.print(" (Id's:");
  for (int id : weatherids) {
    Serial.print(" ");
    Serial.print(id);
  }
  Serial.println(")");

  if (goodTime && goodWeather && goodWind) {
    Serial.println("Outcome: Environment fits requirements. Pump is on.");
    digitalWrite(waterLED, HIGH);
    digitalWrite(bridge, HIGH);
  } else {
    Serial.println("Outcome: Environment does not fit requirements. Pump is off.");
    digitalWrite(waterLED, LOW);
    digitalWrite(bridge, LOW);
    ledBlink(waterLED, 4, 50, false);
  }

  Serial.println();
  WiFi.disconnect();
  digitalWrite(connectionLED, LOW);
  unsigned long timeoutTime = (60 - minute(unixtime)) * 60000;
  Serial.println("All done! Disconnecting and sleeping until the next hour. 'Till then!");
  delay(timeoutTime + 180000);
}
