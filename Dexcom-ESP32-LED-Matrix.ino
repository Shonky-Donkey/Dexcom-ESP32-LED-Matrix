/*
Forked from https://github.com/TooSpeedy/Dexcom-ESP32-LED-Matrix/tree/main
Contains code (c) Adafruit, license BSD
Requires manual install of https://github.com/aligator/NeoPixelBusGfx library

Tested on esp32-S3, following settings:
  USB CDC On Boot: Enabled
  CPU Frequency: 240
  Core Debug level: none
  USB DFU on boot: enabled
  Erase all flash before sketch upload: enabled
  Events run on core: 1
  Flashmode: QIO 80Mhz
  Flash SIze: 16MB (128Mb)
  JTAG adapter: Disabled
  Arduino Runs On: Core 1
  USB Firmware MSC On boot: Enabled
  Partition Scheme: Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)
  PSRAM: Disabled
  Upload Mode: USB-OTG CDC (TinyUSB)
  Upload Speed: 921600
  USB Mode: USB-OTG (TinyUSB)
*/


#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <NeoPixelBrightnessBusGfx.h> //https://github.com/aligator/NeoPixelBusGfx
#include <NeoPixelBrightnessBus.h>    //https://github.com/aligator/NeoPixelBusGfx
#include <esp_task_wdt.h> //ESP watchdog timer library
#include <Chrono.h> //Chrono library by Sofian Audry and Thomas Ouellet Fredericks

//Loop Timer Stuff-----------------------------------------------------------
#define DISPLAY_LOOP_TIMER 100 //ms before loop starts again.
#define CLIENT_LOOP_TIMER 100000
#define MUTE_TIMEOUT 30 //how long the mute button will silence in minutes. Suggest 30 or less for 32 pixel wide display. If you want to go more than this, you will need to fix the display code to make 1 pixel = more than 1 minute.
#define LDR_HYSTERESIS 300
#define DEBOUNCE 10  // this many loops before it does something
#define REAUTH_LOOP_TIMER 60000 // seconds most of a day. Auth string will only last a day
#define WARN_LITTLE_TIMER 60 // seconds
#define BLINK_TIME 1000
#define WDT_TIMEOUT 5 //watchdog timer in seconds
Chrono displayLoopTimer;
Chrono clientLoopTimer;
Chrono blinkTimer;
Chrono reauthLoopTimer(Chrono::SECONDS);
Chrono warnLittleLoopTimer(Chrono::SECONDS);
//END /Loop Timer Stuff------------------------------------------------------

//hardware pins
#define DIM_PIN 18
#define LDR_PIN 16
#define LED_PIN 14  // Data pin for LED Matrix
#define SPKR_PIN  17

//thresholds for glucose
#define VERY_HIGH 250
#define HIGH 180
#define IN_RANGE 69
#define LOW 54

// Define matrix width and height.
#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 8

NeoPixelBrightnessBusGfx<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod> *matrix = new NeoPixelBrightnessBusGfx<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod>(MATRIX_WIDTH, MATRIX_HEIGHT, LED_PIN); // See https://github.com/Makuna/NeoPixelBus/wiki/NeoPixelBus-object and https://github.com/Makuna/NeoPixelBus/wiki/ESP32-NeoMethods
NeoTopology<ColumnMajorAlternating180Layout> topo(MATRIX_WIDTH, MATRIX_HEIGHT); //see https://github.com/Makuna/NeoPixelBus/wiki/Matrix-Panels-Support

//#define MMOL // 1 for mmol/l or 0 for mg/dl
//#define US // 1 for US 0 for international

