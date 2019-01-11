#include <Arduino.h>

/*********
  Async Web server taken from:
  Rui Santos
  Complete project details at https://randomnerdtutorials.com
*********/
/*   Set the local date and time by entering the following on the
   serial monitor:
      year,month,day,hour,minute,second,
 *                                                                      *
   Where
      year can be two or four digits,
      month is 1-12,
      day is 1-31,
      hour is 0-23, and
      minute and second are 0-59.
 *
 *                                                                      *
   Entering the final comma delimiter (after "second") will avoid a
   one-second timeout and will allow the RTC to be set more accurately.
 *                                                                      *
   Validity checking is done, invalid values or incomplete syntax
   in the input will not result in an incorrect RTC setting.*/

// Import required libraries
#include <Streaming.h>
#include <Time.h>
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include <U8g2lib.h>
#include <DHT.h>
#include "RTClib.h"
#include <Timezone.h>         //https://github.com/JChristensen/Timezone

// Central Europe Time Zone (Rome)
TimeChangeRule myCEST = {"CEST", Last, Sun, Mar, 2, +120};    //Daylight time = UTC +2 hours
TimeChangeRule myCET = {"CET", Last, Sun, Oct, 3, +60};     //Standard time = UTC +1 hours
Timezone myTZ(myCEST, myCET);
TimeChangeRule *tcr;        //pointer to the time change rule, use to get TZ abbrev


#define MINTEMP 15 //minimum temperature that can be setup
#define MAXTEMP 27 //maximum temperature that can be setup
#define NOFROSTTEMP 17 //no frost temperature
#define BOUNCE_DURATION 500   // define an appropriate bounce time in ms for switches

// Replace with your network credentials
//const char* ssid = "TIM-19296700";
//const char* password = "canediana5432112345canediana";
const char* ssid = "wifi";
const char* password = "phonix2014";


// Set Rele pin
const int relePin = 2;
const int manPin = 32;
const int incPin = 26;
const int decPin = 25;

boolean running = false; // heating system running flag

String modestr;

volatile int displaytype = 3;

float humidity;
float temperature;

volatile int mode = 1; //initial mode
volatile int previousmode; //initial mode

volatile int setuptemp;

volatile time_t t;
volatile time_t tLast;
volatile time_t tLast2;
volatile time_t LastChanged;
volatile unsigned long bounceTime2 = 0;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ 22, /* data=*/ 21);   // ESP32 Thing, HW I2C with pin remapping

// Create AsyncWebServer object on port 80
AsyncWebServer server(8088);

// Replaces placeholder with LED state value
String processor(const String& var){
    Serial.println(var);
    if(var == "STATE"){
      switch (mode){
      case 1:
        modestr = "Normale";
      break;
      case 2:
        modestr = "Antigelo";
      break;
      case 3:
        modestr = "Jolly";
      break;
      case 4:
        modestr = "Manuale";
      break;
    }
    Serial.println(modestr);
    return modestr;
  }
  if(var=="TEMP"){
    return String(temperature);
  }
  if(var=="SETUP"){
    return String(setuptemp);
  }
  if(var=="RELE"){
    if(running){
      return String("Accesa");
    }
    else{
      return String("Spenta");
    };
  }
  return String();
}

DHT dht;

RTC_DS1307 rtc;

//Print an integer in "00" format (with leading zero),
//followed by a delimiter character to Serial.
//Input value assumed to be between 0 and 99.
void printI00(int val, char delim)
{
  if (val < 10) Serial << F("0");
  Serial << _DEC(val);
  if (delim > 0) Serial << delim;
  return;
}
//print time to Serial
void printTime(time_t t)
{
  printI00(hour(t), ':');
  printI00(minute(t), ':');
  printI00(second(t), ' ');
}

//print date to Serial
void printDate(time_t t)
{
  printI00(day(t), 0);
  Serial << F(" ") << monthShortStr(month(t)) << F(" ") << _DEC(year(t));
}

