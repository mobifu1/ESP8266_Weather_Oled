//ESP8266 Weather Oled Station

#include "SSD1306Wire.h"
#include <ESP8266WiFi.h>
#include <JsonListener.h>
#include <time.h>
#include <Ticker.h>
#include <math.h>
#include "OpenWeatherMapForecast.h"
#include "OpenWeatherMapIcons.h"

// Declare OLED display 128x32 Pixels  0,91 inch
#define OLED_SDA 4
#define OLED_SCL 5
#define OLED_RST 16

SSD1306Wire display(0x3c, OLED_SDA, OLED_SCL, GEOMETRY_128_32);

// Initiate the clients
OpenWeatherMapForecast client;

String OPEN_WEATHER_MAP_APP_ID = "xxx";
String OPEN_WEATHER_MAP_LOCATION_ID = "2911298";//Hamburg
String OPEN_WEATHER_MAP_LANGUAGE = "de";
boolean IS_METRIC = true;
const uint8_t MAX_FORECASTS = 7;
boolean get_weather = false;

// Display pages
uint8_t current_page = 0;
const uint8_t max_pages = MAX_FORECASTS;
const uint8_t max_values_on_page = 14;
boolean show_next_page = false;

// WiFi Settings
const char* ESP_HOST_NAME = "ESP-" + ESP.getFlashChipId();
const char* WIFI_SSID     = "WiFi";
const char* WIFI_PASSWORD = "Password";

// Initiate the Wifi Client
WiFiClient wifiClient;

// Initiate timer
Ticker timer_0; // get data from openweathermap
Ticker timer_1; // change pages on display

// Weather value table
String weather_values[max_pages][max_values_on_page] = { }; //Y=Pages X=Values
String einheiten[max_values_on_page] = { "", "°C", "°C", "°C", "%", "mbar", "m/s", "deg", "%", "mm", "", "" , "" , ""};
String city_name = "Hamburg";

