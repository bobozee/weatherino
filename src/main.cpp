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

// INFO: due to a struct array not supporting conventional array methods (like size()),
// having this variable is crucial later on
const int amntOfNetworks = 2;

// amount of hours to be forecasted, max = 48, 0 means last hour
const int forecastrange = 2;

WiFiClient client;
HTTPClient http;
String responseData;
DynamicJsonDocument weatherDoc(15360);
JsonArray reports;
StaticJsonDocument<1024> filter;
Network networks[amntOfNetworks];
boolean goodTime = false;
boolean goodWeather = true;
boolean goodWind = false;
boolean goodEstimate = true;
unsigned long unixtime;

struct Network {
  String ssid;
  String password;
};

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

void setup() {

  networks[0].ssid = "Erpix";
  networks[0].password = "***REMOVED***";
  networks[1].ssid = "ErpixLAN";
  networks[1].password = "***REMOVED***";

  Serial.begin(115200);
  pinMode(powerLED, OUTPUT);
  pinMode(connectionLED, OUTPUT);
  pinMode(waterLED, OUTPUT);
  pinMode(errorLED, OUTPUT);
  pinMode(bridge, OUTPUT);
  pinMode(signalR, OUTPUT);
  pinMode(signalG, OUTPUT);
  pinMode(signalB, OUTPUT);
  filter["current"]["dt"] = true;
  filter["current"]["wind_speed"] = true;
  filter["current"]["weather"] = true;
  filter["hourly"][0]["weather"] = true;
  delay(250);
  Serial.println();
  Serial.println("Startup complete.");
  Serial.println("~~~~~~~~~~~~~~~~~");
  digitalWrite(powerLED, HIGH);
}

boolean checkTime(DynamicJsonDocument weatherDoc) {
  unsigned long timeOffset = weatherDoc["timezone_offset"];
  unixtime = weatherDoc["current"]["dt"] + timeOffset;

  int timeM = month(unixtime);
  int timeH = hour(unixtime);
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

  Serial.printf(" (Hour: %d, Month: %d)\n", timeH, timeM);

  return timeM >= 4 && timeM <= 10 && timeH >= 8 && timeH < uppermax;
}

boolean checkWeather(DynamicJsonDocument weatherDoc) {

  boolean _goodWeather = true;
  JsonArray currweather = weatherDoc["current"]["weather"].as<JsonArray>(); // get the current weather states as an array

  // INFO: the logic is reversed; while by default its true, if one bad weather is seen,
  // the boolean is false. this is to increase priority of bad weather
  Serial.print(" (Id's:");
  for(JsonVariant obj : currweather) {
    int id = obj["id"].as<int>();
    Serial.printf(" %d", id);
    _goodWeather &= (id < 800);
  }
  Serial.println(")");
  return _goodWeather;
}

boolean checkForecast(DynamicJsonDocument weatherDoc) {

  boolean _goodForecast = true;

  // get the hourly combined weather reports as an array
  JsonArray foreweather = weatherDoc["hourly"].as<JsonArray>();

  Serial.print(" (Id's:");
  for (int i = 1; i <= forecastrange; i++) { // INFO: the loop starts with 1 due to hour 0 being the latest hour, which is not important in a forecast
    JsonArray foreweatherstates = foreweather[i]["weather"].as<JsonArray>(); // get the weather states of the hour as an array
    for(JsonVariant obj : foreweatherstates) {
      int id = obj["id"].as<int>();
      _goodForecast &= (id < 800);
      Serial.printf(" %d", id);
    }
  }
  Serial.println(")");
  return _goodForecast;
}

boolean checkWind(DynamicJsonDocument weatherDoc) {
  float wind = weatherDoc["current"]["wind_speed"];
  Serial.printf(" (Speed: %d)\n", wind);
  return wind <= 2;
}

void loop() {
  boolean succeeded = false;
  boolean esc = false;
  int tries = 0;
  while (!succeeded) {
    Serial.print("Beginning to connect to network with SSID ");
    Serial.print(networks[tries].ssid);
    Serial.print(" and password ");
    Serial.println(networks[tries].password);
    Serial.print("Connecting....");
    WiFi.begin(networks[tries].ssid, networks[tries].password);
    const int rate = 250; // rate of blinking for the connection led
    const int threshold = 20; // amount of time to pass for timeout of connection attempt
    int counter = 0;
    esc = false;
    while (WiFi.status() != WL_CONNECTED && !esc) {
      digitalWrite(connectionLED, HIGH);
      delay(rate);
      digitalWrite(connectionLED, LOW);
      delay(rate);
      counter++;
      if (counter == threshold) {
        tries++;
        if (tries == amntOfNetworks) {
          errorHandler("Timeout");
        } else {
          Serial.println("Timeout, attempting next network.");
          digitalWrite(errorLED, HIGH);
          delay(500);
          digitalWrite(errorLED, LOW);
          esc = true;
        }
      }
    }
    if (esc == false) { // if the loop wasnt thrown out due to a timeout
      succeeded = true;
    }
  }
  digitalWrite(connectionLED, HIGH);
  Serial.println("Connected!");

  int trycount = 0;
  esc = false;
  while (!esc) {
    if (trycount == 3) {
      errorHandler("Failed to establish connection to website.");
    }
    Serial.print("Contacting Weather Website....");
    http.begin(client, "http://api.openweathermap.org/data/2.5/onecall?lat=51.603170&lon=6.917150&exclude=minutely,daily&appid=a02aeb3716cc0681332cb38fe5625bab");
    int httpCode = http.GET(); // fetch the GET code of the HTTP request, INFO: http.GET() is synchronous
    if (httpCode == 302) { //INFO: 302 occurs usually when the chip itself is denied internet access and is thus moved to the router's internet blockage website
        Serial.print("Failed! (");
        Serial.print(httpCode);
        Serial.println(") (Is the chip denied internet access?)");
        digitalWrite(errorLED, HIGH);
        delay(500);
        digitalWrite(errorLED, LOW);
        trycount++;
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
        trycount++;
      }
    }
    http.end();
  }

  Serial.print("Processing Data....");
  DeserializationError error = deserializeJson(weatherDoc, responseData, DeserializationOption::Filter(filter));
  if (error) {
    String errorMsg("Parsing Error: ");
    errorMsg.concat(error.c_str());
    errorHandler(errorMsg);
  }

  goodTime = checkTime(weatherDoc);
  goodWeather = checkWeather(weatherDoc);
  goodWind = checkWind(weatherDoc);
  goodEstimate = checkForecast(weatherDoc);

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

  if (goodWind) {
    Serial.print("Wind is optimal.");
  } else {
    Serial.print("Wind isn't optimal.");
    //rgbBlink(true, true, false, signalduration);
    ledBlink(waterLED, 3, 250, true);
  }

  if (goodWeather) {
    Serial.print("Weather is optimal.");
  } else {
    Serial.print("Weather isn't optimal.");
    //rgbBlink(false, false, true, signalduration);
    ledBlink(waterLED, 4, 250, true);
  }

  if (goodEstimate) {
    Serial.print("Forecast is optimal.");
  } else {
    Serial.print("Forecast isn't optimal.");
    //rgbBlink(true, true, true, signalduration);
    ledBlink(waterLED, 5, 250, true);
  }

  if (goodTime && goodWeather && goodEstimate && goodWind) {
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