//print date and time to Serial
void printDateTime(time_t t)
{
  printDate(t);
  Serial << F(" ");
  printTime(t);
}

//Parse serial input in order to set time
void parseserial(){
  tmElements_t tm;
  //note that the tmElements_t Year member is an offset from 1970,
  //but the RTC wants the last two digits of the calendar year.
  //use the convenience macros from Time.h to do the conversions.
  int y = Serial.parseInt();
  if (y >= 100 && y < 1000)
    Serial << F("Error: Year must be two digits or four digits!") << endl;
  else {
    if (y >= 1000)
      tm.Year = CalendarYrToTm(y);
    else    //(y < 100)
      tm.Year = y2kYearToTm(y);
      tm.Month = Serial.parseInt();
      tm.Day = Serial.parseInt();
      tm.Hour = Serial.parseInt();
      tm.Minute = Serial.parseInt();
      tm.Second = Serial.parseInt();
    if ( (tm.Month >= 1 && tm.Month <= 12) &&
         (tm.Day >= 1  && tm.Day <= 31) &&
         (tm.Hour >= 0 && tm.Hour <= 24) &&
         (tm.Minute >= 0 && tm.Minute <= 59) &&
         (tm.Second >= 0 && tm.Second <= 59)) {
      time_t t_local = makeTime(tm);
      t=myTZ.toUTC(t_local);
      setTime(t);
      rtc.adjust(DateTime(year(t), month(t), day(t), hour(t), minute(t), second(t)));
      DateTime now = rtc.now();

      Serial.print(now.year(), DEC);
      Serial.print('/');
      Serial.print(now.month(), DEC);
      Serial.print('/');
      Serial.print(now.day(), DEC);
      Serial.print(" (");
      Serial.print(now.dayOfTheWeek(), DEC);
      Serial.print(") ");
      Serial.print(now.hour(), DEC);
      Serial.print(':');
      Serial.print(now.minute(), DEC);
      Serial.print(':');
      Serial.print(now.second(), DEC);
      Serial.println();
      Serial << F("Time set to: UTC");
      printDateTime(t);
      Serial << endl;
    }
    else {
      Serial << F("Invalid Date/Time: ");
      Serial << y << F(" ");
      Serial << tm.Month << F(" ");
      Serial << tm.Day << F(" ");
      Serial << tm.Hour << F(" ");
      Serial << tm.Minute << F(" ");
      Serial << tm.Second << endl;
    }
    //dump any extraneous input
    while (Serial.available() > 0) Serial.read();
  }

}


const int weektemp[7][24] =
{ //00   01   02   03   04   05   06   07   08   09   10   11   12   13   14   15   16   17   18   19   20   21   22   23
  { 200, 200, 200, 200, 200, 200, 210, 210, 220, 220, 230, 230, 230, 220, 220, 220, 210, 220, 220, 230, 230, 230, 200, 200}, //sun
  { 200, 200, 200, 200, 205, 210, 220, 220, 220, 215, 205, 205, 205, 210, 210, 215, 215, 220, 220, 230, 230, 230, 200, 200}, //mon
  { 200, 200, 200, 200, 205, 210, 220, 220, 200, 200, 200, 200, 200, 200, 200, 205, 210, 220, 220, 230, 230, 230, 200, 200}, //tue
  { 200, 200, 200, 200, 205, 210, 220, 220, 220, 215, 205, 205, 205, 210, 210, 215, 215, 220, 220, 230, 230, 230, 200, 200}, //wed
  { 200, 200, 200, 200, 205, 210, 220, 220, 220, 215, 205, 205, 205, 210, 210, 215, 215, 220, 220, 230, 230, 230, 200, 200}, //thu
  { 200, 200, 200, 200, 205, 210, 220, 220, 220, 215, 205, 205, 205, 210, 210, 215, 215, 220, 220, 230, 230, 230, 200, 200}, //fri
  { 200, 200, 200, 200, 200, 200, 210, 210, 220, 220, 230, 230, 230, 220, 220, 220, 210, 220, 220, 230, 230, 230, 200, 200}  //sat
};