static const uint8_t PROGMEM
    arrows[][7] =
    {
	{   // 0: flat
	    B00001000,
	    B00000100,
	    B00000010,
	    B11111111,
	    B00000010,
	    B00000100,
	    B00001000,
			},

	{   // 1: fortyFiveUp
	    B00001111,
	    B00000011,
	    B00000101,
	    B00001001,
	    B00010000,
	    B00100000,
	    B01000000,
			},

	{   // 2: singleUp
	    B00001000,
	    B00011100,
	    B00101010,
	    B01001001,
	    B00001000,
	    B00001000,
	    B00001000,
			},

	{   // 3: doubleUp
	    B00011000,
	    B00111100,
	    B01011010,
	    B10011001,
	    B00011000,
	    B00011000,
	    B00011000,
			},

	{   // 4; fortyFiveDown
	    B01000000,
	    B00100000,
	    B00010000,
	    B00001001,
	    B00000101,
	    B00000011,
	    B00001111,
			},

	{   // 5; singleDown
	    B00001000,
	    B00001000,
	    B00001000,
	    B01001001,
	    B00101010,
	    B00011100,
	    B00001000,
			},

	{   // 6; doubleDown
	    B00011000,
	    B00011000,
	    B00011000,
	    B10011001,
	    B01011010,
	    B00111100,
	    B00011000,
			},
	{   // 7; X
	    B00000000,
	    B00000000,
	    B00000000,
	    B00000000,
	    B00000000,
	    B00000000,
	    B00000000,
			},
    };

#define FLAT 0
#define FORTYFIVEUP 1
#define SINGLEUP 2
#define DOUBLEUP 3
#define FORTYFIVEDOWN 4
#define SINGLEDOWN 5
#define DOUBLEDOWN 6
#define XXX 7

#define RED (31 << 11)
#define GREEN (63 << 5)
#define YELLOW (RED + GREEN)
#define BLUE 31
#define PURPLE (RED + BLUE)
#define TEAL (GREEN + BLUE)
#define WHITE (RED + GREEN + BLUE)

// Wifi Config
const char* ssid = "your wifi ssid here";
const char* password = "your wifi password here";

// Decom URL - US use share1
const char* host = "https://share1.dexcom.com/ShareWebServices/Services/General/LoginPublisherAccountByName";

int BRIGHT = 40;
String authkey;
float FormattedDex;
String DexcomTrend;
int mute = 0; 
int pushbutton = 0; //used as a counter that will count cycles so you can do different things with different length pushes, and also a debounce
long mutetimer = 0;
int blink = 0;
int brightOn = 1;
bool warnLittleUp = 0;
bool warnLittleDown = 0;


void setup() {
  //digitalWrite(LED_PIN, LOW);
  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch
  esp_task_wdt_reset(); //reset watchdog timer
  analogReadResolution(12); //0-4095
  pinMode(DIM_PIN, INPUT_PULLUP);
  tone(SPKR_PIN, 440, 100);
  delay(100);

  matrix->setRemapFunction(&remap);
  matrix->Begin();
  matrix->fillScreen(0);
  matrix->Show();
  matrix->setTextWrap(false);
  matrix->setTextSize(1);
  matrix->setRotation(0);
  matrix->SetBrightness(5);
  matrix->setTextColor(RED);
  matrix->fillScreen(0);
  matrix->setCursor(1, 1);
  matrix->print("Wi-fi");
  matrix->Show();
  
  delay(100);
 
  //Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    esp_task_wdt_reset(); //reset watchdog timer
   // Serial.println("Connecting to WiFi...");
  }
  tone(SPKR_PIN, 880, 100);
 // Serial.println("Connected to WiFi");
  authenticate();
  reauthLoopTimer.restart();
  warnLittleLoopTimer.restart(); 
  esp_task_wdt_reset(); //reset watchdog timer
  //delay(500);
  clientLoopTimer.add(CLIENT_LOOP_TIMER);//this makes sure things run straight away
  displayLoopTimer.add(DISPLAY_LOOP_TIMER); 
  esp_task_wdt_reset(); //reset watchdog timer

}
//int x = matrix->width();
int pass = 0;

