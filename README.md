
# THIS NEEDS TO BE UPDATED. LOTS OF CHANGES

Main changes from TooSpeedy repo:
  
  	Converted to US standard units
  
  	Light sensor
  
  	Small speaker
  
  	Button on the top to mute said speaker
  
  	Colors used dependent on glucose state
  
  	Will light up the entire top row of LEDs full white when in an alert state. Helps wake up from sleep.

    Converted to https://github.com/aligator/NeoPixelBusGfx library from Adafruit Neopixel library. The Adafruit library does some assembly bit banging of the data with some interrupt blocking stuff that appears to mess with the ESP32, especially with the wifi. The NeoPixelBus library uses the remote peripheral in the ESP32 to be able to asynchronously send the LED data. 

    Along with the above, changed the data structure of the arrows to be a bit easier to visually edit.

Based on the light sensor, it  has two modes; day time and night time. With the day time mode green means good. Once it goes into night time mode, it's as dim as possible red when everything is good so as to disturb sleep the least. As things get "worse" it goes to amber and then adds the white bar. It also turns on an audible alarm.

The mute button on top turns on red LEDs on the top row from left to right, based on how long you held it for. It slowly turns off the LEDs from right to left as the timer runs out. Each red LED is worth a minute of mute.

I also added a bunch of watchdog type stuff, and refactored a lot of the code.

TODO:

    Look into why device sometimes will bootloop with all white LEDs. Suspect need to play around with pulldown on LED dataline. Worst case need to add NFET to GND of LED array supply and manually turn on when ready 

    Observe operation when dexcom server is down, reading is "LOW" or "HIGH", and also http error messages.

    Potentially add wifi registration, although without a method to put login for dexcom perhaps that is useless. Maybe make device enumerate as USB storage and have a config text file that contains wifi and dexcom login stuff. 