const int jollytemp[24] =
  //00   01   02   03   04   05   06   07   08   09   10   11   12   13   14   15   16   17   18   19   20   21   22   23
  { 200, 200, 200, 200, 200, 210, 220, 220, 230, 230, 230, 230, 230, 230, 210, 210, 210, 220, 220, 230, 230, 230, 200, 200};

void mantemp()
  {
    if (abs(millis() - bounceTime2) > BOUNCE_DURATION)
    {
      //Serial << endl << F("mantemp") << endl ;
      if(displaytype == 3)
      {
        mode = mode +1;
        if (mode > 3){ mode = 1;}
      } else {
        displaytype = 3;
      }
      bounceTime2 = millis();
    }
  }

  void inctemp()
  {
    if (abs(millis() - bounceTime2) > BOUNCE_DURATION)
    {
      //Serial << endl << F("inctemp") << endl ;
      if (displaytype == 2)
      {
        if(mode!=4){
          previousmode = mode; //save the current mode
        }
        mode=4;
        setuptemp = constrain(setuptemp + 1, MINTEMP, MAXTEMP);
        displaytype = 2; //Show setup temp
        LastChanged = t;
      }
      else if(displaytype == 1)
      {
        displaytype = 3; //Show time and current temp
      }
      else if(displaytype == 3)
      {
        displaytype = 2; //Show time and current temp
      }
      bounceTime2 = millis();
    }
  }

  void dectemp()
  {
    if (abs(millis() - bounceTime2) > BOUNCE_DURATION)
    {
      //Serial << endl << F("dectemp") << endl ;
      if (displaytype == 2)
      {
        if(mode!=4){
          previousmode = mode; //save the current mode
        }
        mode=4;
        setuptemp = constrain(setuptemp - 1, MINTEMP, MAXTEMP);
        displaytype = 2; //Show setup temp
        LastChanged = t;
      }
      else if(displaytype == 1)
      {
        displaytype = 3; //Show time and current temp
      }
      else if(displaytype == 3)
      {
        displaytype = 2; //Show time and current temp
      }
      bounceTime2 = millis();
    }
  }

  void display3() // show temp, time and mode
  {
    int val;
    //print on the OLED
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_inb16_mr);
//    u8g2.setCursor(30,20);
    u8g2.setCursor(50,20);
    u8g2.print(temperature);  // write something to the internal memory
//    u8g2.setCursor(30,50);
    u8g2.setCursor(50,60);
    val = hour(t);
    if(val<10){u8g2.print("0");}
    u8g2.print(val);  // write something to the internal memory
    u8g2.print(":");
    val = minute(t);
    if(val<10){u8g2.print("0");}
    u8g2.print(val);
    //u8g2.sendBuffer();          // transfer internal memory to the display