void loop() {
  if(WiFi.status() != WL_CONNECTED) { 
    tone(SPKR_PIN, 880, 100);
    tone(SPKR_PIN, 440, 100);
    while (WiFi.status() != WL_CONNECTED) {
      matrix->SetBrightness(50);  //            MIGHT WANT TO CHANGE THIS TO GET MORE AND MORE BRIGHT OVER TIME DEPENDING ON HOW FAR FROM IDEAL GLUCOSE
      matrix->setTextColor(RED);
      matrix->fillScreen(0);
      matrix->setCursor(1, 1);
      matrix->print("Wi-fi");
      matrix->Show();
      esp_task_wdt_reset(); //reset watchdog timer
      delay(1000);
      // Serial.println("Connecting to WiFi...");
      }
    reauthLoopTimer.add(REAUTH_LOOP_TIMER);
    
  }
  //ADD CHECK FOR AUTH HERE> MIGHT NEED TO LOOK AT WHAT RESPONSE IS WHEN AUTH IS BAD
  if(reauthLoopTimer.hasPassed(REAUTH_LOOP_TIMER)){
    authenticate();
    reauthLoopTimer.restart();
  }
  if(blinkTimer.hasPassed(BLINK_TIME)){
    if(blink == 1){blink = 0;}
    else{blink = 1;}
    blinkTimer.restart();
  }
  if (clientLoopTimer.hasPassed(CLIENT_LOOP_TIMER)){//||!firstRun){
    getPayload();
    clientLoopTimer.restart();
  }

  if (displayLoopTimer.hasPassed(DISPLAY_LOOP_TIMER)){
    displayLoopTimer.restart();
    drawNumberArrow();
  }
  if (warnLittleLoopTimer.hasPassed(WARN_LITTLE_TIMER)){
    warnLittleLoopTimer.restart();
    if (mute == 0){
      if(warnLittleUp){
        tone(SPKR_PIN, 440, 100);
        tone(SPKR_PIN, 880, 100);
        tone(SPKR_PIN, 1320, 100);
      }
      if(warnLittleDown){
        tone(SPKR_PIN, 1320, 100);
        tone(SPKR_PIN, 880, 100);
        tone(SPKR_PIN, 440, 100);
      }
    }
  }


  esp_task_wdt_reset(); //reset watchdog timer
  //https.end();
}

void authenticate(){
  matrix->setTextColor(RED);
  matrix->fillScreen(0);
  matrix->setCursor(1, 1);
  matrix->print("Auth");
  matrix->Show();

  HTTPClient https;
  String url = host;
  https.begin(url);
  https.addHeader("Content-Type", "application/json");
  https.addHeader("User-Agent", "Dexcom Share/3.0.2.11 CFNetwork/711.2.23 Darwin/14.0.0");
  https.addHeader("Accept", "application/json");
  int httpCode = 100;
  
  while(httpCode > 299 || httpCode < 200){
    // Credentials
    String json = "{\"accountName\":\"your dexcom account name here\",\"applicationId\":\"d89443d2-327c-4a6f-89e5-496bbb0317db\",\"password\":\"Your dexcom password here\"}";
    httpCode = https.POST(json);
    if(httpCode > 299 || httpCode < 200){
      matrix->setTextColor(RED);
      matrix->fillScreen(0);
      matrix->setCursor(1, 1);
      matrix->print("E");
      matrix->print(httpCode);
      matrix->Show();
      if (warnLittleLoopTimer.hasPassed(WARN_LITTLE_TIMER)){
        warnLittleLoopTimer.restart();
        tone(SPKR_PIN, 1320, 100);
        tone(SPKR_PIN, 880, 100);
        tone(SPKR_PIN, 440, 100);
      }
      for(int i = 0; i < 10; i++){ // 1 second per loop. How many loops is how long it will wait before retrying.
        esp_task_wdt_reset(); //reset watchdog timer
        delay(1000);
      }
    }
  matrix->fillScreen(0);
  matrix->Show();  
  }

  String payload = https.getString();
  //Serial.println(httpCode);
  //Serial.println(payload);
  authkey = payload; 
  authkey = authkey.substring(1, authkey.length() - 1);
  https.end();

}

