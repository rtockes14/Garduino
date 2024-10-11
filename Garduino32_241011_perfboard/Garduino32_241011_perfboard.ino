#include <Arduino.h>
#include <U8g2lib.h>
#include "uRTCLib.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "time.h"
#include "sntp.h"
#include "FS.h"
#include "SD.h"
#include "DHT.h"


#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif

//#define SPI_MOSI  23
//#define SPI_MISO 19
//#define SPI_SCK 18
//#define SD_CS  5


// define pins
#define BLUE 27
#define RED 25
#define GREEN 26

#define BUZZER 5

#define PUMP 4 //

#define Relay_1 14 // V
#define Relay_2 27
#define Relay_3 33
#define Relay_4 2 // ^

#define CLK 21
#define DT 22
#define SW 13

#define sensorPin 35
#define sensorPin2 34
#define sensorPin3 16
#define DHTTYPE DHT22




/* ======================================================================================================


  Notes during Programming & TO-DO:

  1. Add state to remove relays triggering on each loop
  2. Calibrate wet / dry references and update map function
  3. Ensure on auto trigger there is a timeout so it doesn't keep watering until sensor catches up.  Must wait at least a day for auto trigger
  4. Add timeout to switch back to Passive state(green led)
  5. Average readings to reduce jumpy values
  6. Incorporate scheduling
  7. Switch for Modes / auto-trigger + Schedule + manual, auto-trigger + manual, Schedule + manual



 ======================================================================================================
*/

 //------- Wifi Creds --------
const char* ssid       = "GoBears-2.4";
const char* password   = "Marlin22#";


const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long  gmtOffset_sec = -21600;
const int   daylightOffset_sec = 3600;

const int dry = 595;
const int wet = 239;

//const char* time_zone = "CET-1CEST,M3.5.0,M10.5.0/3";  // TimeZone rule for Europe/Rome including daylight adjustment rules (optional)

// Oled Setup
U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 15, /* dc=*/ 32, /* reset=*/ 4); 

//Audio audio;

struct plantNumber
{
  //char name[12];
  String name;
  int DayNumber;
  int hour;
  int minute;
  int mL;
  bool wateredToday;
  int plantState;       // 1. Dry   2. Fair   3. Good   4. Wet
};

const int BOX_HEIGHT = 15;
const int BOX_WIDTH = 62;

unsigned long lastButtonPress = 0;   
int currentStateCLK;
int lastStateCLK;       
int btnState = HIGH;   
int lastbtnState;     
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
int diffState = false;

byte k = 0;
// define color mode
int mode = 0;

int selector = 0;
int rotPos = 0;

bool menuState = false;
int startState = 0;
int counter = 42;
int counter2 = 42;
float temperatureVal = 69;            // TODO -- fix this later
bool wateringState = false;
bool manualTrigger = false;
bool running = false;
bool flashing = false;
bool initSetup = false;
char buffer[18];

float moisture = 0;
float moisture2 = 0;

unsigned long currentMillis = 0;
unsigned long animationPreviousMillis = 0;
unsigned long readingPreviousMillis = 0;
unsigned long postPreviousMillis = 0;
unsigned long postInfoPreviousMillis = 0;
unsigned long menuPreviousMillis = 0;

const int animationDelay = 200;
const int readingDelay = 3000;
const int wateringDelay1 = 5000;
const int wateringDelay2 = 2000;
const int menuDelay = 20000;
const int sendDataDelay = 5000;

String serverName = "https://bluegarden-6f0bc98539d7.herokuapp.com/api/plants";
String serverSchedule = "https://bluegarden-6f0bc98539d7.herokuapp.com/api/device_info";

String DEVICE_ID = "2341";

struct tm timeinfo;

DHT dht(sensorPin3, DHTTYPE);

char DaysoftheWeek[8][5] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun", "N/A"};
char daysOfTheWeek[8][12] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday", "N/A"};

plantNumber myPlant1 = {"1:", 7, 0, 0, 0, false, 3};
plantNumber myPlant2 = {"2:", 7, 0, 0, 0, false, 3};

// ====================================================================================================================================

//                      FUNCTIONS 

// ====================================================================================================================================

void printLocalTime(void)
{
  if(!getLocalTime(&timeinfo)){
    Serial.println("No time available (yet)");
    return;
  }
  tm* myTime = &timeinfo;

  time_t rawtime;
  struct tm * timeinfo;
  char buffer[15];

  time(&rawtime);
  timeinfo = localtime (&rawtime);

  //strftime (buffer, 80, "%I:%M%p", timeinfo);
  strftime (buffer, 15, "%H%M%S", timeinfo);
  puts(buffer);

  int val = atoi(buffer);
  printf("This %d simple \t", val);

  // if the time is between 11:59:53PM and 6secs later(for readtime) reset watered today bool check
  if (val >= 235953 && val <= 235959)
  {
    myPlant1.wateredToday = false;
    myPlant2.wateredToday = false;
  }

  //Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  Serial.println(myTime, "%H:%M \n");
}


// Callback function (get's called when time adjusts via NTP)
void timeavailable(struct timeval *t)
{
  Serial.println("Got time adjustment from NTP!");
  printLocalTime();
}