//if WiFi.localIP()
    switch (mode){
      case 1: // Normale
        u8g2.setCursor(0,20);
        u8g2.setFont(u8g2_font_inb16_mr);
        u8g2.print("N");
        u8g2.setFont(u8g2_font_ncenB10_tr);
        u8g2.setCursor(14,20);
        u8g2.print("orm");
      break;
      case 2: // No frost
        u8g2.setCursor(0,20);
        u8g2.setFont(u8g2_font_inb16_mr);
        u8g2.print("S");
        u8g2.setFont(u8g2_font_ncenB10_tr);
        u8g2.setCursor(14,20);
        u8g2.print("top");
      break;
      case 3: // Jolly
        u8g2.setCursor(0,20);
        u8g2.setFont(u8g2_font_inb16_mr);
        u8g2.print("J");
        u8g2.setFont(u8g2_font_ncenB10_tr);
        u8g2.setCursor(14,20);
        u8g2.print("olly");
      break;
      case 4: // Manuale
        u8g2.setCursor(0,20);
        u8g2.setFont(u8g2_font_inb16_mr);
        u8g2.print("M");
        u8g2.setFont(u8g2_font_ncenB10_tr);
        u8g2.setCursor(14,20);
        u8g2.print("an");
      break;
    }
    if(running){
//      u8g2.setCursor(0,60);
//      u8g2.setFont(u8g2_font_ncenB24_tr);
//      u8g2.print("*");
      u8g2.setCursor(0,60);
      u8g2.setFont(u8g2_font_ncenB24_tr);
      u8g2.print("*");
    }
    if(WiFi.status() == WL_CONNECTED)
    {
//      u8g2.setCursor(30,60);
      u8g2.setCursor(0,30);
//      u8g2.setFont(u8g2_font_ncenB10_tr);
      u8g2.setFont(u8g2_font_ncenR08_te);
      u8g2.print("wifi");
    }
    u8g2.sendBuffer();
  }

  void display2() //shows setup temp only
  {
    //print on the OLED
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_inb16_mr);
//    u8g2.setCursor(40,40);
    u8g2.setCursor(50,40);
    u8g2.print(setuptemp);  // write something to the internal memory
    u8g2.sendBuffer();          // transfer internal memory to the display
  // print mode flag
  }

  void display1()  // void
  {
    //print on the OLED
    u8g2.clearBuffer();
    u8g2.sendBuffer();          // transfer internal memory to the display
  }