void getPayload(){
    // Build Dexcom URL
    String dexcomreadingurl = "https://share1.dexcom.com/ShareWebServices/Services/Publisher/ReadPublisherLatestGlucoseValues?sessionId=" + authkey + "&minutes=1440&maxCount=1";
    HTTPClient https;
    // Add Headers
    https.begin(dexcomreadingurl);
    https.addHeader("Content-Type", "application/json");
    https.addHeader("User-Agent", "Dexcom Share/3.0.2.11 CFNetwork/711.2.23 Darwin/14.0.0");
    https.addHeader("Accept", "application/json");
    // Make reuqest and get readings
    //int httpCode = 100;
    //while(httpCode > 299 || httpCode < 200){
    int  httpCode = https.GET();
    //   if(httpCode > 299 || httpCode < 200){
    //     matrix->setTextColor(RED);
    //     matrix->fillScreen(0);
    //     matrix->setCursor(1, 1);
    //     matrix->print("E");
    //     matrix->print(httpCode);
    //     matrix->Show();
    //     if (warnLittleLoopTimer.hasPassed(WARN_LITTLE_TIMER)){
    //       warnLittleLoopTimer.restart();
    //       tone(SPKR_PIN, 1320, 100);
    //       tone(SPKR_PIN, 880, 100);
    //       tone(SPKR_PIN, 440, 100);
    //     }
    //     for(int i = 0; i < 10; i++){ // 1 second per loop. How many loops is how long it will wait before retrying.
    //       esp_task_wdt_reset(); //reset watchdog timer
    //       delay(1000);
    //     }
    //   }
    // }
    String payload1 = https.getString();
    //Serial.println(httpCode);
    //Serial.println(payload1);
    payload1 = payload1.substring(1, payload1.length() - 1);
    StaticJsonDocument<200> doc;
    deserializeJson(doc, payload1);
    // You can now access the values in the JSON object like this:
    String DexcomDate = doc["ST"];
    float DexcomValue = doc["Value"];
    FormattedDex = DexcomValue;// / 18;
    String tempString = doc["Trend"];
    DexcomTrend = tempString;
    float Testing = (FormattedDex, 1);
    //Serial.println(DexcomDate);
    //Serial.println(DexcomValue);
    //Serial.println(FormattedDex, 1);
    //Serial.println(DexcomTrend);
    //Serial.println(Testing);
}

uint16_t remap(uint16_t x, uint16_t y) {
  return topo.Map(x, y);
}