void colorSelect(char color){

  switch (color) {
    case 'r':
      analogWrite(RED, 0);
      analogWrite(GREEN, 255);
      analogWrite(BLUE, 255);
      break;
    case 'g':
      analogWrite(RED, 255);
      analogWrite(GREEN, 0);
      analogWrite(BLUE, 255);
      break;
    case 'b':
      analogWrite(RED, 255);
      analogWrite(GREEN, 255);
      analogWrite(BLUE, 0);
      break;
    case 'w':
      analogWrite(RED, 0);
      analogWrite(GREEN, 0);
      analogWrite(BLUE, 0);
      break;
    case 'o':
      analogWrite(RED, 255);
      analogWrite(GREEN, 255);
      analogWrite(BLUE, 255);
      break;
    default :
      analogWrite(RED, 255);
      analogWrite(GREEN, 255);
      analogWrite(BLUE, 255);
  }
}


void u8g2_prepare(void)
{
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);
}

void buzz()
{
  digitalWrite(BUZZER, HIGH);
  delay(250);
  digitalWrite(BUZZER, LOW);
}

void update() 
{

 	// Read the current state of CLK
	currentStateCLK = digitalRead(CLK);

  // If there is a minimal movement of 1 step
  if ((currentStateCLK != lastStateCLK) && (currentStateCLK == 1)) {
    
    if (digitalRead(DT) != currentStateCLK) {      // If Pin B is HIGH
      Serial.println("Right");             // Print on screen
      rotPos++;
    } else {
      Serial.println("Left");            // Print on screen
      rotPos--;
    }
    
    Serial.print("Direction: ");
		Serial.print(rotPos);
		Serial.print(" | Counter: ");
		Serial.println(rotPos);
  }
  
  lastStateCLK = currentStateCLK;  
}

void u8g2_lastWater(void)
{
  u8g2_prepare();
  if(menuState == 0)
  {
    u8g2.setFontMode(1);
    u8g2.setDrawColor(1);
    u8g2.drawStr(41, 5, DaysoftheWeek[myPlant1.DayNumber]);
    u8g2.drawStr(103, 5, DaysoftheWeek[myPlant2.DayNumber]);
  }
  else if(menuState == 1)
  {
    if(selector == 1)
    {
    u8g2.setFontMode(1);
    u8g2.setDrawColor(1);
    u8g2.drawStr(92, 5, DaysoftheWeek[myPlant2.DayNumber]);
    }
    else if(selector == 2)
    {
      u8g2.setFontMode(1);
      u8g2.setDrawColor(1);
      u8g2.drawStr(92, 5, DaysoftheWeek[myPlant2.DayNumber]);
    }
  }
}
/*void check_schedule(int hours, int minutes)    // ================ NEEDS WORK
{
  // CHANGE THIS LATER
  if(hours == 12 && minutes == 30)
  {
    wateringState = true;
  }
*/

void waterStart(void)
{
  if(running == true)
  {
    //return;
  }
  if(wateringState == true && selector == 1)
  {
    // Low is active
    buzz();
    digitalWrite(Relay_1, LOW);
    digitalWrite(Relay_2, LOW);
    digitalWrite(Relay_3, LOW);
    digitalWrite(Relay_4, LOW);
  }
  else if (wateringState == true && selector == 2) 
  {
    buzz();
    digitalWrite(Relay_1, LOW);
    digitalWrite(Relay_2, HIGH);
    digitalWrite(Relay_3, LOW);
    digitalWrite(Relay_4, HIGH);
  }
  else 
  {
    digitalWrite(Relay_1, HIGH);
    digitalWrite(Relay_2, LOW);
    digitalWrite(Relay_3, LOW);
    digitalWrite(Relay_4, HIGH);
    
  }
}

void waterEnd(void)
{
  digitalWrite(Relay_1, HIGH);
  digitalWrite(Relay_2, HIGH);
  digitalWrite(Relay_3, HIGH);
  digitalWrite(Relay_4, HIGH);

}

void checkPlantState(void)  //=============================================
{
  if(myPlant1.plantState == 1)
  {
    selector = 1;
    menuState = true;
    wateringState = true;
    if(!myPlant1.wateredToday || manualTrigger)
    {
      waterStart();
      delay(wateringDelay1);
      wateringState = false;
      myPlant1.plantState = 2;
      waterEnd();
      myPlant1.wateredToday = true;
      manualTrigger = false;
    }

  }
    if(myPlant2.plantState == 1)
  {
    selector = 2;
    menuState = true;
    wateringState = true;
    if(!myPlant2.wateredToday || manualTrigger)
    {
      waterStart();
      delay(wateringDelay2);
      wateringState = false;
      myPlant2.plantState = 2;
      waterEnd();
      myPlant2.wateredToday = true;
      manualTrigger = false;
    }
  }
}

