//forked from https://github.com/TooSpeedy/Dexcom-ESP32-LED-Matrix/tree/main
//tested on esp32-S3, following settings:
//USB CDC On Boot: Enabled
// CPU Frequency: 240
// Core Debug level: none
// USB DFU on boot: enabled
// Erase all flash before sketch upload: enabled
// Events run on core: 1
// Flashmode: QIO 80Mhz
// Flash SIze: 16MB (128Mb)
// JTAG adapter: Disabled
// Arduino Runs On: Core 1
// USB Firmware MSC On boot: Enabled
// Partition Scheme: Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)
// PSRAM: Disabled
// Upload Mode: USB-OTG CDC (TinyUSB)
// Upload Speed: 921600
// USB Mode: USB-OTG (TinyUSB)



#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>
#include <esp_task_wdt.h> //ESP watchdog timer library
#include <Chrono.h> //Chrono library by Sofian Audry and Thomas Ouellet Fredericks

//Loop Timer Stuff-----------------------------------------------------------
  #define DISPLAY_LOOP_TIMER 100 //ms before loop starts again.
  #define CLIENT_LOOP_TIMER 100000
  #define MUTE_TIMEOUT 30 //how long the mute button will silence in minutes. Suggest 30 or less for 32 pixel wide display. If you want to go more than this, you will need to fix the display code to make 1 pixel = more than 1 minute.
  #define LDR_HYSTERESIS 300
  #define DEBOUNCE 10  // this many loops before it does something
  #define REAUTH_LOOP_TIMER 60000 // most of a day. Auth string will only last a day
  #define BLINK_TIME 1000
  #define WDT_TIMEOUT 5 //watchdog timer in seconds
  Chrono displayLoopTimer;
  Chrono clientLoopTimer;
  Chrono blinkTimer;
  Chrono reauthLoopTimer(Chrono::SECONDS);



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

// Change the pixel count for your board, this is using 8x32
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(32, 8, LED_PIN, NEO_MATRIX_BOTTOM + NEO_MATRIX_RIGHT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG, NEO_GRB + NEO_KHZ800);
//Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(32, 8, LED_PIN, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG, NEO_GRB + NEO_KHZ800);

//#define MMOL // 1 for mmol/l or 0 for mg/dl
//#define US // 1 for US 0 for international


int BRIGHT = 40;



// #define GOOD_R 0
// #define GOOD_G 1
// #define GOOD_B 0

// #define WARN_R 1
// #define WARN_G 1
// #define WARN_B 0

// #define BAD_R 1
// #define BAD_G 0
// #define BAD_B 0






const uint16_t colors[] = {
  matrix.Color(255, 0, 0), matrix.Color(0, 255, 0), matrix.Color(255, 255, 0), matrix.Color(0, 0, 255), matrix.Color(255, 0, 255), matrix.Color(0, 255, 255), matrix.Color(255, 255, 255)
};

#define RED 0
#define GREEN 1
#define YELLOW 2
#define BLUE 3
#define PURPLE 4
#define TEAL 5
#define WHITE 6



// Wifi Config
const char* ssid = "WIFI SSID";
const char* password = "WIFI PASSWORD";

// Decom URL - US use share1
const char* host = "https://share1.dexcom.com/ShareWebServices/Services/General/LoginPublisherAccountByName";

String authkey;




float FormattedDex;
String DexcomTrend;
//int firstRun = 0;
int mute = 0; 
int pushbutton = 0; //used as a counter that will count cycles so you can do different things with different length pushes, and also a debounce
long mutetimer = 0;
int blink = 0;

int brightOn = 1;

void setup() {
  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch
  esp_task_wdt_reset(); //reset watchdog timer

  analogReadResolution(12); //0-4095
  pinMode(DIM_PIN, INPUT_PULLUP);
  //digitalWrite(LED_PIN, LOW);
  tone(SPKR_PIN, 440, 100);
  matrix.begin();
  matrix.fillScreen(0);
  matrix.show();
  matrix.setTextWrap(false);
  matrix.setBrightness(5);
  matrix.setTextColor(colors[RED]);
  matrix.setCursor(1, 1);
  matrix.print("Wifi");
  matrix.show();
  tone(SPKR_PIN, 880, 100);
  //Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    esp_task_wdt_reset(); //reset watchdog timer
   // Serial.println("Connecting to WiFi...");
  }
  
 // Serial.println("Connected to WiFi");
  authenticate();
  reauthLoopTimer.restart();
  
  esp_task_wdt_reset(); //reset watchdog timer
  //delay(500);
  clientLoopTimer.add(CLIENT_LOOP_TIMER);//this makes sure things run straight away
  displayLoopTimer.add(DISPLAY_LOOP_TIMER); 
  esp_task_wdt_reset(); //reset watchdog timer

}
//int x = matrix.width();
int pass = 0;

