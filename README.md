
# THIS NEEDS TO BE UPDATED. LOTS OF CHANGES

Main changes from TooSpeedy repo:
  Converted to US standard units
  Light sensor
  Small speaker
  Button on the top to mute said speaker
  Colors used dependent on glucose state
  Will light up the entire top row of LEDs full white when in an alert state. Helps wake up from sleep.

Based on the light sensor, it  has two modes; day time and night time. With the day time mode green means good. Once it goes into night time mode, it's as dim as possible red when everything is good so as to disturb sleep the least. As things get "worse" it goes to amber and then adds the white bar. It also turns on an audible alarm.

The mute button on top turns on red LEDs on the top row from left to right, based on how long you held it for. It slowly turns off the LEDs from right to left as the timer runs out. Each red LED is worth a minute of mute.

I also added a bunch of watchdog type stuff, and refactored a lot of the code.

It still needs lots of tweaking, and there's some hardware instability. I need to look at the power supply... the native USB-C on the board I have just doesn't really cut it for the full display I think.