void checkMenu(void)
{
  if(rotPos == 2 && menuState == false)
  {
    selector++;
  }
  else if(rotPos == 0 && menuState == false)
  {
    selector--;
  }
  else if(rotPos == 2 && menuState == true)
  {
    startState++;
    if(startState > 1)
    {
      startState = 0;
    }
  }
  else if(rotPos == 0 && menuState == true)
  {
    menuState = 0;
  }
  else if(diffState == true && menuState == true && startState == 1)
  {
    startState = 2;
    delay(200);
  }

  else if((diffState == true && selector == 1) || (diffState == true && selector == 2))
  {
    if(menuState == true)
    {
      menuState = 0;
    }
    else
    {
      menuState = 1;
    }
    delay(200);
  }

  if(selector > 3)
    {
      selector = 1;
    }
  else if(selector < 0)
  {
    selector = 0;
  }

  rotPos = 1;
}

void multibox(void) 
{
  u8g2_prepare();
  animation(k);
  colorSelect('g');
  u8g2.drawRFrame(1, 1, 62, 62, 3);
  u8g2.drawRFrame(65, 1, 62, 62, 3);
  //u8g2.setDrawColor(2);
  if (selector == 1)
  {
    u8g2.drawRBox(1, 48, BOX_WIDTH, BOX_HEIGHT, 3);
    u8g2.drawRFrame(65, 48, BOX_WIDTH, BOX_HEIGHT, 3);
  }
  else if(selector == 2)
  {
    u8g2.drawRFrame(1, 48, BOX_WIDTH, BOX_HEIGHT, 3);
    u8g2.drawRBox(65, 48, BOX_WIDTH, BOX_HEIGHT, 3);
  }
  else
  {
    u8g2.drawRFrame(1, 48, BOX_WIDTH, BOX_HEIGHT, 3);
    u8g2.drawRFrame(65, 48, BOX_WIDTH, BOX_HEIGHT, 3);
  }

  if(myPlant1.plantState == 1)
    {
      u8g2.drawStr(35, 25, "DRY");
    }
    else if(myPlant1.plantState == 2)
    {
      u8g2.drawStr(34, 25, "FAIR");
    }
    else if(myPlant1.plantState == 4)
    {
      u8g2.drawStr(35, 25, "WET");
    }
    else
    {
      u8g2.drawStr(32, 25, "GOOD");
    }

    if(myPlant2.plantState == 1)
    {
      u8g2.drawStr(98, 25, "DRY");
    }
    else if(myPlant2.plantState == 2)
    {
      u8g2.drawStr(97, 25, "FAIR");
    }
    else if(myPlant2.plantState == 4)
    {
      u8g2.drawStr(98, 25, "WET");
    }
    else
    {
      u8g2.drawStr(95, 25, "GOOD");
    }


  u8g2.setFontMode(2);
  u8g2.setDrawColor(2);
  u8g2.drawStr(4, 50, myPlant1.name.c_str());
  u8g2.drawStr(68, 50, myPlant2.name.c_str());

  u8g2_lastWater();

}

void singleBoxBig(int counter, byte k)
{

  //myPlant1.mL = 44;
  //myPlant2.mL = 70;

  int hour = timeinfo.tm_hour;
  int minutes = timeinfo.tm_min;
  //int day = timeinfo.tm_wday;
  char* wday = DaysoftheWeek[timeinfo.tm_wday];
  char mL_string1[5]; 
  char mL_string2[5];

  itoa(myPlant1.mL, mL_string1, 10);
  itoa(myPlant2.mL, mL_string2, 10);

  u8g2_prepare();
  animation(k);
  u8g2.drawRFrame(1, 1, 126, 62, 3);
  u8g2.drawRBox(1, 48, 126, BOX_HEIGHT, 3);
  if(minutes < 10)
  {
    sprintf(buffer, "%s %d:0%d", wday, hour, minutes);
  }
  else
  {
    sprintf(buffer, "%s %d:%d", wday, hour, minutes);
  }

 //sprintf(moisture_buffer, "%.0f", moisture1);

  if(selector == 1)
  {
    u8g2.setFontMode(2);
    u8g2.setDrawColor(2);
    u8g2.drawStr(4, 50, myPlant1.name.c_str());
    u8g2.drawStr(69, 50, buffer);
   //u8g2.drawStr(115, 45, moisture_buffer);

    if(myPlant1.plantState == 1)
    {
      u8g2.drawStr(84, 25, "DRY");
    }
    else if(myPlant1.plantState == 2)
    {
      u8g2.drawStr(84, 25, "FAIR");
    }
    else if(myPlant1.plantState == 4)
    {
      u8g2.drawStr(84, 25, "WET");
    }
    else
    {
      u8g2.drawStr(81, 25, "GOOD");
    }

    u8g2.setFont(u8g2_font_fub20_tr);
    u8g2.setFontMode(1);
    u8g2.setDrawColor(1);
    u8g2.drawStr(28, 25, mL_string1);
    u8g2.setFont(u8g2_font_profont11_mf);
    u8g2.setFontMode(1);
    u8g2.setDrawColor(1);
    if (myPlant1.mL < 100)
    {
      u8g2.drawStr(60, 36, "mL");
    }
    else
    {
      u8g2.drawStr(80, 36, "mL");
    }
    u8g2_lastWater();
    u8g2.drawFrame(114, 4, 10, 42);
    
  }
  else if(selector == 2)
  {
    u8g2.setFontMode(2);
    u8g2.setDrawColor(2);
    u8g2.drawStr(4, 50, myPlant2.name.c_str());
    u8g2.drawStr(69, 50, buffer);

    if(myPlant2.plantState == 1)
    {
      u8g2.drawStr(84, 25, "DRY");
    }
    else if(myPlant2.plantState == 2)
    {
      u8g2.drawStr(84, 25, "FAIR");
    }
    else if(myPlant2.plantState == 4)
    {
      u8g2.drawStr(81, 25, "WET");
    }
    else
    {
      u8g2.drawStr(81, 25, "GOOD");
    }

    u8g2.setFont(u8g2_font_fub20_tr);
    u8g2.setFontMode(1);
    u8g2.setDrawColor(1);
    u8g2.drawStr(28, 25, mL_string2);
    u8g2.setFont(u8g2_font_profont11_mf);
    u8g2.setFontMode(1);
    u8g2.setDrawColor(1);
    if (myPlant2.mL < 100)
    {
      u8g2.drawStr(60, 36, "mL");
    }
    else
    {
      u8g2.drawStr(80, 36, "mL");
    }
    u8g2_lastWater();
    u8g2.drawFrame(114, 4, 10, 42);
  }
}

