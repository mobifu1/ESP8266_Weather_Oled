/*
  ESP8266 Weather Oled Station
  How it works:

  Importand Information: Arduino IDE > Arduino/Tools/Erase Flash:All Flash Contents !!!
  After flashing you can send variables by the same serial-comport in to the eeprom of esp8622

  Commands:
  ssid=xxxxx
  passwort=xxxxx
  api_key =xxxxx                    from openweathermap
  location_name=Hamburg
  location_id=xxxxx                 from openweathermap
  debuging=false/true               serial debuging informations
  invert_display=false/true
  flip_display=false/true
  display_mode=0-2
  requ_hour_1=12 > 12:00            0/3/6/9/12/15/18/21
  requ_hour_2=21 > 21:00            0/3/6/9/12/15/18/21
  config -get                       show all stored variable on serial port
  ip -get                           local IP

  after setting changes (ssid or password), you have to restart the device.
*/

#include "SSD1306Wire.h"
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <JsonListener.h>
#include <time.h>
#include <Ticker.h>
#include <math.h>
#include "OpenWeatherMapForecast.h"
#include "OpenWeatherMapIcons.h"

//https://blog.squix.org/2015/05/esp8266-nodemcu-how-to-create-xbm.html

// Declare OLED display 128x32 Pixels  0,91 inch
#define OLED_SDA 4
#define OLED_SCL 5
#define OLED_RST 16

SSD1306Wire display(0x3c, OLED_SDA, OLED_SCL, GEOMETRY_128_32);

// Initiate the clients
OpenWeatherMapForecast client;

String OPEN_WEATHER_MAP_APP_ID = "";
String OPEN_WEATHER_MAP_LOCATION_ID = "";
String OPEN_WEATHER_MAP_LANGUAGE = "de";
boolean IS_METRIC = true;
const uint8_t MAX_FORECASTS = 7;
boolean get_weather = false;
uint8_t requ_hour_1; //  0 = 00:00
uint8_t requ_hour_2; // 12 = 12:00

// Display pages
uint8_t current_page = 0;
const uint8_t max_pages = MAX_FORECASTS;
const uint8_t max_values_on_page = 14;
boolean show_next_page = false;
boolean flip_display;
boolean invert_display;
uint8_t display_mode;

// Display rows
uint8_t xd1 = 0;
uint8_t xd2 = 38;
uint8_t xd21 = 45;
uint8_t xd3 = 63;
uint8_t xd4 = 95;
uint8_t xd5 = 127;
uint8_t yd1 = 0;
uint8_t yd11 = 10;
uint8_t yd2 = 11;
uint8_t yd3 = 22;

// WiFi Settings
const char* ESP_HOST_NAME = "ESP-" + ESP.getFlashChipId();
char* WIFI_SSID     = "ssid------------";
char* WIFI_PASSWORD = "password------------";

// Initiate the Wifi Client
WiFiClient wifiClient;

// Initiate timer
Ticker timer_0; // get data from openweathermap
Ticker timer_1; // change pages on display

// Weather value table
String weather_values[max_pages][max_values_on_page] = { }; //Y=Pages X=Values
String einheiten[max_values_on_page] = { "", "°C", "°C", "°C", "%", "mbar", "m/s", "deg", "%", "mm", "", "" , "" , ""};
String city_name = "?";

// EEprom statements
const int eeprom_size = 256 ; //Size can be anywhere between 4 and 4096 bytes

int debuging_eeprom_address = 0;        //boolean value
int flip_display_eeprom_address = 1;    //boolean value
int invert_display_eeprom_address = 2;  //boolean value
int display_mode_eeprom_address = 6;    //int value
int requ_hour_1_eeprom_address = 8;     //int value
int requ_hour_2_eeprom_address = 10;    //int value
int ssid_eeprom_address = 16;           //string max 22
int password_eeprom_address = 40;       //string max 32
int location_id_eeprom_address = 72;    //string max 32
int location_name_eeprom_address = 104; //string max 32
int api_key_eeprom_address = 136;       //string max 32

String serial_line_0;//read bytes from serial port 0