void loop() {
  if(WiFi.status() != WL_CONNECTED) { 
    tone(SPKR_PIN, 880, 100);
    tone(SPKR_PIN, 440, 100);
    while (WiFi.status() != WL_CONNECTED) {
      matrix.setBrightness(50);  //            MIGHT WANT TO CHANGE THIS TO GET MORE AND MORE BRIGHT OVER TIME DEPENDING ON HOW FAR FROM IDEAL GLUCOSE
      matrix.setTextColor(colors[RED]);
      matrix.setCursor(1, 1);
      matrix.print("Wifi");
      matrix.show();
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
  esp_task_wdt_reset(); //reset watchdog timer
  //https.end();
}

void authenticate(){
  matrix.setTextColor(colors[RED]);
  matrix.fillScreen(0);
  matrix.setCursor(1, 1);
  matrix.print("Auth");
  matrix.show();

  HTTPClient https;
  String url = host;
  https.begin(url);
  https.addHeader("Content-Type", "application/json");
  https.addHeader("User-Agent", "Dexcom Share/3.0.2.11 CFNetwork/711.2.23 Darwin/14.0.0");
  https.addHeader("Accept", "application/json");
  int httpCode = 100;
  
  while(httpCode > 299 || httpCode < 200){
    // Credentials
    String json = "{\"accountName\":\"USERNAME\",\"applicationId\":\"d89443d2-327c-4a6f-89e5-496bbb0317db\",\"password\":\"PASSWORD\"}";
    httpCode = https.POST(json);
    if(httpCode > 299 || httpCode < 200){
      matrix.setTextColor(colors[RED]);
      matrix.fillScreen(0);
      matrix.setCursor(1, 1);
      matrix.print("E");
      matrix.print(httpCode);
      matrix.show();
      for(int i = 0; i < 10; i++){ // 1 second per loop. How many loops is how long it will wait before retrying.
        esp_task_wdt_reset(); //reset watchdog timer
        delay(1000);
      }
    }
  matrix.fillScreen(0);
  matrix.show();  
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
    int httpCode = https.GET();
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
    //  Serial.println(DexcomValue);
    //Serial.println(FormattedDex, 1);
    //Serial.println(DexcomTrend);
    //Serial.println(Testing);
}

void drawNumberArrow(){
int arrow_color = RED;
    int textcolor = RED;

    bool warn = 1; //change max brightness based on this, and make it flash.. might add second one for low only for audible alarm. output relay for light maybe? top row of LEDs white maybe?
    if (FormattedDex > VERY_HIGH){
      if (DexcomTrend == "Flat" || DexcomTrend == "flat") {
        arrow_color = YELLOW; 
        warn = 0;
      }
      if (DexcomTrend == "fortyFiveUp" || DexcomTrend == "FortyFiveUp") {
        warn = 0;
      }
      if (DexcomTrend == "SingleUp" || DexcomTrend == "singleUp") {
        warn = 0;
      }
      if (DexcomTrend == "doubleUp" || DexcomTrend == "DoubleUp") {
      }   
      if (DexcomTrend == "fortyFiveDown" || DexcomTrend == "FortyFiveDown") {
        arrow_color = GREEN; 
        warn = 0;
      }   
      if (DexcomTrend == "singleDown" || DexcomTrend == "SingleDown") {
        arrow_color = GREEN; 
        warn = 0;
      }   
      if (DexcomTrend == "doubleDown" || DexcomTrend == "DoubleDown") {
        arrow_color = GREEN; 
        warn = 0;
      }   
      }
    else if (FormattedDex > HIGH){
      if (DexcomTrend == "Flat" || DexcomTrend == "flat") {
        arrow_color = YELLOW; 
        warn = 0;
      }
      if (DexcomTrend == "fortyFiveUp" || DexcomTrend == "FortyFiveUp") {
        arrow_color = YELLOW; 
        warn = 0;
      }
      if (DexcomTrend == "SingleUp" || DexcomTrend == "singleUp") {
        warn = 0;
      }
      if (DexcomTrend == "doubleUp" || DexcomTrend == "DoubleUp") {
      }   
      if (DexcomTrend == "fortyFiveDown" || DexcomTrend == "FortyFiveDown") {
        arrow_color = GREEN; 
        warn = 0;
      }   
      if (DexcomTrend == "singleDown" || DexcomTrend == "SingleDown") {
        arrow_color = GREEN; 
        warn = 0;
      }   
      if (DexcomTrend == "doubleDown" || DexcomTrend == "DoubleDown") {
        arrow_color = YELLOW; 
        warn = 0;
      }   
      }
    else if (FormattedDex > IN_RANGE + ((HIGH - IN_RANGE) / 2)){ //on the high side of in range
        if (DexcomTrend == "Flat" || DexcomTrend == "flat") {
          arrow_color = GREEN; 
          warn = 0;
        }
        if (DexcomTrend == "fortyFiveUp" || DexcomTrend == "FortyFiveUp") {
          arrow_color = YELLOW; 
          warn = 0;
        }
        if (DexcomTrend == "SingleUp" || DexcomTrend == "singleUp") {
          warn = 0;
        }
        if (DexcomTrend == "doubleUp" || DexcomTrend == "DoubleUp") {
        }   
        if (DexcomTrend == "fortyFiveDown" || DexcomTrend == "FortyFiveDown") {
          arrow_color = GREEN; 
          warn = 0;
        }   
        if (DexcomTrend == "singleDown" || DexcomTrend == "SingleDown") {
          warn = 0;
        }   
        if (DexcomTrend == "doubleDown" || DexcomTrend == "DoubleDown") {
        }   
      }
    else if (FormattedDex > IN_RANGE){ //on the low side of in range
        if (DexcomTrend == "Flat" || DexcomTrend == "flat") {
          arrow_color = GREEN; 
          warn = 0;
        }
        if (DexcomTrend == "fortyFiveUp" || DexcomTrend == "FortyFiveUp") {
          arrow_color = GREEN; 
          warn = 0;
        }
        if (DexcomTrend == "SingleUp" || DexcomTrend == "singleUp") {
          warn = 0;
        }
        if (DexcomTrend == "doubleUp" || DexcomTrend == "DoubleUp") {
          warn = 0;
        }   
        if (DexcomTrend == "fortyFiveDown" || DexcomTrend == "FortyFiveDown") {
          arrow_color = YELLOW; 
          warn = 0;
        }   
        if (DexcomTrend == "singleDown" || DexcomTrend == "SingleDown") {
          warn = 0;
        }   
        if (DexcomTrend == "doubleDown" || DexcomTrend == "DoubleDown") {
        }   
      }
    else if (FormattedDex > LOW){ 
        if (DexcomTrend == "Flat" || DexcomTrend == "flat") {
          arrow_color = YELLOW; 
          warn = 0;
        }
        if (DexcomTrend == "fortyFiveUp" || DexcomTrend == "FortyFiveUp") {
          arrow_color = GREEN; 
          warn = 0;
        }
        if (DexcomTrend == "SingleUp" || DexcomTrend == "singleUp") {
          arrow_color = GREEN; 
          warn = 0;
        }
        if (DexcomTrend == "doubleUp" || DexcomTrend == "DoubleUp") {
          arrow_color = GREEN; 
          warn = 0;
        }   
        if (DexcomTrend == "fortyFiveDown" || DexcomTrend == "FortyFiveDown") {
        }   
        if (DexcomTrend == "singleDown" || DexcomTrend == "SingleDown") {
        }   
        if (DexcomTrend == "doubleDown" || DexcomTrend == "DoubleDown") {
        }   
      }
    else { //very low
      if (DexcomTrend == "Flat" || DexcomTrend == "flat") {
      }
      if (DexcomTrend == "fortyFiveUp" || DexcomTrend == "FortyFiveUp") {
        arrow_color = GREEN; 
        warn = 0;
      }
      if (DexcomTrend == "SingleUp" || DexcomTrend == "singleUp") {
        arrow_color = GREEN; 
        warn = 0;
      }
      if (DexcomTrend == "doubleUp" || DexcomTrend == "DoubleUp") {
        arrow_color = GREEN; 
        warn = 0;
      }   
      if (DexcomTrend == "fortyFiveDown" || DexcomTrend == "FortyFiveDown") {
      }   
      if (DexcomTrend == "singleDown" || DexcomTrend == "SingleDown") {
      }   
      if (DexcomTrend == "doubleDown" || DexcomTrend == "DoubleDown") {
      }  
    }



       if(!digitalRead(DIM_PIN)){
       //pushbutton = pushbutton + 1; 
      mutetimer = mutetimer + 1000; 
     }
     //else{pushbutton = pushbutton - 10;}
    // if(pushbutton > DEBOUNCE){pushbutton = DEBOUNCE;}
    // if(pushbutton < 0){pushbutton = 0;}
    // if(pushbutton = DEBOUNCE){
    //   mutetimer = 10; //MUTE_TIMEOUT * (60000 / DISPLAY_LOOP_TIMER);
    //   }
     mutetimer = mutetimer - 1;
     if(mutetimer < 0){mutetimer = 0;}
     if(mutetimer > (MUTE_TIMEOUT * (60000 / DISPLAY_LOOP_TIMER))){mutetimer = MUTE_TIMEOUT * (60000 / DISPLAY_LOOP_TIMER);}
     if(mutetimer > 1){mute = 1;}
     else{mute = 0;}


    if(analogRead(LDR_PIN) > (1000 - brightOn * LDR_HYSTERESIS)){
      brightOn = 1;
      BRIGHT = analogRead(LDR_PIN) / 30;
      if(BRIGHT > 184){BRIGHT = 184;}
      matrix.setBrightness(BRIGHT);
      matrix.setTextColor(colors[RED]);
      if(FormattedDex > IN_RANGE){
        matrix.setTextColor(colors[GREEN]);
        }
      if(FormattedDex > HIGH){
        matrix.setTextColor(colors[YELLOW]);
        }
      if(FormattedDex > VERY_HIGH){
        matrix.setTextColor(colors[RED]);
        }  
      }
    
    else{
      brightOn = 0;
      arrow_color = RED;
      BRIGHT = 1;
      matrix.setBrightness(BRIGHT);
      matrix.setTextColor(colors[YELLOW]);
      if(FormattedDex > IN_RANGE){
        matrix.setTextColor(colors[RED]);
        }
      }

    

    matrix.fillScreen(0);  //Turn off all the LEDs
    matrix.setCursor(0, 1);
    matrix.print(FormattedDex, 0);


    if (DexcomTrend == "Flat" || DexcomTrend == "flat") {
      matrix.drawPixel(29, 2, colors[arrow_color]);
      matrix.drawPixel(30, 3, colors[arrow_color]);
      matrix.drawPixel(31, 4, colors[arrow_color]);
      matrix.drawPixel(30, 4, colors[arrow_color]);
      matrix.drawPixel(29, 4, colors[arrow_color]);
      matrix.drawPixel(28, 4, colors[arrow_color]);
      matrix.drawPixel(27, 4, colors[arrow_color]);
      matrix.drawPixel(26, 4, colors[arrow_color]);
      matrix.drawPixel(30, 5, colors[arrow_color]);
      matrix.drawPixel(29, 6, colors[arrow_color]);
    }
    if (DexcomTrend == "fortyFiveUp" || DexcomTrend == "FortyFiveUp") {
      matrix.drawPixel(30, 1, colors[arrow_color]);
      matrix.drawPixel(30, 2, colors[arrow_color]);
      matrix.drawPixel(30, 3, colors[arrow_color]);
      matrix.drawPixel(30, 4, colors[arrow_color]);
      matrix.drawPixel(29, 1, colors[arrow_color]);
      matrix.drawPixel(28, 1, colors[arrow_color]);
      matrix.drawPixel(27, 1, colors[arrow_color]);
      matrix.drawPixel(29, 2, colors[arrow_color]);
      matrix.drawPixel(28, 3, colors[arrow_color]);
      matrix.drawPixel(27, 4, colors[arrow_color]);
      matrix.drawPixel(26, 5, colors[arrow_color]);
    }
    if (DexcomTrend == "SingleUp" || DexcomTrend == "singleUp") {
      //Serial.println("if statement works");
      matrix.drawPixel(29, 1, colors[arrow_color]);
      matrix.drawPixel(29, 2, colors[arrow_color]);
      matrix.drawPixel(29, 3, colors[arrow_color]);
      matrix.drawPixel(29, 4, colors[arrow_color]);
      matrix.drawPixel(29, 5, colors[arrow_color]);
      matrix.drawPixel(29, 6, colors[arrow_color]);
      matrix.drawPixel(28, 2, colors[arrow_color]);
      matrix.drawPixel(27, 3, colors[arrow_color]);
      matrix.drawPixel(30, 2, colors[arrow_color]);
      matrix.drawPixel(31, 3, colors[arrow_color]);
    }
    if (DexcomTrend == "doubleUp" || DexcomTrend == "DoubleUp") {
      matrix.drawPixel(29, 1, colors[arrow_color]);
      matrix.drawPixel(29, 2, colors[arrow_color]);
      matrix.drawPixel(29, 3, colors[arrow_color]);
      matrix.drawPixel(29, 4, colors[arrow_color]);
      matrix.drawPixel(29, 5, colors[arrow_color]);
      matrix.drawPixel(29, 6, colors[arrow_color]);
      matrix.drawPixel(28, 2, colors[arrow_color]);
      matrix.drawPixel(27, 3, colors[arrow_color]);
      matrix.drawPixel(30, 2, colors[arrow_color]);
      matrix.drawPixel(31, 3, colors[arrow_color]);
    }
    if (DexcomTrend == "fortyFiveDown" || DexcomTrend == "FortyFiveDown") {
      matrix.drawPixel(30, 6, colors[arrow_color]);
      matrix.drawPixel(29, 5, colors[arrow_color]);
      matrix.drawPixel(28, 4, colors[arrow_color]);
      matrix.drawPixel(27, 3, colors[arrow_color]);
      matrix.drawPixel(26, 2, colors[arrow_color]);
      matrix.drawPixel(30, 5, colors[arrow_color]);
      matrix.drawPixel(30, 4, colors[arrow_color]);
      matrix.drawPixel(30, 3, colors[arrow_color]);
      matrix.drawPixel(29, 6, colors[arrow_color]);
      matrix.drawPixel(28, 6, colors[arrow_color]);
      matrix.drawPixel(27, 6, colors[arrow_color]);
    }
    if (DexcomTrend == "singleDown" || DexcomTrend == "SingleDown") {
      matrix.drawPixel(29, 1, colors[arrow_color]);
      matrix.drawPixel(29, 2, colors[arrow_color]);
      matrix.drawPixel(29, 3, colors[arrow_color]);
      matrix.drawPixel(29, 4, colors[arrow_color]);
      matrix.drawPixel(29, 5, colors[arrow_color]);
      matrix.drawPixel(29, 6, colors[arrow_color]);
      matrix.drawPixel(28, 5, colors[arrow_color]);
      matrix.drawPixel(27, 4, colors[arrow_color]);
      matrix.drawPixel(30, 5, colors[arrow_color]);
      matrix.drawPixel(31, 4, colors[arrow_color]);
    }
    if (DexcomTrend == "doubleDown" || DexcomTrend == "DoubleDown") {
      matrix.drawPixel(29, 1, colors[arrow_color]);
      matrix.drawPixel(29, 2, colors[arrow_color]);
      matrix.drawPixel(29, 3, colors[arrow_color]);
      matrix.drawPixel(29, 4, colors[arrow_color]);
      matrix.drawPixel(29, 5, colors[arrow_color]);
      matrix.drawPixel(29, 6, colors[arrow_color]);
      matrix.drawPixel(28, 5, colors[arrow_color]);
      matrix.drawPixel(27, 4, colors[arrow_color]);
      matrix.drawPixel(30, 5, colors[arrow_color]);
      matrix.drawPixel(31, 4, colors[arrow_color]);
    }
    if (mute == 1){
      int foo = (mutetimer / 600) + 1; //this 600 hardcode should probably be 60000 / display loop timer
      for(int i = 0; i < foo; i++){
        matrix.drawPixel(i, 0, colors[RED]); //could change this to use onboard neopixel to light button bezel instead
      }
    }
    if (warn && (mute == 0)){
      matrix.setBrightness(255);
      for(int i = 0; i <32; i++){
        matrix.drawPixel(i,0,colors[WHITE]);   
      }
      if(blink){tone(SPKR_PIN, 440, DISPLAY_LOOP_TIMER / 2);}
    }
    if(blink){matrix.drawPixel(31,0,colors[RED]);}
    matrix.show();
}