float readSensor() 
{
  float Plantval = analogRead(sensorPin);	// Read the analog value form sensor
	return Plantval;							// Return analog moisture value
}

float readSensor2() 
{
	float Plantval2 = analogRead(sensorPin2);	// Read the analog value form sensor
	return Plantval2;							// Return analog moisture value
}

void startCyclePassive(int counter, int counter2)
{
  u8g2_prepare();

    u8g2.drawRFrame(40, 4, 45, 12, 3);
    if(selector == 1)
    {
      u8g2.drawBox(114, 46 - counter, 9, counter);
    }
    else
    {
      u8g2.drawBox(114, 46 - counter2, 9, counter2);
    }
    u8g2.setFontMode(2);
    u8g2.setDrawColor(2);
    u8g2.drawStr(47, 6, "START");
}

void startCycleReady(int counter, int counter2)
{
    u8g2_prepare();
    u8g2.drawRBox(40, 4, 45, 12, 3);
    if(selector == 1)
    {
      u8g2.drawBox(114, 46 - counter, 9, counter);
    }
    else
    {
      u8g2.drawBox(114, 46 - counter2, 9, counter2);
    }

    u8g2.setFontMode(2);
    u8g2.setDrawColor(2);
    u8g2.drawStr(47, 6, "START");
    colorSelect('r');
}

void startCycleActive(int counter, int counter2, byte k)
{
  //static unsigned long wateringMillis = millis();

  u8g2_prepare();
  //animation(k);
  u8g2.drawRBox(40, 4, 45, 12, 3);
  if(selector == 1)
  {
    u8g2.drawBox(114, 46 - counter, 9, counter);
    //wateringState = true;
    manualTrigger = true;
    running = true;
    colorSelect('b');
    waterStart();
    
  }
  if(selector == 2)
  {
    u8g2.drawBox(114, 46 - counter2, 9, counter);
    //wateringState = true;
    manualTrigger = true;
    running = true;
    colorSelect('b');
    waterStart();
    
  }

  u8g2.setFontMode(2);
  u8g2.setDrawColor(2);
  u8g2.drawStr(50, 6, "STOP");
}


void menuStateReturn(void)
{
  if(currentMillis - menuPreviousMillis >= menuDelay) {
    
  menuPreviousMillis = currentMillis;
  menuState = false;
  }
}

int updatePlantSchedule(String plantName, int nextDay, int nextHour, int nextMinute, int amount)
{
  Serial.println(plantName);


  if (plantName[0] == myPlant1.name[0])
  {
    Serial.println("Plant 1 info updated");
    myPlant1.name = plantName;
    myPlant1.DayNumber = nextDay;
    myPlant1.hour = nextHour;
    myPlant1.minute = nextMinute;
    myPlant1.mL = amount;
  }
  else if (plantName[0] == myPlant2.name[0])
  {
    Serial.println("Plant 2 info updated");
    myPlant2.name = plantName;
    myPlant2.DayNumber = nextDay;
    myPlant2.hour = nextHour;
    myPlant2.minute = nextMinute;
    myPlant2.mL = amount;
  }
  else{
    return -1;
  }
  return 0;
}