// Service
boolean debuging;
String version_ = "V1.0.1-r";
//--------------------------------------------------------------------------
void setup() {

  delay(2000);
  Serial.begin(115200);

  //Load config
  load_config();

  // Reset the display via reset pin
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, HIGH);
  delay(200);
  digitalWrite(OLED_RST, LOW);
  delay(200);
  digitalWrite(OLED_RST, HIGH);
  delay(200);

  display.init();
  if (flip_display == true)display.flipScreenVertically();//180grad
  if (invert_display == true)display.invertDisplay();//B/W > W/B
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 11, version_);
  display.display();
  delay(2000);

  //Initialize Ticker
  timer_0.attach(1200, timer_0_event);//every 20min
  timer_1.attach(5, timer_1_event);//every 5 sec

  delay(500);

  connectWifi();

  get_weather_forecasts();
}
//--------------------------------------------------------------------------
void loop() {

  read_serial_port_0();

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
    Serial.print(".");
    read_serial_port_0();
    delay(1000);
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
  uint8_t allowedHours[] = {requ_hour_1, requ_hour_2};
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
String ms_to_beaufort(String speed_) {

  float v = float(speed_.toInt());
  v = v / 0.836;
  int beaufort = int(pow(v , 0.666));
  String beaufort_string = (String(beaufort) + " Bft");

  if (beaufort >= 10 && beaufort <= 12) {
    beaufort_string += " !!";
  }

  return beaufort_string;
}
//--------------------------------------------------------------------------
String degrees_to_direction(String deg) {

  float degrees_ = float(deg.toInt());
  String dir = "";

  if (degrees_ >= 0.000 && degrees_ < 11.25) dir = "N ";
  if (degrees_ >= 11.25 && degrees_ < 33.75) dir = "NNO ";
  if (degrees_ >= 33.75 && degrees_ < 56.25) dir = "NO ";
  if (degrees_ >= 56.25 && degrees_ < 78.75) dir = "ONO ";
  if (degrees_ >= 78.75 && degrees_ < 101.25) dir = "O ";
  if (degrees_ >= 101.25 && degrees_ < 123.75) dir = "OSO ";
  if (degrees_ >= 123.75 && degrees_ < 146.25 ) dir = "SO ";
  if (degrees_ >= 146.25 && degrees_ < 168.75) dir = "SSO ";
  if (degrees_ >= 168.75 && degrees_ < 191.25) dir = "S ";
  if (degrees_ >= 191.25 && degrees_ < 213.75) dir = "SSW ";
  if (degrees_ >= 213.75 && degrees_ < 236.25) dir = "SW ";
  if (degrees_ >= 236.25 && degrees_ < 258.75) dir = "WSW ";
  if (degrees_ >= 258.75 && degrees_ < 281.25) dir = "W ";
  if (degrees_ >= 281.25 && degrees_ < 303.75) dir = "WNW ";
  if (degrees_ >= 303.75 && degrees_ < 326.25) dir = "NW ";
  if (degrees_ >= 326.25 && degrees_ < 348.75) dir = "NNW ";
  if (degrees_ >= 348.75 && degrees_ <= 360 ) dir = "N ";

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
  int forcast_index;

  if (y == 0) {

    forcast_index = 0;

    display.clear();
    //----------------------------------icon left:
    String icon_id = weather_values[forcast_index][10];
    draw_weather_icon(icon_id);
    //----------------------------------text right
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(xd5, yd1, city_name);//City
    display.setFont(ArialMT_Plain_24);
    display.drawString(xd5, yd11, weather_values[forcast_index][1] + einheiten[1]); //Temp
    //----------------------------------text mid
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    int length_ = weather_values[forcast_index][13].length(); //2018-12-30 12:00:00
    if (length_ >= 19) {
      String date = weather_values[forcast_index][13];
      String month_ = date.substring(5, 7);
      String day_ = date.substring(8, 10);
      display.drawString(xd2, yd1, (day_ + "." + month_ + ".")); //Date
      display.drawString(xd21, yd2, "o---");
      String time_ = weather_values[forcast_index][13];
      time_ = time_.substring(11, 16);
      display.drawString(xd2, yd3, time_); //Time
      //----------------------------------
    }
    display.display();
  }

  if (y == 1) {

    forcast_index = 0;

    display.clear();
    //----------------------------------text  left
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    String dir = degrees_to_direction(weather_values[forcast_index][7]);
    String bft = ms_to_beaufort(weather_values[forcast_index][6]);
    display.drawString(xd1, yd1, dir + bft);//Wind
    if (display_mode == 0)display.drawString(xd1, yd2, "/// " + weather_values[forcast_index][9] + einheiten[9]); //Rain
    if (display_mode == 1)display.drawString(xd1, yd2, "Hmy " + weather_values[forcast_index][4] + einheiten[4]); //Humidity
    if (display_mode == 2)display.drawString(xd1, yd2, weather_values[forcast_index][5] + einheiten[5]);//Pressure
    String description = weather_values[forcast_index][12];
    int length = description.length();
    if (length > 15) {
      description = description.substring(0, 14);
    }
    display.drawString(xd1, yd3, description );//Description
    //----------------------------------text right
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(xd5, yd1, city_name);//City
    display.setFont(ArialMT_Plain_24);
    display.drawString(xd5, yd11, weather_values[forcast_index][1] + einheiten[1]);//Temp
    //----------------------------------
    display.display();
  }

  if (y == 2) {

    forcast_index = 1;

    display.clear();
    //----------------------------------icon left:
    String icon_id = weather_values[forcast_index][10];
    draw_weather_icon(icon_id);
    //----------------------------------text right
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(xd5, yd1, city_name);//City
    display.setFont(ArialMT_Plain_24);
    display.drawString(xd5, yd11, weather_values[forcast_index][1] + einheiten[1]); //Temp
    //----------------------------------text mid
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    int length_ = weather_values[forcast_index][13].length(); //2018-12-30 12:00:00
    if (length_ >= 19) {
      String date = weather_values[forcast_index][13];
      String month_ = date.substring(5, 7);
      String day_ = date.substring(8, 10);
      display.drawString(xd2, yd1, (day_ + "." + month_ + ".")); //Date
      display.drawString(xd21, yd2, "-o--");
      String time_ = weather_values[forcast_index][13];
      time_ = time_.substring(11, 16);
      display.drawString(xd2, yd3, time_); //Time
      //----------------------------------
    }
    display.display();
  }

  if (y == 3) {

    forcast_index = 1;

    display.clear();
    //----------------------------------text  left
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    String dir = degrees_to_direction(weather_values[forcast_index][7]);
    String bft = ms_to_beaufort(weather_values[forcast_index][6]);
    display.drawString(xd1, yd1, dir + bft);//Wind
    if (display_mode == 0)display.drawString(xd1, yd2, "/// " + weather_values[forcast_index][9] + einheiten[9]); //Rain
    if (display_mode == 1)display.drawString(xd1, yd2, "Hmy " + weather_values[forcast_index][4] + einheiten[4]);//Humidity
    if (display_mode == 2)display.drawString(xd1, yd2, weather_values[forcast_index][5] + einheiten[5]);//Pressure
    String description = weather_values[forcast_index][12];
    int length = description.length();
    if (length > 15) {
      description = description.substring(0, 14);
    }
    display.drawString(xd1, yd3, description );//Description
    //----------------------------------text right
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(xd5, yd1, city_name);//City
    display.setFont(ArialMT_Plain_24);
    display.drawString(xd5, yd11, weather_values[forcast_index][1] + einheiten[1]);//Temp
    //----------------------------------
    display.display();
  }

  if (y == 4) {

    forcast_index = 2;

    display.clear();
    //----------------------------------icon left:
    String icon_id = weather_values[forcast_index][10];
    draw_weather_icon(icon_id);
    //----------------------------------text right
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(xd5, yd1, city_name);//City
    display.setFont(ArialMT_Plain_24);
    display.drawString(xd5, yd11, weather_values[forcast_index][1] + einheiten[1]); //Temp
    //----------------------------------text mid
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    int length_ = weather_values[forcast_index][13].length(); //2018-12-30 12:00:00
    if (length_ >= 19) {
      String date = weather_values[forcast_index][13];
      String month_ = date.substring(5, 7);
      String day_ = date.substring(8, 10);
      display.drawString(xd2, yd1, (day_ + "." + month_ + ".")); //Date
      display.drawString(xd21, yd2, "--o-");
      String time_ = weather_values[forcast_index][13];
      time_ = time_.substring(11, 16);
      display.drawString(xd2, yd3, time_); //Time
      //----------------------------------
    }
    display.display();
  }

  if (y == 5) {

    forcast_index = 2;

    display.clear();
    //----------------------------------text  left
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    String dir = degrees_to_direction(weather_values[forcast_index][7]);
    String bft = ms_to_beaufort(weather_values[forcast_index][6]);
    display.drawString(xd1, yd1, dir + bft);//Wind
    if (display_mode == 0)display.drawString(xd1, yd2, "/// " + weather_values[forcast_index][9] + einheiten[9]); //Rain
    if (display_mode == 1)display.drawString(xd1, yd2, "Hmy " + weather_values[forcast_index][4] + einheiten[4]);//Humidity
    if (display_mode == 2)display.drawString(xd1, yd2, weather_values[forcast_index][5] + einheiten[5]);//Pressure
    String description = weather_values[forcast_index][12];
    int length = description.length();
    if (length > 15) {
      description = description.substring(0, 14);
    }
    display.drawString(xd1, yd3, description );//Description
    //----------------------------------text right
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(xd5, yd1, city_name);//City
    display.setFont(ArialMT_Plain_24);
    display.drawString(xd5, yd11, weather_values[forcast_index][1] + einheiten[1]);//Temp
    //----------------------------------
    display.display();
  }

  if (y == 6) {

    forcast_index = 3;

    display.clear();
    //----------------------------------icon left:
    String icon_id = weather_values[forcast_index][10];
    draw_weather_icon(icon_id);
    //----------------------------------text right
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(xd5, yd1, city_name);//City
    display.setFont(ArialMT_Plain_24);
    display.drawString(xd5, yd11, weather_values[forcast_index][1] + einheiten[1]); //Temp
    //----------------------------------text mid
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    int length_ = weather_values[forcast_index][13].length(); //2018-12-30 12:00:00
    if (length_ >= 19) {
      String date = weather_values[forcast_index][13];
      String month_ = date.substring(5, 7);
      String day_ = date.substring(8, 10);
      display.drawString(xd2, yd1, (day_ + "." + month_ + ".")); //Date
      display.drawString(xd21, yd2, "---o");
      String time_ = weather_values[forcast_index][13];
      time_ = time_.substring(11, 16);
      display.drawString(xd2, yd3, time_); //Time
      //----------------------------------
    }
    display.display();
  }

  if (y == 7) {

    forcast_index = 3;

    display.clear();
    //----------------------------------text  left
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    String dir = degrees_to_direction(weather_values[forcast_index][7]);
    String bft = ms_to_beaufort(weather_values[forcast_index][6]);
    display.drawString(xd1, yd1, dir + bft);//Wind
    if (display_mode == 0)display.drawString(xd1, yd2, "/// " + weather_values[forcast_index][9] + einheiten[9]); //Rain
    if (display_mode == 1)display.drawString(xd1, yd2, "Hmy " + weather_values[forcast_index][4] + einheiten[4]);//Humidity
    if (display_mode == 2)display.drawString(xd1, yd2, weather_values[forcast_index][5] + einheiten[5]);//Pressure
    String description = weather_values[forcast_index][12];
    int length = description.length();
    if (length > 15) {
      description = description.substring(0, 14);
    }
    display.drawString(xd1, yd3, description );//Description
    //----------------------------------text right
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(xd5, yd1, city_name);//City
    display.setFont(ArialMT_Plain_24);
    display.drawString(xd5, yd11, weather_values[forcast_index][1] + einheiten[1]);//Temp
    //----------------------------------
    display.display();
  }
}
//--------------------------------------------------------------------------
void draw_weather_icon(String icon_id) {

  if (icon_id == "01d") display.drawXbm(xd1, yd1, icon_width, icon_height, meteo_01d_bits);
  if (icon_id == "01n") display.drawXbm(xd1, yd1, icon_width, icon_height, meteo_01n_bits);
  if (icon_id == "02d") display.drawXbm(xd1, yd1, icon_width, icon_height, meteo_02d_bits);
  if (icon_id == "02n") display.drawXbm(xd1, yd1, icon_width, icon_height, meteo_02n_bits);
  if (icon_id == "03d") display.drawXbm(xd1, yd1, icon_width, icon_height, meteo_03d_bits);
  if (icon_id == "03n") display.drawXbm(xd1, yd1, icon_width, icon_height, meteo_03n_bits);
  if (icon_id == "04d") display.drawXbm(xd1, yd1, icon_width, icon_height, meteo_04d_bits);
  if (icon_id == "04n") display.drawXbm(xd1, yd1, icon_width, icon_height, meteo_04n_bits);
  if (icon_id == "09d") display.drawXbm(xd1, yd1, icon_width, icon_height, meteo_09d_bits);
  if (icon_id == "09n") display.drawXbm(xd1, yd1, icon_width, icon_height, meteo_09n_bits);
  if (icon_id == "10d") display.drawXbm(xd1, yd1, icon_width, icon_height, meteo_10d_bits);
  if (icon_id == "10n") display.drawXbm(xd1, yd1, icon_width, icon_height, meteo_10n_bits);
  if (icon_id == "11d") display.drawXbm(xd1, yd1, icon_width, icon_height, meteo_11d_bits);
  if (icon_id == "11n") display.drawXbm(xd1, yd1, icon_width, icon_height, meteo_11n_bits);
  if (icon_id == "13d") display.drawXbm(xd1, yd1, icon_width, icon_height, meteo_13d_bits);
  if (icon_id == "13n") display.drawXbm(xd1, yd1, icon_width, icon_height, meteo_13n_bits);
  if (icon_id == "50d") display.drawXbm(xd1, yd1, icon_width, icon_height, meteo_50d_bits);
  if (icon_id == "50n") display.drawXbm(xd1, yd1, icon_width, icon_height, meteo_50n_bits);
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
void write_eeprom_string(int address, String value) {

  EEPROM.begin(eeprom_size);
  int len = value.length();
  len++;
  if (len < 64) {
    int address_end = address + len;
    char buf[len];
    byte count = 0;
    value.toCharArray(buf, len);
    for (int i = address ; i < address_end ; i++) {
      EEPROM.write(i, buf[count]);
      count++;
    }
  }
  EEPROM.end();
}
//-----------------------------------------------------------------
void write_eeprom_long(int address, long value) {

  EEPROM.begin(eeprom_size);
  byte four = (value & 0xFF);
  byte three = ((value >> 8) & 0xFF);
  byte two = ((value >> 16) & 0xFF);
  byte one = ((value >> 24) & 0xFF);

  EEPROM.write(address, four);
  EEPROM.write(address + 1, three);
  EEPROM.write(address + 2, two);
  EEPROM.write(address + 3, one);
  EEPROM.end();
}
//-----------------------------------------------------------------
void write_eeprom_int(int address, int value) {

  EEPROM.begin(eeprom_size);
  EEPROM.write(address, highByte(value));
  EEPROM.write(address + 1, lowByte(value));
  EEPROM.end();
}
//-----------------------------------------------------------------
void write_eeprom_byte(int address, byte value) {

  EEPROM.begin(eeprom_size);
  //EEPROM.write(address, value);
  EEPROM.write(address, value);
  EEPROM.end();
}
//-----------------------------------------------------------------
void write_eeprom_bool(int address, boolean value) {

  EEPROM.begin(eeprom_size);
  //EEPROM.write(address, value);
  if (value == true)EEPROM.write(address, 1);
  if (value == false)EEPROM.write(address, 0);
  EEPROM.end();
}
//-----------------------------------------------------------------
String read_eeprom_string(int address) {

  EEPROM.begin(eeprom_size);
  String value;
  byte count = 0;
  char buf[64];
  for (int i = address ; i < (address + 63) ; i++) {
    buf[count] = EEPROM.read(i);
    if (buf[count] == 0) break; //endmark of string
    value += buf[count];
    count++;
  }
  EEPROM.end();
  return value;
}
//-----------------------------------------------------------------
long read_eeprom_long(int address) {

  EEPROM.begin(eeprom_size);
  long four = EEPROM.read(address);
  long thre = EEPROM.read(address + 1);
  long two = EEPROM.read(address + 2);
  long one = EEPROM.read(address + 3);
  EEPROM.end();
  return ((four << 0) & 0xFF) + ((thre << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}
//-----------------------------------------------------------------
int read_eeprom_int(int address) {

  EEPROM.begin(eeprom_size);
  int value;
  int value_1;
  value = EEPROM.read(address); //highByte(value));
  value = value << 8;
  value_1 = EEPROM.read(address + 1); //lowByte(value));
  value = value + value_1;
  EEPROM.end();
  return value;
}
//-----------------------------------------------------------------
byte read_eeprom_byte(int address) {

  EEPROM.begin(eeprom_size);
  byte value;
  value = EEPROM.read(address);
  EEPROM.end();
  return value;
}
//-----------------------------------------------------------------
boolean read_eeprom_bool(int address) {

  EEPROM.begin(eeprom_size);
  byte value;
  boolean bool_value;
  value = EEPROM.read(address);
  if (value == 1)bool_value = true;
  if (value != 1)bool_value = false;
  return bool_value;
  EEPROM.end();
}
//--------------------------------------------------------------------------
void load_config() {

  Serial.println();
  Serial.println(F("config load:"));

  debuging = read_eeprom_bool(debuging_eeprom_address);
  Serial.println("debuging=" + String(debuging));

  String value = "";
  value = read_eeprom_string(ssid_eeprom_address);
  strcpy(WIFI_SSID, value.c_str());
  Serial.println("ssid=" + String(WIFI_SSID));

  value = read_eeprom_string(password_eeprom_address);
  strcpy(WIFI_PASSWORD, value.c_str());
  Serial.println("password=" + String(WIFI_PASSWORD));

  OPEN_WEATHER_MAP_LOCATION_ID = read_eeprom_string(location_id_eeprom_address);
  Serial.println("location_id=" + String(OPEN_WEATHER_MAP_LOCATION_ID));

  OPEN_WEATHER_MAP_APP_ID = read_eeprom_string(api_key_eeprom_address);
  Serial.println("api_key=" + String(OPEN_WEATHER_MAP_APP_ID));

  city_name = read_eeprom_string(location_name_eeprom_address);
  Serial.println("location_name=" + String(city_name));

  flip_display = read_eeprom_bool(flip_display_eeprom_address);
  Serial.println("flip_display=" + String(flip_display));

  invert_display = read_eeprom_bool(invert_display_eeprom_address);
  Serial.println("invert_display=" + String(invert_display));

  display_mode = read_eeprom_int(display_mode_eeprom_address);
  Serial.println("display_mode=" + String(display_mode));

  requ_hour_1 = read_eeprom_int(requ_hour_1_eeprom_address);
  Serial.println("requ_hour_1=" + String(requ_hour_1));

  requ_hour_2 = read_eeprom_int(requ_hour_2_eeprom_address);
  Serial.println("requ_hour_2=" + String(requ_hour_2));
}
//--------------------------------------------------------------------------
void lookup_commands() {

  int length_ = serial_line_0.length();
  length_ -= 1;

  if (serial_line_0.substring(0, 5) == F("ssid=")) {
    write_eeprom_string(ssid_eeprom_address, serial_line_0.substring(5, length_));
    Serial.println(serial_line_0.substring(0, 5) + serial_line_0.substring(5, length_));
    load_config();
  }

  if (serial_line_0.substring(0, 9) == F("password=")) {
    write_eeprom_string(password_eeprom_address, serial_line_0.substring(9, length_));
    Serial.println(serial_line_0.substring(0, 9) + serial_line_0.substring(9, length_));
    load_config();
  }

  if (serial_line_0.substring(0, 12) == F("location_id=")) {
    write_eeprom_string(location_id_eeprom_address, serial_line_0.substring(12, length_));
    Serial.println(serial_line_0.substring(0, 12) + serial_line_0.substring(12, length_));
    load_config();
  }

  if (serial_line_0.substring(0, 14) == F("location_name=")) {
    write_eeprom_string(location_name_eeprom_address, serial_line_0.substring(14, length_));
    Serial.println(serial_line_0.substring(0, 14) + serial_line_0.substring(14, length_));
    load_config();
  }

  if (serial_line_0.substring(0, 8) == F("api_key=")) {
    write_eeprom_string(api_key_eeprom_address, serial_line_0.substring(8, length_));
    Serial.println(serial_line_0.substring(0, 8) + serial_line_0.substring(8, length_));
    load_config();
  }

  if (serial_line_0.substring(0, 9) == F("debuging=")) {
    if (serial_line_0.substring(9, length_) == "false") {
      write_eeprom_bool(debuging_eeprom_address, false);
      Serial.println(serial_line_0.substring(0, 9) + serial_line_0.substring(9, length_));
      load_config();
    }
    if (serial_line_0.substring(9, length_) == "true") {
      write_eeprom_bool(debuging_eeprom_address, true);
      Serial.println(serial_line_0.substring(0, 9) + serial_line_0.substring(9, length_));
      load_config();
    }
  }

  if (serial_line_0.substring(0, 7) == F("ip -get")) {
    Serial.print(F("ip="));
    Serial.println(WiFi.localIP());
  }

  if (serial_line_0.substring(0, 11) == F("config -get")) {
    load_config();
  }

  if (serial_line_0.substring(0, 13) == F("flip_display=")) {
    if (serial_line_0.substring(13, length_) == "false") {
      write_eeprom_bool(flip_display_eeprom_address, false);
      Serial.println(serial_line_0.substring(0, 13) + serial_line_0.substring(13, length_));
      load_config();
    }
    if (serial_line_0.substring(13, length_) == "true") {
      write_eeprom_bool(flip_display_eeprom_address, true);
      Serial.println(serial_line_0.substring(0, 13) + serial_line_0.substring(13, length_));
      load_config();
    }
  }

  if (serial_line_0.substring(0, 15) == F("invert_display=")) {
    if (serial_line_0.substring(15, length_) == "false") {
      write_eeprom_bool(invert_display_eeprom_address, false);
      Serial.println(serial_line_0.substring(0, 15) + serial_line_0.substring(15, length_));
      load_config();
    }
    if (serial_line_0.substring(15, length_) == "true") {
      write_eeprom_bool(invert_display_eeprom_address, true);
      Serial.println(serial_line_0.substring(0, 15) + serial_line_0.substring(15, length_));
      load_config();
    }
  }

  if (serial_line_0.substring(0, 13) == F("display_mode=")) {
    if (serial_line_0.substring(13, length_) == "0") {
      write_eeprom_int(display_mode_eeprom_address, 0);
      Serial.println(serial_line_0.substring(0, 13) + serial_line_0.substring(13, length_));
      load_config();
    }
    if (serial_line_0.substring(13, length_) == "1") {
      write_eeprom_int(display_mode_eeprom_address, 1);
      Serial.println(serial_line_0.substring(0, 13) + serial_line_0.substring(13, length_));
      load_config();
    }
    if (serial_line_0.substring(13, length_) == "2") {
      write_eeprom_int(display_mode_eeprom_address, 2);
      Serial.println(serial_line_0.substring(0, 13) + serial_line_0.substring(13, length_));
      load_config();
    }
  }

  if (serial_line_0.substring(0, 12) == F("requ_hour_1=")) {
    String value = serial_line_0.substring(12, length_);
    int value_int = value.toInt();
    if (value_int >= 0 && value_int <= 23) {
      write_eeprom_int(requ_hour_1_eeprom_address, value_int);
      Serial.println(serial_line_0.substring(0, 12) + serial_line_0.substring(12, length_));
      load_config();
    }
  }

  if (serial_line_0.substring(0, 12) == F("requ_hour_2=")) {
    String value = serial_line_0.substring(12, length_);
    int value_int = value.toInt();
    if (value_int >= 0 && value_int <= 23) {
      write_eeprom_int(requ_hour_2_eeprom_address, value_int);
      Serial.println(serial_line_0.substring(0, 12) + serial_line_0.substring(12, length_));
      load_config();
    }
  }
}
//--------------------------------------------------------------------------
void read_serial_port_0() {

  if (Serial.available() > 0) {
    serial_line_0 = Serial.readStringUntil('\n');
    //Serial.println(serial_line_0);
    lookup_commands();
  }
}
//-----------------------------------------------------------------

