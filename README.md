
# THIS NEEDS TO BE UPDATED. LOTS OF CHANGES

# Dexcom ESP

The objective of this project is to display Dexcom readings on an LED matrix. It uses an ESP32 and checks the Dexcom share endpoint for readings. It will auto update every "x" mins and display it on an always on LED matrix display.

I've only started workingo on this project so there is lots more to do.

![Alt text](assets/ESP32-Dexcom.jpg?raw=true "Title")

## Bits to Change

### WiFi Details

Change the SSID and Password for your WiFi. Change this on **Line 22** and **Line 23**.

```C+
  // Wifi Config
    const char* ssid = "YOUR WIFI SSID";
    const char* password = "YOUR WIFI PASSWORD";
```

### Dexcom Share URL

There are 2 URLS that are used for the endpoints. 

US  - https://share1.dexcom.com

UK / Europe - https://shareous1.dexcom.com

There are 2 places within the code that will need to be changed depending on where the Dexcom account is. **Line 26** and **Line 64**.

### Dexcom Credentials

In order for this  work, you will need to use your Clarity Credentials. Change this on **Line 46**.

```C+
  // Credentials 
  String json = "{\"accountName\":\"PUTYOURUSERNAMEHERE\",\"applicationId\":\"d89443d2-327c-4a6f-89e5-496bbb0317db\",\"password\":\"PUTYOURPASSWORDHERE\"}";
  ```
## Bits to check

### Board PIN

This is coded to use `PIN 5` however if you would like to use another pin change **Line 11**.

```C+
  #define PIN 5 // Data pin for LED Matrix
```

### Matrix Size

This is configured to use a board size of 8x32. Currently this hasn't been tested on anything else, however this can be changed on **Line 13**.

```C+
  Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(32, 8, PIN,
```


## Parts Used

[ESP32 Board](https://www.aliexpress.com/item/32959541446.html)

[8x32 Matrix](https://www.aliexpress.com/item/4001296811800.html)

1mm Clear PETG (Used for the screen)

Matte Black Smoke Tint Vinyl

A3 Paper

## Housing

To house the components I used the following STL:

[Awtrix Case](https://www.thingiverse.com/thing:2791276)

Due to not needing some of the holes etc... I modded the files to fit my needs.

## Acknowledgements

 - [Great information about the Dexcom EndPoints](https://gist.github.com/StephenBlackWasAlreadyTaken/adb0525344bedade1e25)