void retrieveSchedule(void)
{
  //  if(currentMillis - postInfoPreviousMillis >= 10000) {
  //  postInfoPreviousMillis = currentMillis;
    HTTPClient http;

    String serverPath = serverSchedule; 

    int update1Confirm = 0;
    int update2Confirm = 0;
    
    // Your Domain name with URL path or IP address with path
    http.begin(serverPath.c_str());
    http.addHeader("Content-Type", "application/json");
    
    // If you need Node-RED/server authentication, insert user and password below
    //http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");
    StaticJsonDocument<200> doc;
    // Add values in the document
    
    doc["device_id"] = DEVICE_ID;

    String requestBody;
    serializeJson(doc, requestBody);
    
    int httpResponseCode = http.POST(requestBody);

    if (httpResponseCode>0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      //String payload = http.getString();
      deserializeJson(doc, http.getString());

      String plantName = doc["Plant1"]["Plant_name"];
      int nextDay = doc["Plant1"]["Day"];
      int nextHour = doc["Plant1"]["Hour"];
      int nextMinute = doc["Plant1"]["Minute"];
      int nextAmount = doc["Plant1"]["Amount"];

      String plant2Name = doc["Plant2"]["Plant_name"];
      int plant2_nextDay = doc["Plant2"]["Day"];
      int plant2_nextHour = doc["Plant2"]["Hour"];
      int plant2_nextMinute = doc["Plant2"]["Minute"];
      int plant2_nextAmount = doc["Plant2"]["Amount"];
      //Serial.println(payload);
      //Serial.println(doc);
      Serial.println(plantName);
      Serial.println(nextDay);
      Serial.println(nextHour);
      Serial.println(nextMinute);
      Serial.println(nextAmount);
      Serial.println("\n");

      update1Confirm = updatePlantSchedule(plantName, nextDay, nextHour, nextMinute, nextAmount);
      update2Confirm = updatePlantSchedule(plant2Name, plant2_nextDay, plant2_nextHour, plant2_nextMinute, plant2_nextAmount);

      Serial.print(update1Confirm);
      Serial.println(update2Confirm);
      Serial.println("\n");
    }
    else {
      Serial.print("Error code 2: ");
      Serial.println(httpResponseCode);
    }
    // Free resources
    http.end();
  // }
}

void postData(void)
{
  
   if(currentMillis - postPreviousMillis >= 300000 || initSetup == false) {           //300000 for every 5 min
    postPreviousMillis = currentMillis;
    colorSelect('r');
    HTTPClient http;

    String serverPath = serverName; 
    
    // Your Domain name with URL path or IP address with path
    http.begin(serverPath.c_str());
    http.addHeader("Content-Type", "application/json");
    
    // If you need Node-RED/server authentication, insert user and password below
    //http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");
    StaticJsonDocument<200> doc;
    // Add values in the document
    if(myPlant1.name != "N/A")
    {
      // TODO -- FIX temp value reading
      temperatureVal = 69;
      doc["user_id"] = 5;
      doc["temperature"] = temperatureVal;
      doc["plant1_name"] = myPlant1.name;
      doc["plant1_moisture"] = moisture;
      doc["plant2_name"] = myPlant2.name;
      doc["plant2_moisture"] = moisture2;
      
      String requestBody;
      serializeJson(doc, requestBody);
      
      int httpResponseCode = http.POST(requestBody);

      
      if (httpResponseCode>0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
      }
      else {
        Serial.print("Error code 1: ");
        Serial.println(httpResponseCode);
      }

    }
    // Free resources
    http.end();

    retrieveSchedule();

    //buzz();
    
    colorSelect('o');

  }
}