void drawNumberArrow(){
  int arrow_color = RED;
  int arrow_type = DOUBLEDOWN;
  int textcolor = RED;
  bool warn = 1; //change max brightness based on this, and make it flash.. might add second one for low only for audible alarm. output relay for light maybe? top row of LEDs white maybe?
  warnLittleUp = 0;
  warnLittleDown = 0;

  if (FormattedDex > VERY_HIGH){
    if (DexcomTrend == "Flat" || DexcomTrend == "flat") {
      arrow_color = YELLOW; 
      arrow_type = FLAT;
      warn = 0;
    }
    if (DexcomTrend == "fortyFiveUp" || DexcomTrend == "FortyFiveUp") {
      arrow_type = FORTYFIVEUP;
      warn = 0;
    }
    if (DexcomTrend == "SingleUp" || DexcomTrend == "singleUp") {
      arrow_type = SINGLEUP;
      warn = 0;
    }
    if (DexcomTrend == "doubleUp" || DexcomTrend == "DoubleUp") {
      arrow_type = DOUBLEUP;
    }   
    if (DexcomTrend == "fortyFiveDown" || DexcomTrend == "FortyFiveDown") {
      arrow_type = FORTYFIVEDOWN;
      arrow_color = GREEN; 
      warn = 0;
    }   
    if (DexcomTrend == "singleDown" || DexcomTrend == "SingleDown") {
      arrow_type = SINGLEDOWN;
      arrow_color = GREEN; 
      warn = 0;
    }   
    if (DexcomTrend == "doubleDown" || DexcomTrend == "DoubleDown") {
      arrow_type = DOUBLEDOWN;
      arrow_color = GREEN; 
      warn = 0;
    }   
  }
  else if (FormattedDex > HIGH){
    if (DexcomTrend == "Flat" || DexcomTrend == "flat") {
      arrow_type = FLAT;
      arrow_color = YELLOW; 
      warn = 0;
    }
    if (DexcomTrend == "fortyFiveUp" || DexcomTrend == "FortyFiveUp") {
      arrow_type = FORTYFIVEUP;
      arrow_color = YELLOW; 
      warn = 0;
    }
    if (DexcomTrend == "SingleUp" || DexcomTrend == "singleUp") {
      arrow_type = SINGLEUP;
      warn = 0;
    }
    if (DexcomTrend == "doubleUp" || DexcomTrend == "DoubleUp") {
      arrow_type = DOUBLEUP;
      warn = 0;
      warnLittleUp = 1;
    }   
    if (DexcomTrend == "fortyFiveDown" || DexcomTrend == "FortyFiveDown") {
      arrow_type = FORTYFIVEDOWN;
      arrow_color = GREEN; 
      warn = 0;
    }   
    if (DexcomTrend == "singleDown" || DexcomTrend == "SingleDown") {
      arrow_type = SINGLEDOWN;
      arrow_color = GREEN; 
      warn = 0;
    }   
    if (DexcomTrend == "doubleDown" || DexcomTrend == "DoubleDown") {
      arrow_type = DOUBLEDOWN;
      arrow_color = YELLOW; 
      warn = 0;
    }   
  }
  else if (FormattedDex > IN_RANGE + ((HIGH - IN_RANGE) / 2)){ //on the high side of in range
    if (DexcomTrend == "Flat" || DexcomTrend == "flat") {
      arrow_type = FLAT;
      arrow_color = GREEN; 
      warn = 0;
    }
    if (DexcomTrend == "fortyFiveUp" || DexcomTrend == "FortyFiveUp") {
      arrow_type = FORTYFIVEUP;
      arrow_color = YELLOW; 
      warn = 0;
    }
    if (DexcomTrend == "SingleUp" || DexcomTrend == "singleUp") {
      arrow_type = SINGLEUP;
      warn = 0;
    }
    if (DexcomTrend == "doubleUp" || DexcomTrend == "DoubleUp") {
      arrow_type = DOUBLEUP;
      warn = 0;
      warnLittleUp = 1;
    }   
    if (DexcomTrend == "fortyFiveDown" || DexcomTrend == "FortyFiveDown") {
      arrow_type = FORTYFIVEDOWN;
      arrow_color = GREEN; 
      warn = 0;
    }   
    if (DexcomTrend == "singleDown" || DexcomTrend == "SingleDown") {
      arrow_type = SINGLEDOWN;
      warn = 0;
    }   
    if (DexcomTrend == "doubleDown" || DexcomTrend == "DoubleDown") {
      arrow_type = DOUBLEDOWN;
    }   
  }
  else if (FormattedDex > IN_RANGE){ //on the low side of in range
    if (DexcomTrend == "Flat" || DexcomTrend == "flat") {
      arrow_type = FLAT;
      arrow_color = GREEN; 
      warn = 0;
    }
    if (DexcomTrend == "fortyFiveUp" || DexcomTrend == "FortyFiveUp") {
      arrow_type = FORTYFIVEUP;
      arrow_color = GREEN; 
      warn = 0;
    }
    if (DexcomTrend == "SingleUp" || DexcomTrend == "singleUp") {
      arrow_type = SINGLEUP;
      warn = 0;
    }
    if (DexcomTrend == "doubleUp" || DexcomTrend == "DoubleUp") {
      arrow_type = DOUBLEUP;
      warn = 0;
    }   
    if (DexcomTrend == "fortyFiveDown" || DexcomTrend == "FortyFiveDown") {
      arrow_type = FORTYFIVEDOWN;
      arrow_color = YELLOW; 
      warn = 0;
    }   
    if (DexcomTrend == "singleDown" || DexcomTrend == "SingleDown") {
      arrow_type = SINGLEDOWN;
      warn = 0;
    }   
    if (DexcomTrend == "doubleDown" || DexcomTrend == "DoubleDown") {
      arrow_type = DOUBLEDOWN;
    }   
  }
  else if (FormattedDex > LOW){ 
    if (DexcomTrend == "Flat" || DexcomTrend == "flat") {
      arrow_type = FLAT;
      arrow_color = YELLOW; 
      warn = 0;
      warnLittleDown = 1;
    }
    if (DexcomTrend == "fortyFiveUp" || DexcomTrend == "FortyFiveUp") {
      arrow_type = FORTYFIVEUP;
      arrow_color = GREEN; 
      warn = 0;
    }
    if (DexcomTrend == "SingleUp" || DexcomTrend == "singleUp") {
      arrow_type = SINGLEUP;
      arrow_color = GREEN; 
      warn = 0;
    }
    if (DexcomTrend == "doubleUp" || DexcomTrend == "DoubleUp") {
      arrow_type = DOUBLEUP;
      arrow_color = GREEN; 
      warn = 0;
    }   
    if (DexcomTrend == "fortyFiveDown" || DexcomTrend == "FortyFiveDown") {
      arrow_type = FORTYFIVEDOWN;
    }   
    if (DexcomTrend == "singleDown" || DexcomTrend == "SingleDown") {
      arrow_type = SINGLEDOWN;
    }   
    if (DexcomTrend == "doubleDown" || DexcomTrend == "DoubleDown") {
      arrow_type = DOUBLEDOWN;
    }   
  }
  else { //very low
    if (DexcomTrend == "Flat" || DexcomTrend == "flat") {
      arrow_type = FLAT;
    }
    if (DexcomTrend == "fortyFiveUp" || DexcomTrend == "FortyFiveUp") {
      arrow_type = FORTYFIVEUP;
      arrow_color = GREEN; 
      warn = 0;
    }
    if (DexcomTrend == "SingleUp" || DexcomTrend == "singleUp") {
      arrow_type = SINGLEUP;
      arrow_color = GREEN; 
      warn = 0;
    }
    if (DexcomTrend == "doubleUp" || DexcomTrend == "DoubleUp") {
      arrow_type = DOUBLEUP;
      arrow_color = GREEN; 
      warn = 0;
    }   
    if (DexcomTrend == "fortyFiveDown" || DexcomTrend == "FortyFiveDown") {
      arrow_type = FORTYFIVEDOWN;
    }   
    if (DexcomTrend == "singleDown" || DexcomTrend == "SingleDown") {
      arrow_type = SINGLEDOWN;
    }   
    if (DexcomTrend == "doubleDown" || DexcomTrend == "DoubleDown") {
      arrow_type = DOUBLEDOWN;
    }  
  }

  if (FormattedDex == 0){
    arrow_type = XXX;
    arrow_color = RED; 
    warn = 0;
    warnLittleDown = 1;
  }

  if(!digitalRead(DIM_PIN)){
    mutetimer = mutetimer + 1000; 
  }

  mutetimer = mutetimer - 1;
  if(mutetimer < 0){mutetimer = 0;}
  if(mutetimer > (MUTE_TIMEOUT * (60000 / DISPLAY_LOOP_TIMER))){mutetimer = MUTE_TIMEOUT * (60000 / DISPLAY_LOOP_TIMER);}
  if(mutetimer > 1){mute = 1;}
  else{mute = 0;}

  if(analogRead(LDR_PIN) > (1000 - brightOn * LDR_HYSTERESIS)){
    brightOn = 1;
    BRIGHT = analogRead(LDR_PIN) / 30;
    if(BRIGHT > 184){BRIGHT = 184;}
    matrix->SetBrightness(BRIGHT);
    matrix->setTextColor(RED);
    if(FormattedDex > IN_RANGE){matrix->setTextColor(GREEN);}
    if(FormattedDex > HIGH){matrix->setTextColor(YELLOW);}
    if(FormattedDex > VERY_HIGH){matrix->setTextColor(RED);}  
  }
  
  else{
    brightOn = 0;
    arrow_color = RED;
    BRIGHT = 1;
    matrix->SetBrightness(BRIGHT);
    matrix->setTextColor(YELLOW);
    if(FormattedDex > IN_RANGE){matrix->setTextColor(RED);}
  }

  matrix->fillScreen(0);  //Turn off all the LEDs
  matrix->setCursor(0, 1);
  matrix->print(FormattedDex, 0);
  matrix->drawBitmap(24, 1, arrows[arrow_type], 8, 7, arrow_color);

  if (mute == 1){
    int foo = (mutetimer / 600) + 1; //this 600 hardcode should probably be 60000 / display loop timer
    for(int i = 0; i < foo; i++){
      matrix->drawPixel(i, 0, RED); //could change this to use onboard neopixel to light button bezel instead
    }
  }
  if (warn && (mute == 0)){
    matrix->SetBrightness(255);
    for(int i = 0; i <32; i++){
      matrix->drawPixel(i,0,WHITE);   
    }
    if(blink){tone(SPKR_PIN, 440, DISPLAY_LOOP_TIMER / 2);}
  }
  if (FormattedDex == 0){
    matrix->setTextColor(RED);
    matrix->fillScreen(0);
    matrix->setCursor(1, 1);
    matrix->print("SRVR");
  }
  if(blink){matrix->drawPixel(31,0,RED);}
  matrix->Show();
}
