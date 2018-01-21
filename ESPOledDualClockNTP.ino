#include <ESP8266WiFi.h>
#include <Wire.h>  // Include Wire if you're using I2C
#include <SFE_MicroOLED.h>  // Include the SFE_MicroOLED library


#define PIN_RESET 255  //
#define DC_JUMPER 0  // I2C Addres: 0 - 0x3C, 1 - 0x3D

#include <TimeLib.h> 

#include <WiFiUdp.h>

const char* ssid = "**********";
const char* password = "*********";

// Create an instance of the server
// specify the port to listen on as an argument
WiFiServer server(80);

int power[8]={1,2,4,8,16,32,64,128};

// 64x48 display size

// 7 segment         A  B  C  D  E  F  G
// this is full display sideways digits.
//byte xcoords[7]= {59,35, 6, 0, 6,35,29};
//byte ycoords[7]= {14,38,38,14, 7, 7,14};
//byte xhoriz[7] = { 0, 1, 1, 0, 1, 1, 0};
//byte seg_width=4; byte seg_height=18;
// half size digits
byte xcoords[7]= { 4,20,20, 4, 0, 0, 4};
byte ycoords[7]= { 2, 6,26,42,26, 6,22};
byte xhoriz[7] = { 1, 0, 0, 1, 0, 0, 1};
byte seg_width=3; byte seg_height=12;
//  lookup val     1  2  4  8 16 32 64
// 7 segment       A  B  C  D  E  F  G

//    ---A---
//  |         |
//  F         B
//  |         |
//    ---G---
//  |         |
//  E         C
//  |         |
//    ---D---

// This lists which segments are active for which digits. (B0GFEDCBA)
int segment_lookup[10]={B00111111, B00000110, B01011011, B01001111, B01100110, 
                        B01101101, B01111101, B00000111, B01111111, B01100111};



unsigned int localPort = 2390;      // local port to listen for UDP packets

IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "0.uk.pool.ntp.org";
const int timeZone = 0;     // Grenwich Mean Time

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
time_t prevDisplay = 0; // when the digital clock was displayed

byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

int timeout;

MicroOLED oled1(255,0);
MicroOLED oled2(255,1);

void setup() {
  // put your setup code here, to run once:

  Serial.begin(115200);

  oled1.begin();
  oled1.clear(ALL);
  oled1.display();
  oled2.begin();
  oled2.clear(ALL);
  oled2.display();
  delay(1000);
  oled1.clear(PAGE);
  oled2.clear(PAGE);
  
  
  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  oled1.setFontType(0);
  oled1.setCursor(0,0);
  oled1.print("Wifi Connected");
  oled1.print(ssid);
  oled1.display();
  oled2.clear(PAGE);
  oled2.setFontType(0);
  oled2.setCursor(0,0);
  oled2.print(WiFi.localIP());
  oled2.display();
  
  
  // Print the IP address
  Serial.println(WiFi.localIP());  
  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  
}

int lastHour = -1;
int lastMin = -1;
int lastSec = -1;
int newHour, newMin, newSec;
int animStep=0;

void loop() {
 
  //get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP); 
  oled1.setFontType(3);
  oled2.setFontType(3);
  
  if (hour() != lastHour) {
    newHour=hour();
    oled1.clear(PAGE);
    oled1.setCursor(0,0);
    if (newHour/10 == lastHour/10) {
       Show_Digit(oled1,(newHour/10),0);
    } else {
      Animate_Digit(oled1, (newHour/10), (lastHour/10), 0, animStep);
    }
    Animate_Digit(oled1, (newHour%10), 30, animStep);
    
    Show_Colon(oled1, second()%2==0);
    oled1.display();
  } else 
  if (second() != lastSec) {
    newSec=second();
    oled1.clear(PAGE);
    Show_Digit(oled1,(lastHour/10),0);
    Show_Digit(oled1,(lastHour%10),30);
    Show_Colon(oled1,newSec%2==0);
    lastSec=newSec;
    oled1.display();
  }

  if (minute() != lastMin) {
    newMin=minute();
    oled2.clear(PAGE);
    oled2.setCursor(0,0);
    if (newMin/10 == lastMin/10) {
      Show_Digit(oled2,(newMin/10),0);
    } else {
      Animate_Digit(oled2,(newMin/10),(lastMin/10), 0,animStep);
    }
     Animate_Digit(oled2,(newMin%10),30, animStep);
     animStep++;
     if (animStep == 8) {
      animStep=0;
      lastMin=newMin;
      lastHour=newHour;
      Show_Digit(oled2,(newMin/10),0);
      Show_Digit(oled2,(newMin%10),30);
     }
    oled2.display();
  }

  delay (100);

}