// Service
boolean debuging = true;
String version_ = "V0.9-beta";
//--------------------------------------------------------------------------
void setup() {

  // Reset the display via reset pin
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, HIGH);
  delay(200);
  digitalWrite(OLED_RST, LOW);
  delay(200);
  digitalWrite(OLED_RST, HIGH);
  delay(200);

  display.init();
  display.flipScreenVertically();//180grad
  //display.invertDisplay();
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 11, version_);
  display.display();
  delay(2000);

  //Initialize Ticker
  timer_0.attach(1200, timer_0_event);//every 20min
  timer_1.attach(5, timer_1_event);//every 5 sec

  Serial.begin(115200);
  delay(500);

  connectWifi();

  get_weather_forecasts();
}
//--------------------------------------------------------------------------
void loop() {

  if (get_weather == true) {
    get_weather_forecasts();
    get_weather = false;
  }

  if (show_next_page == true) {
    display_weather(current_page);
    show_next_page = false;
  }
}
//--------------------------------------------------------------------------
void connectWifi() {

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.println(WiFi.localIP());
  Serial.println();
}
//--------------------------------------------------------------------------
void get_weather_forecasts() {

  OpenWeatherMapForecastData data[MAX_FORECASTS];
  client.setMetric(IS_METRIC);
  client.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  uint8_t allowedHours[] = {0, 12};
  client.setAllowedHours(allowedHours, 2);
  uint8_t foundForecasts = client.updateForecastsById(data, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_ID, MAX_FORECASTS);

  for (uint8_t i = 0; i < foundForecasts; i++) {

    weather_values[i][0] = city_name;
    weather_values[i][1] = String((int)round(data[i].temp));
    weather_values[i][2] = String((int)round(data[i].tempMin));
    weather_values[i][3] = String((int)round(data[i].tempMax));
    weather_values[i][4] = String(data[i].humidity);
    weather_values[i][5] = String((int)round(data[i].pressureGroundLevel));
    weather_values[i][6] = String((int)round(data[i].windSpeed));
    weather_values[i][7] = String((int)round(data[i].windDeg));
    weather_values[i][8] = String(data[i].clouds);
    weather_values[i][9] = String(data[i].rain);
    weather_values[i][10] = String(data[i].icon);
    weather_values[i][11] = String(data[i].iconMeteoCon);
    weather_values[i][12] = String(data[i].description);
    weather_values[i][13] = String(data[i].observationTimeText);
  }
}
//--------------------------------------------------------------------------
String degrees_to_direction(String deg) {

  int degrees_ = deg.toInt();
  String dir = "";

  if (degrees_ >= 0 && degrees_ < 23) dir = "N ";
  if (degrees_ >= 23 && degrees_ < 68) dir = "NO ";
  if (degrees_ >= 68 && degrees_ < 113) dir = "O ";
  if (degrees_ >= 113 && degrees_ < 158) dir = "SO ";
  if (degrees_ >= 158 && degrees_ < 203) dir = "S ";
  if (degrees_ >= 203 && degrees_ < 248) dir = "SW ";
  if (degrees_ >= 248 && degrees_ < 293) dir = "W ";
  if (degrees_ >= 293 && degrees_ < 338) dir = "NW ";
  if (degrees_ >= 338 && degrees_ <= 360) dir = "N ";

  return dir;
}
//--------------------------------------------------------------------------
void display_weather(uint8_t y) {

  if (debuging) {
    for (int y = 0; y < max_pages; y++) {//page
      for (int x = 0; x < max_values_on_page; x++) {
        Serial.print(weather_values[y][x]);
        Serial.print(einheiten[x]);
        Serial.print(",");
      }
      Serial.println();
    }
  }

  if (y == 0) {

    display.clear();
    //----------------------------------icon:
    String icon_id = weather_values[0][10];
    draw_weather_icon(icon_id);
    //----------------------------------
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(128, 0, city_name);
    display.setFont(ArialMT_Plain_24);
    display.drawString(128, 10, weather_values[0][1] + einheiten[1]);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_10);

    int length_ = weather_values[0][13].length(); //2018-12-30 12:00:00
    if (length_ >= 19) {
      String date = weather_values[0][13];
      String month_ = date.substring(5, 7);
      String day_ = date.substring(8, 10);
      display.drawString(60, 0, (day_ + "." + month_ + ".")); //30.12.
      String time_ = weather_values[0][13];
      time_ = time_.substring(11, 16);
      display.drawString(60, 22, time_); //12:00
    }

    display.drawString(60, 11, "o---");
    display.display();
  }

  if (y == 1) {

    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    String dir = degrees_to_direction(weather_values[0][7]);
    display.drawString(0, 0, dir + weather_values[0][6] + einheiten[6]);
    display.drawString(0, 11, "/// " + weather_values[0][9] + einheiten[9]);

    String description = weather_values[0][12];
    int length = description.length();
    if (length > 15) {
      description = description.substring(0, 14);
    }
    display.drawString(0, 22, description );

    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(128, 0, city_name);
    display.setFont(ArialMT_Plain_24);
    display.drawString(128, 10, weather_values[0][1] + einheiten[1]);
    display.display();
  }

  if (y == 2) {

    display.clear();
    //----------------------------------icon:
    String icon_id = weather_values[1][10];
    draw_weather_icon(icon_id);
    //----------------------------------
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(128, 0, city_name);
    display.setFont(ArialMT_Plain_24);
    display.drawString(128, 10, weather_values[1][1] + einheiten[1]);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_10);

    int length_ = weather_values[1][13].length(); //2018-12-30 12:00:00
    if (length_ >= 19) {
      String date = weather_values[1][13];
      String month_ = date.substring(5, 7);
      String day_ = date.substring(8, 10);
      display.drawString(60, 0, (day_ + "." + month_ + ".")); //30.12.
      String time_ = weather_values[1][13];
      time_ = time_.substring(11, 16);
      display.drawString(60, 22, time_); //12:00
    }

    display.drawString(60, 11, "-o--");
    display.display();
  }

  if (y == 3) {

    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    String dir = degrees_to_direction(weather_values[1][7]);
    display.drawString(0, 0, dir + weather_values[1][6] + einheiten[6]);
    display.drawString(0, 11, "/// " + weather_values[1][9] + einheiten[9]);

    String description = weather_values[1][12];
    int length = description.length();
    if (length > 15) {
      description = description.substring(0, 14);
    }
    display.drawString(0, 22, description );

    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(128, 0, city_name);
    display.setFont(ArialMT_Plain_24);
    display.drawString(128, 10, weather_values[1][1] + einheiten[1]);
    display.display();
  }

  if (y == 4) {

    display.clear();
    //----------------------------------icon:
    String icon_id = weather_values[2][10];
    draw_weather_icon(icon_id);
    //----------------------------------
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(128, 0, city_name);
    display.setFont(ArialMT_Plain_24);
    display.drawString(128, 10, weather_values[2][1] + einheiten[1]);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_10);

    int length_ = weather_values[2][13].length(); //2018-12-30 12:00:00
    if (length_ >= 19) {
      String date = weather_values[2][13];
      String month_ = date.substring(5, 7);
      String day_ = date.substring(8, 10);
      display.drawString(60, 0, (day_ + "." + month_ + ".")); //30.12.
      String time_ = weather_values[2][13];
      time_ = time_.substring(11, 16);
      display.drawString(60, 22, time_); //12:00
    }
    display.drawString(60, 11, "--o-");
    display.display();
  }

  if (y == 5) {

    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    String dir = degrees_to_direction(weather_values[2][7]);
    display.drawString(0, 0, dir + weather_values[2][6] + einheiten[6]);
    display.drawString(0, 11, "/// " + weather_values[2][9] + einheiten[9]);

    String description = weather_values[0][12];
    int length = description.length();
    if (length > 15) {
      description = description.substring(0, 14);
    }
    display.drawString(0, 22, description );

    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(128, 0, city_name);
    display.setFont(ArialMT_Plain_24);
    display.drawString(128, 10, weather_values[2][1] + einheiten[1]);
    display.display();
  }

  if (y == 6) {

    display.clear();
    //----------------------------------icon:
    String icon_id = weather_values[3][10];
    draw_weather_icon(icon_id);
    //----------------------------------
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(128, 0, city_name);
    display.setFont(ArialMT_Plain_24);
    display.drawString(128, 10, weather_values[3][1] + einheiten[1]);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_10);

    int length_ = weather_values[3][13].length(); //2018-12-30 12:00:00
    if (length_ >= 19) {
      String date = weather_values[3][13];
      String month_ = date.substring(5, 7);
      String day_ = date.substring(8, 10);
      display.drawString(60, 0, (day_ + "." + month_ + ".")); //30.12.
      String time_ = weather_values[3][13];
      time_ = time_.substring(11, 16);
      display.drawString(60, 22, time_); //12:00
    }

    display.drawString(60, 11, "---o");
    display.display();
  }

  if (y == 7) {

    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    String dir = degrees_to_direction(weather_values[3][7]);
    display.drawString(0, 0, dir + weather_values[3][6] + einheiten[6]);
    display.drawString(0, 11, "/// " + weather_values[3][9] + einheiten[9]);

    String description = weather_values[0][12];
    int length = description.length();
    if (length > 15) {
      description = description.substring(0, 14);
    }
    display.drawString(0, 22, description );

    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(128, 0, city_name);
    display.setFont(ArialMT_Plain_24);
    display.drawString(128, 10, weather_values[3][1] + einheiten[1]);
    display.display();
  }
}
//--------------------------------------------------------------------------
void draw_weather_icon(String icon_id) {

  if (icon_id == "01d") display.drawXbm(0, 5, icon_width, icon_height, meteo_01d_bits);
  if (icon_id == "01n") display.drawXbm(0, 5, icon_width, icon_height, meteo_01n_bits);
  if (icon_id == "02d") display.drawXbm(0, 5, icon_width, icon_height, meteo_02d_bits);
  if (icon_id == "02n") display.drawXbm(0, 5, icon_width, icon_height, meteo_02n_bits);
  if (icon_id == "03d") display.drawXbm(0, 5, icon_width, icon_height, meteo_03d_bits);
  if (icon_id == "03n") display.drawXbm(0, 5, icon_width, icon_height, meteo_03n_bits);
  if (icon_id == "04d") display.drawXbm(0, 5, icon_width, icon_height, meteo_04d_bits);
  if (icon_id == "04n") display.drawXbm(0, 5, icon_width, icon_height, meteo_04n_bits);
  if (icon_id == "09d") display.drawXbm(0, 5, icon_width, icon_height, meteo_09d_bits);
  if (icon_id == "09n") display.drawXbm(0, 5, icon_width, icon_height, meteo_09n_bits);
  if (icon_id == "10d") display.drawXbm(0, 5, icon_width, icon_height, meteo_10d_bits);
  if (icon_id == "10n") display.drawXbm(0, 5, icon_width, icon_height, meteo_10n_bits);
  if (icon_id == "11d") display.drawXbm(0, 5, icon_width, icon_height, meteo_11d_bits);
  if (icon_id == "11n") display.drawXbm(0, 5, icon_width, icon_height, meteo_11n_bits);
  if (icon_id == "13d") display.drawXbm(0, 5, icon_width, icon_height, meteo_13d_bits);
  if (icon_id == "13n") display.drawXbm(0, 5, icon_width, icon_height, meteo_13n_bits);
  if (icon_id == "50d") display.drawXbm(0, 5, icon_width, icon_height, meteo_50d_bits);
  if (icon_id == "50n") display.drawXbm(0, 5, icon_width, icon_height, meteo_50n_bits);
}
//--------------------------------------------------------------------------
void timer_0_event() {

  if (debuging) Serial.println("Get Weather:");
  get_weather = true;
}
//--------------------------------------------------------------------------
void timer_1_event() {

  current_page ++;
  if (current_page == max_pages + 1)current_page = 0;
  if (debuging) Serial.println("Page:" + String(current_page));
  show_next_page = true;
}
//--------------------------------------------------------------------------