// 'New Piskel-1', 47x47px
const unsigned char epd_bitmap_New_Piskel_1 [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 
	0x00, 0x00, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x10, 0x00, 0x00, 0x00, 0x04, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0xf0, 0x87, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 
	0x88, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x84, 0x20, 0x04, 0x00, 0x00, 0x00, 0xd6, 0x20, 0x00, 0x00, 
	0x00, 0x00, 0x44, 0xf0, 0x10, 0x00, 0x00, 0x00, 0x06, 0xb8, 0x03, 0x00, 0x00, 0x00, 0x02, 0x3c, 
	0x06, 0x00, 0x00, 0x00, 0x0c, 0x37, 0x08, 0x00, 0x00, 0x00, 0xfc, 0x71, 0x08, 0x00, 0x00, 0x00, 
	0x00, 0x70, 0x10, 0x00, 0x00, 0x00, 0x02, 0x70, 0x10, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x10, 0x00, 
	0x00, 0x00, 0x00, 0x30, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 
	0x20, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x02, 0x30, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0x1f, 0x00, 0x00, 0x00, 0xf8, 0xff, 0x5f, 0x00, 
	0x00, 0x00, 0xf8, 0xff, 0x1f, 0x00, 0x00, 0x00, 0xf8, 0xff, 0x1f, 0x00, 0x00, 0x00, 0xf0, 0xff, 
	0x0f, 0x00, 0x00, 0x00, 0x10, 0x00, 0x08, 0x00, 0x00, 0x00, 0x10, 0x00, 0x08, 0x00, 0x00, 0x00, 
	0x10, 0x00, 0x08, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 
	0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x00, 0x00, 0x20, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0x40, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x00, 0x02, 0x00, 0x00, 0x00, 
	0x40, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x00, 0x02, 0x00, 0x00, 0x00, 0x80, 0x00, 0x01, 0x00, 
	0x00, 0x00, 0x80, 0x00, 0x01, 0x00, 0x00, 0x00, 0x80, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
// 'New Piskel-1', 47x47px
const unsigned char epd_bitmap_New_Piskel_2 [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x88, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0x12, 0x00, 0x08, 0x00, 0x00, 0x00, 0x80, 0x1f, 0x01, 0x00, 0x00, 0x00, 
	0xe0, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x32, 0xa6, 0x00, 0x00, 0x00, 0x00, 0x10, 0x23, 0x00, 0x00, 
	0x00, 0x00, 0x08, 0x31, 0x00, 0x00, 0x00, 0x00, 0x88, 0xf1, 0x10, 0x00, 0x00, 0x00, 0x8c, 0xb8, 
	0x01, 0x00, 0x00, 0x00, 0x84, 0x38, 0x0b, 0x00, 0x00, 0x00, 0x04, 0x3c, 0x06, 0x00, 0x00, 0x00, 
	0x04, 0x36, 0x2c, 0x00, 0x00, 0x00, 0xc4, 0x71, 0x08, 0x00, 0x00, 0x00, 0x3c, 0x70, 0x10, 0x00, 
	0x00, 0x00, 0x02, 0xf0, 0x10, 0x02, 0x00, 0x00, 0x00, 0xb0, 0x1b, 0x00, 0x00, 0x00, 0x00, 0x30, 
	0x0e, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0x1f, 0x00, 0x00, 0x00, 0xfa, 0xff, 0x1f, 0x00, 
	0x00, 0x00, 0xf8, 0xff, 0x1f, 0x00, 0x00, 0x00, 0xf8, 0xff, 0x1f, 0x00, 0x00, 0x00, 0xf0, 0xff, 
	0x0f, 0x00, 0x00, 0x00, 0x10, 0x00, 0x08, 0x00, 0x00, 0x00, 0x10, 0x00, 0x08, 0x00, 0x00, 0x00, 
	0x10, 0x00, 0x08, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 
	0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x00, 0x00, 0x20, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0x60, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x00, 0x02, 0x00, 0x00, 0x00, 
	0x40, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x00, 0x02, 0x00, 0x00, 0x00, 0x80, 0x00, 0x01, 0x00, 
	0x00, 0x00, 0x80, 0x00, 0x01, 0x00, 0x00, 0x00, 0x80, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
// 'New Piskel-1', 47x47px
const unsigned char epd_bitmap_New_Piskel_3 [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 
	0x00, 0x00, 0x88, 0x0f, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x70, 0x93, 
	0x00, 0x00, 0x00, 0x00, 0x18, 0x31, 0x28, 0x00, 0x00, 0x00, 0xc8, 0x30, 0x00, 0x00, 0x00, 0x00, 
	0x46, 0x30, 0x00, 0x00, 0x00, 0x00, 0x22, 0x32, 0x22, 0x00, 0x00, 0x00, 0x32, 0x30, 0x00, 0x00, 
	0x00, 0x00, 0x92, 0xf8, 0x07, 0x00, 0x00, 0x00, 0x02, 0x3c, 0x0c, 0x00, 0x00, 0x00, 0x02, 0x36, 
	0x10, 0x00, 0x00, 0x00, 0x82, 0x33, 0x30, 0x00, 0x00, 0x00, 0xfe, 0x30, 0x20, 0x00, 0x00, 0x00, 
	0x00, 0x30, 0x20, 0x00, 0x00, 0x00, 0x00, 0x30, 0x20, 0x00, 0x00, 0x00, 0x00, 0x30, 0x60, 0x00, 
	0x00, 0x00, 0x00, 0x70, 0x60, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x41, 0x00, 0x00, 0x00, 0x00, 0x30, 
	0x7f, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0x1f, 0x00, 0x00, 0x00, 0xf8, 0xff, 0x1f, 0x02, 
	0x00, 0x00, 0xf8, 0xff, 0x1f, 0x00, 0x00, 0x00, 0xf8, 0xff, 0x1f, 0x00, 0x00, 0x00, 0xf0, 0xff, 
	0x0f, 0x00, 0x00, 0x00, 0x10, 0x00, 0x08, 0x00, 0x00, 0x00, 0x12, 0x00, 0x08, 0x00, 0x00, 0x00, 
	0x10, 0x00, 0x08, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 
	0x00, 0x00, 0x20, 0x00, 0x44, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x00, 0x00, 0x20, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0x60, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x00, 0x02, 0x00, 0x00, 0x00, 
	0x40, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x00, 0x02, 0x00, 0x00, 0x00, 0x80, 0x00, 0x01, 0x00, 
	0x00, 0x00, 0x80, 0x00, 0x01, 0x00, 0x00, 0x00, 0x80, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
  // 'New Piskel-1', 47x47px
const unsigned char epd_bitmap_New_Piskel_4 [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x1f, 0x00, 0x00, 0x00, 0x00, 
	0xe0, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x30, 0x26, 0x00, 0x00, 0x00, 0x00, 0x10, 0x23, 0x00, 0x00, 
	0x00, 0x00, 0x08, 0x31, 0x00, 0x00, 0x00, 0x00, 0x88, 0xf1, 0x00, 0x00, 0x00, 0x00, 0x8c, 0xb8, 
	0x01, 0x00, 0x00, 0x00, 0x84, 0x38, 0x03, 0x00, 0x00, 0x00, 0x04, 0x3c, 0x06, 0x00, 0x00, 0x00, 
	0x04, 0x36, 0x0c, 0x00, 0x00, 0x00, 0xc4, 0x71, 0x08, 0x00, 0x00, 0x00, 0x3c, 0x70, 0x10, 0x00, 
	0x00, 0x00, 0x00, 0xf0, 0x10, 0x00, 0x00, 0x00, 0x00, 0xb0, 0x1b, 0x00, 0x00, 0x00, 0x00, 0x30, 
	0x0e, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0x1f, 0x00, 0x00, 0x00, 0xf8, 0xff, 0x1f, 0x00, 
	0x00, 0x00, 0xf8, 0xff, 0x1f, 0x00, 0x00, 0x00, 0xf8, 0xff, 0x1f, 0x00, 0x00, 0x00, 0xf0, 0xff, 
	0x0f, 0x00, 0x00, 0x00, 0x10, 0x00, 0x08, 0x00, 0x00, 0x00, 0x10, 0x00, 0x08, 0x00, 0x00, 0x00, 
	0x10, 0x00, 0x08, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 
	0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x00, 0x00, 0x20, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0x60, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x00, 0x02, 0x00, 0x00, 0x00, 
	0x40, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x00, 0x02, 0x00, 0x00, 0x00, 0x80, 0x00, 0x01, 0x00, 
	0x00, 0x00, 0x80, 0x00, 0x01, 0x00, 0x00, 0x00, 0x80, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Array of all bitmaps for convenience. (Total bytes used to store images in PROGMEM = 912)
const int epd_bitmap_allArray_LEN = 4;
const unsigned char* epd_bitmap_allArray[4] = {
	epd_bitmap_New_Piskel_1,
	epd_bitmap_New_Piskel_2,
	epd_bitmap_New_Piskel_3,
  epd_bitmap_New_Piskel_4
};

  void animation(byte k)
  {    
    u8g2.setDrawColor(1);
    
      if(selector == 1 && menuState == false)
      {
        u8g2.drawXBMP(3, 1, 47 , 47, epd_bitmap_allArray[k]);
        u8g2.drawXBMP(67, 1, 47 , 47, epd_bitmap_allArray[3]);
      }
      else if(selector == 2 && menuState == false)
      {
        u8g2.drawXBMP(3, 1, 47 , 47, epd_bitmap_allArray[3]);
        u8g2.drawXBMP(67, 1, 47 , 47, epd_bitmap_allArray[k]);
      }
      else if(menuState == false)
      {
        u8g2.drawXBMP(3, 1, 47 , 47, epd_bitmap_allArray[3]);
        u8g2.drawXBMP(67, 1, 47 , 47, epd_bitmap_allArray[3]);
      }
      else if(menuState == true && startState == 0)
      {
        u8g2.drawXBMP(3, 1, 47 , 47, epd_bitmap_allArray[3]);
      }
      else
      {
        u8g2.drawXBMP(3, 1, 47 , 47, epd_bitmap_allArray[k]);
      }
  }

// ====================================================================================================================================

//                      SETUP 

// ====================================================================================================================================

void setup(void) 
{
  // set notification call-back function
  sntp_set_time_sync_notification_cb( timeavailable );

    /**
   * NTP server address could be aquired via DHCP,
   *
   * NOTE: This call should be made BEFORE esp32 aquires IP address via DHCP,
   * otherwise SNTP option 42 would be rejected by default.
   * NOTE: configTime() function call if made AFTER DHCP-client run
   * will OVERRIDE aquired NTP server address
   */
  //sntp_servermode_dhcp(1);    // (optional)

  /**
   * This will set configured ntp servers and constant TimeZone/daylightOffset
   * should be OK if your time zone does not need to adjust daylightOffset twice a year,
   * in such a case time adjustment won't be handled automagicaly.
   */
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);

  /**
   * A more convenient approach to handle TimeZones with daylightOffset 
   * would be to specify a environmnet variable with TimeZone definition including daylight adjustmnet rules.
   * A list of rules for your zone could be obtained from https://github.com/esp8266/Arduino/blob/master/cores/esp8266/TZ.h
   */
  //configTzTime(time_zone, ntpServer1, ntpServer2);

  //connect to WiFi
  //Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);

  //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  //WiFiManager wm;

  // reset settings - wipe stored credentials for testing
  // these are stored by the esp library
  //wm.resetSettings();                         // ---------------- UNCOMMENT THIS TO RESET THE WIFI AND REQUIRE AP CREDENTIALS AGAIN (FOR TESTING)

  //bool res;
    // res = wm.autoConnect(); // auto generated AP name from chipid
    // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
    //res = wm.autoConnect("Garduino","swordfish"); // password protected ap

    //if(!res) {
        //Serial.println("Failed to connect");
        // ESP.restart();
    //}
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println(" CONNECTED");

    
  // if(!SD.begin(SD_CS))
  // {
  //   Serial.println("Error accessing microSD card!");
  //   while(true);
  // }
  

  u8g2.begin();

  dht.begin();

  delay(500);

  pinMode(RED, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(BLUE, OUTPUT);
  pinMode(BUZZER, OUTPUT);


  colorSelect('o');
  delay(25);
  colorSelect('r');
  delay(500);
  colorSelect('g');
  delay(500);
  colorSelect('b');
  delay(500);

  colorSelect('g');

  pinMode(SW, INPUT);       // Enable the switchPin as input 
  pinMode(DT, INPUT);                   // Set PinA as input
  pinMode(CLK, INPUT);                   // Set PinB as input

  //pinMode(sensorPin, INPUT);
  //pinMode(sensorPin2, INPUT);

  pinMode(Relay_1, OUTPUT);               // Pump
  pinMode(Relay_2, OUTPUT);               // Solenoid 1
  pinMode(Relay_3, OUTPUT);               // Solenoid 2
  pinMode(Relay_4, OUTPUT);  
  
  digitalWrite(Relay_1, HIGH);
  digitalWrite(Relay_2, HIGH);
  digitalWrite(Relay_3, HIGH);
  digitalWrite(Relay_4, HIGH);

  digitalWrite(BUZZER, HIGH);

  
  // Atach a CHANGE interrupt to PinB and exectute the update function when this change occurs.
  attachInterrupt(digitalPinToInterrupt(22), update, CHANGE);

  lastStateCLK = digitalRead(CLK);

  Serial.begin(115200);

  digitalWrite(BUZZER, LOW);

  postData();
  
  initSetup = true;



}

// ====================================================================================================================================

//                      LOOP 

// ====================================================================================================================================

void loop(void) 
{

  currentMillis = millis();

  if(currentMillis - animationPreviousMillis >= animationDelay) {
    
    animationPreviousMillis = currentMillis;

    k++;

    if(k > 3)
    {
      k = 0;
    }
  }


  if(currentMillis - readingPreviousMillis >= readingDelay) {
    
    readingPreviousMillis = currentMillis;
    
    uint32_t readings1 = 0;
    uint32_t readings2 = 0;

    // take 5 readings
    for(int i = 0; i < 5; i++)
    {
      readings1 = readings1 + analogRead(sensorPin);
      readings2 = readings2 + analogRead(sensorPin2);
    }
    int sensorVal1 = readings1 / 5; // average the last 5 readings;
    int sensorVal2 = readings2 / 5; 

    temperatureVal = dht.readTemperature(true);

    moisture = map(sensorVal1,  1000, 3000, 100, 0);
    moisture2 = map(sensorVal2, 1000, 3000, 100, 0);
    
    Serial.print(sensorVal1);
    Serial.print("\t");
    Serial.print(moisture);
    Serial.print("\t");
    Serial.print(sensorVal2);
    Serial.print("\t");
    Serial.print(moisture2);
    Serial.print("\t");
    Serial.print(temperatureVal);
    Serial.print("\n");


    printLocalTime();     // it will take some time to sync time :)

    counter = map(moisture, 0, 100, 0, 41);
    counter2 = map(moisture2, 0, 100, 0, 41);
  }


  postData();

  //retrieveSchedule();

  u8g2.clearBuffer();

  int reading = digitalRead(SW);

  // If the switch changed, due to noise or pressing:
  if (reading != lastbtnState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the button state has changed:
    if (reading != btnState) {
      btnState = reading;

        if(btnState == LOW)
        {
          diffState = true;
          delay(300);
        }
    }
    
  }

  // save the reading. Next time through the loop, it'll be the lastButtonState:
  lastbtnState = reading;

  checkMenu();

  checkPlantState();

  //menuStateReturn();


    if (menuState == false)
    {
      multibox();
      startState = 0;
      
    }
    else if(menuState == true && startState == 0)
    {
      singleBoxBig(counter, k);
      startCyclePassive(counter, counter2);
      colorSelect('g');
    }
    else if(menuState == true && startState == 2)
    {
      singleBoxBig(counter, k);
      startCycleActive(counter, counter2, k);
      colorSelect('b');
      //waterStart();
    
      //animationPreviousMillis = currentMillis;
      //counter--;
    }
    else if(menuState == true && startState == 1)
    {
      singleBoxBig(counter, k);
      startCycleReady(counter, counter2);
      colorSelect('r');
    }

  if(moisture > 80)
  {
    myPlant1.plantState = 4;
  }
  else if(moisture < 30)
  {
    myPlant1.plantState = 1;
    
  }
  else if(moisture >= 30 && moisture <= 50)
  {
    myPlant1.plantState = 2;
  }
  else
  {
    myPlant1.plantState = 3;
  }

  if(moisture2 > 80)
  {
    myPlant2.plantState = 4;
  }
  else if(moisture2 < 30)
  {
    myPlant2.plantState = 1;
  }
  else if(moisture2 >= 30 && moisture2 <= 50)
  {
    myPlant2.plantState = 2;
  }
  else
  {
    myPlant2.plantState = 3;
  }

  u8g2.sendBuffer();

  if(running == true && selector == 1) {
    delay(wateringDelay1);
    waterEnd();
    wateringState = false;
    startState = 0;
    running = false;
  }
  else if(running == true && selector == 2) {
    delay(wateringDelay2);
    waterEnd();
    wateringState = false;
    startState = 0;
    running = false;
  }
  else
  {
    waterEnd();
  }

  diffState = false;
}

  