void Show_Colon(MicroOLED &oled, boolean white) 
{  
    oled.setColor(white?WHITE:BLACK);
    oled.rectFill(58,12,4,4);
    oled.rectFill(58,32,4,4);
}

void Show_Digit(MicroOLED &oled,byte digit,byte offset)
{
  boolean segment_on=true;
  int segment;
  byte x,y,wid,len;
 //  foreach segment
   for (int seg = 0 ; seg <7 ; seg++ ) {
 //     determine if illuminated for this digit
      segment_on =  (segment_lookup[digit] & power[seg]) > 0;
 //     get xy coords
      x=xcoords[seg]+offset;
      y=ycoords[seg];
      wid=seg_width+xhoriz[seg]*seg_height;
      len=seg_width+(1-xhoriz[seg])*seg_height;

      if (segment_on) {
        oled.setColor(WHITE); 
      } else {
        oled.setColor(BLACK);
      }
      oled.rectFill(x,y,wid,len);
   }
}
void Animate_Digit(MicroOLED &oled,byte digit,byte offset, byte anim_step) 
{
  byte prev_digit=digit-1;
  if (digit==0) prev_digit=9;
  Animate_Digit(oled, digit, prev_digit, offset, anim_step);
}

void Animate_Digit(MicroOLED &oled,byte digit,byte prev_digit, byte offset, byte anim_step)
{
  Serial.print("Animating ");
  Serial.print(digit);
  boolean segment_on=true, animated=false;
  int segment;
  byte x,y,wid,len;
 //  foreach segment

  Serial.print("  Prev digit: ");
  Serial.print(prev_digit);
  Serial.print(" step: ");
  Serial.println(anim_step);
  
   for (int seg = 0 ; seg <7 ; seg++ ) {
      
 //     determine if illuminated for this digit
      segment_on =  (segment_lookup[digit] & power[seg]) > 0;
      animated = segment_on != ((segment_lookup[prev_digit] & power[seg]) > 0);
 //     get xy coords
      x=xcoords[seg]+offset;
      y=ycoords[seg];
      wid=seg_width+xhoriz[seg]*seg_height;
      len=seg_width+(1-xhoriz[seg])*seg_height;
      if (animated) {
        oled.setColor(BLACK);
        oled.rectFill(x,y,wid,len);
        oled.setColor(WHITE); 
        if (segment_on) {
          // turn on animation
          x=x+(xhoriz[seg]*((7-anim_step)));
          wid=wid-(xhoriz[seg]*(2*(7-anim_step)));
          y=y+((1-xhoriz[seg])*((7-anim_step)));
          len=len-((1-xhoriz[seg])*(2*(7-anim_step)));
        } else {
          // turn off animation
          x=x+(xhoriz[seg]*((anim_step)));
          wid=wid-(xhoriz[seg]*(2*(anim_step)));
          y=y+((1-xhoriz[seg])*((anim_step)));
          len=len-((1-xhoriz[seg])*(2*(anim_step)));        
        }
      } else {
        if (segment_on) {
          oled.setColor(WHITE); 
//          Serial.print(seg);
//          Serial.print(" ");
        } else {
          oled.setColor(BLACK);
        }
      }
      oled.rectFill(x,y,wid,len);
   }
//   Serial.println(); 
}

/*-------- NTP code ----------*/

time_t getNtpTime()
{
  while (udp.parsePacket() > 0) ; // discard any previously received packets
  //get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP); 
  Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:                 
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

