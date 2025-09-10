# WiFi Waterbed Heater 3 (final)

This is the 3rd and final iteration of this. The GUI is based on my [Universal Remote](https://github.com/CuriousTech/Smart-Universal-Remote) so it has a multi-tile swap gesture interface using the
[WaveShare-ESP32-S3-Touch-LCD-2.8](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-2.8).  
  
![wb3](http://curioustech.net/images/wb3.jpg)  
  
The small breakout board is not really needed and uses more space since the only component required is a 4K7 resistor to pull up the DS18B20 signal line, but I already have connectors for everything from the old project. So from left to right: DS18B20, power/slave, LD2410, and AM2320/AM2322 vertically.  The i2c lines have internal pullups.  
  
![WBBOB](http://curioustech.net/images/wbbob.jpg)  
  
The slave board and 3D files from the WB2 project is reposted here as well.  
  
![3D](http://curioustech.net/images/wbcase2.jpg)  
  
  
The web page. Same as previous, but allows drag and drop files over the lower file display areas. The internal files are only used for click and notifications sounds so far. It's just easier than setting up to upload the FFat.  Although SPIFFS or LittleFS can be used instead very easily. The SD card is used for the MP3 player.  
  
![Web](http://curioustech.net/images/wb3web.png)  
  
Building: The ino explains the common settings for the IDE like my other projects. I use the Windows Store version because it works best for me. If a library is missing, the error should show a line with a link to where to get it. If not, complain.  
  
A little explanation of the HLK-LD2450 reader: The bottom area of the page represents the detection area. The 5000 (mm) is the X (signed X/2) and Y cutoff. Anything beyond that is zone 2 (outside the room). Zone 2 is also anything painted grey (this sees through walls). This area means lights/fan off. Zome 1 is in the room/lights on and fan off, motion or not. Latch area switches to zone 0/light off and fan on. Hold area is still in bed/zone 0 if latched.  
  