//-------------------------------------------------
void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);
  pinMode(relePin, OUTPUT);
  pinMode(manPin, INPUT_PULLUP);
  pinMode(incPin, INPUT_PULLUP);
  pinMode(decPin, INPUT_PULLUP);
  // setup interrupt handlers
  attachInterrupt(digitalPinToInterrupt(manPin), mantemp, FALLING);
  attachInterrupt(digitalPinToInterrupt(incPin), inctemp, FALLING);
  attachInterrupt(digitalPinToInterrupt(decPin), dectemp, FALLING);

  u8g2.begin();

  dht.setup(4); // data pin 4
  temperature = dht.getTemperature();




  // Initialize SPIFFS
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // Connect to Wi-Fi
  int k=0;
  WiFi.begin(ssid, password);
  while ((WiFi.status() != WL_CONNECTED) && (k<=10)) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
    k++;
  }

  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());
  // show IP on the OLED
  u8g2.clearBuffer();         // clear the internal memory
  u8g2.setFont(u8g2_font_ncenB12_tr); // choose a suitable font
  //u8g2.drawStr(0,10,"Hello World!");
  u8g2.setCursor(10,40);
  u8g2.print(WiFi.localIP().toString());  // write something to the internal memory
  u8g2.sendBuffer();          // transfer internal memory to the display
  delay(5000);
  u8g2.clearBuffer();
  u8g2.sendBuffer();
  display3();


  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }
  tmElements_t tm;
  DateTime now1 = rtc.now();
  /*Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" (");
  Serial.print(now.dayOfTheWeek(), DEC);
  Serial.print(") ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(now.second(), DEC);
  Serial.print(':');
  Serial.println();*/
  tm.Year = CalendarYrToTm(now1.year());
  tm.Month = now1.month();
  tm.Day = now1.day();
  tm.Hour = now1.hour();
  tm.Minute = now1.minute();
  tm.Second = now1.second();
  t = makeTime(tm);
  setTime(t);
  Serial << F("Time set from RTC to: UTC ");
  printDateTime(t);
  Serial << endl;



  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
  });

  server.on("/plus", HTTP_GET, [](AsyncWebServerRequest *request){
    setuptemp = setuptemp  + 1;
    mode=4;
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  server.on("/minus", HTTP_GET, [](AsyncWebServerRequest *request){
    setuptemp = setuptemp  - 1;
    mode=4;
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  // Route to set mode normale
  server.on("/on1", HTTP_GET, [](AsyncWebServerRequest *request){
    mode=1;
    setuptemp = weektemp[weekday(t) - 1][hour(t)] / 10;
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  // Route to set mode nofrost
  server.on("/on2", HTTP_GET, [](AsyncWebServerRequest *request){
    mode=2;

    setuptemp = NOFROSTTEMP;
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  // Route to set mode JOLLY
  server.on("/on3", HTTP_GET, [](AsyncWebServerRequest *request){
    mode=3;
    setuptemp = jollytemp[hour(t)] / 10;
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  // Start server
  server.begin();

  /* show the temperature time table on the serial line */

  int i;
  int j;
  for (i = 0; i < 7; i++) {
    for (j = 0; j < 24; j++) {
      setuptemp = weektemp[i][j] / 10;
      Serial << setuptemp << F(" ");
    }
    Serial << endl;
  }
  time_t utc_t;
  utc_t = now();
  t = myTZ.toLocal(utc_t, &tcr);
  tLast2 = t;
  tLast = t;
}

//----------------------------------------------------------
void loop()
{

  // Connect to Wi-Fi
  /*if((t-tLast >=120) && (WiFi.status() != WL_CONNECTED))*/
  /*if(t-tLast >=120)
  {
    tLast = t;/*
    int k=0;
    WiFi.begin(ssid, password);
    while ((WiFi.status() != WL_CONNECTED) && (k<=10)) {
      delay(1000);
      Serial.println("Retrying WiFi..");
      k++;
    }
    if(WiFi.status() == WL_CONNECTED)
    {
        // Print ESP32 Local IP Address
        Serial.println(WiFi.localIP());
        // show IP on the OLED
      /*  u8g2.clearBuffer();         // clear the internal memory
        u8g2.setFont(u8g2_font_ncenB12_tr); // choose a suitable font
        //u8g2.drawStr(0,10,"Hello World!");
        u8g2.setCursor(10,40);
        u8g2.print(WiFi.localIP().toString());  // write something to the internal memory
        u8g2.sendBuffer();          // transfer internal memory to the display
        delay(5000);
        u8g2.clearBuffer();
        u8g2.sendBuffer();
        display3();
    }
  }*/
  time_t utc_t;
  utc_t = now();
  t = myTZ.toLocal(utc_t, &tcr);
  if ((t - LastChanged) >= 3600) {
    if(mode == 4){
      mode=previousmode; //reset
    }
  }

  switch (mode){
    case 1: // Normal
      setuptemp = weektemp[weekday(t) - 1][hour(t)] / 10;
    break;
    case 2: // No frost
      setuptemp = NOFROSTTEMP;
    break;
    case 3: // Jolly
      setuptemp = jollytemp[hour(t)] / 10;
    break;
    case 4: // Manual
    break;
  }
  setuptemp = constrain(setuptemp, MINTEMP, MAXTEMP);

  if ((t - tLast2) >= 60) { //  check the temperature every minute
    temperature = dht.getTemperature();
    Serial << F(" Day: ") << weekday(t) << F(" Hour: ") << hour(t) << F(" Set up Temperature ") << setuptemp << F(" Temp:") << temperature << endl;
    if ((setuptemp > temperature) && !running) {
      //switch on heating system
      digitalWrite(relePin, HIGH);
      running = true;
    }
    else {
      if ((setuptemp <= temperature) && running) {
        //switch off heating system
        digitalWrite(relePin, LOW);
        running = false;
      }
    }
    tLast2 = t;
    displaytype = 1;
  }
  // check if a date/time setting is coming from serial
  if (Serial.available() >= 12) {
      parseserial();
  }

  switch (displaytype) {
    case 1:
      display1();
    break;
    case 2:
      display2();
    break;
    case 3:
      display3();
    break;
  }

}
