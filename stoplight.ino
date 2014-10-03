/*
Filename:  stoplight.ino
Author:    Chad
Created:   Sep 29, 2014
Purpose:   This circuit controls 3 LEDs based on internet connectivity. Green if there is internet, Red if there is not, and Yellow while reverifying
            It also tracks this information so we can calculate lost time
Layout:    Ethernet shield requires pins Analog 10,11,12,13. LEDs are using Analog 4,6,8
*/

// ***** IMPORTED LIBRARIES ***** //
#include <SPI.h>        
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <ICMPPing.h>
#include <Time.h>

// ***** GLOBAL VARIABLES ***** //
// LEDS
int red = 4;
int yellow = 6;
int green = 8;

// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = {0xDE,0xAD,0xBE,0xEF,0xFE,0xED};
byte ip[] = {X,X,X,X}; // ip address for ethernet shield
unsigned int localPort = 8888;      // local port to listen for UDP packets
IPAddress timeServer(132, 163, 4, 101); // time-a.timefreq.bldrdoc.gov NTP server
// IPAddress timeServer(132, 163, 4, 102); // time-b.timefreq.bldrdoc.gov NTP server
// IPAddress timeServer(132, 163, 4, 103); // time-c.timefreq.bldrdoc.gov NTP server
const int NTP_PACKET_SIZE= 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
EthernetUDP Udp;  // A UDP instance to let us send and receive packets over UDP
IPAddress pingAddr(8,8,8,8); // ip address to ping
SOCKET pingSocket = 0;
ICMPPing ping(pingSocket, (uint16_t)random(0, 255));
int counter = 0;
int timeDown = 0;


// ***** RUN-ONCE SETUP LOOP ***** //
void setup()
{
 // Open serial communications and wait for port to open:
  Serial.begin(9600);

  // start Ethernet and UDP
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    Serial.println("Using hardcoded static IP");
    // configure using static IP
    Ethernet.begin(mac, ip);
  }
  // RED LED
  pinMode(red, OUTPUT);
  // YELLOW LED
  pinMode(yellow, OUTPUT);
  // GREEN LED
  pinMode(green, OUTPUT);
  
  Udp.begin(localPort);
  
  fetchTime();
}


// ***** LED CONTROL ***** //    
void loop()
  {
    ICMPEchoReply echoReply = ping(pingAddr, 4);
    if (echoReply.status == SUCCESS)
      {
        digitalWrite(yellow, LOW);    // set the yellow LED off
        delay(1000);
        digitalWrite(green, HIGH);    // set the green LED on
      }
    else
      {
        int start = now();
        // Make sure yellow is off, set green off, and turn red on
        digitalWrite(yellow, LOW);
        digitalWrite(green, LOW);
        delay(1000);
        digitalWrite(red, HIGH);

        counter = 0;
        // Loop until we have had 3 successful pings in a row
        while (counter < 3)
        {
          delay(20000);
          ICMPEchoReply echoReply = ping(pingAddr, 4);
          
          // One successful ping will change to yellow, any failure will keep/change it to red
          if (echoReply.status == SUCCESS)
          {
            digitalWrite(red, LOW);
            delay(1000);
            digitalWrite(yellow, HIGH);
            counter = counter + 1;
          }
          else
          {
            digitalWrite(yellow, LOW);
            delay(1000);
            digitalWrite(red, HIGH);
            counter = 0;
          }
        }
        timeDown = timeDown + (now() - start);
        printTime();
        printDay();
        Serial.print("Been down for: ");
        if (timeDown > 3600)
        {
          Serial.print(hour(timeDown));
          Serial.print(" hours, ");
        }
        Serial.print(minute(timeDown));
        Serial.print(" minutes, ");
        Serial.print(second(timeDown));
        Serial.println(" seconds.");
      }

  }

// ***** FETCH NETWORK TIME ***** //    
void fetchTime()
{
  sendNTPpacket(timeServer); // send an NTP packet to a time server

    // wait to see if a reply is available
  delay(1000);  
  if ( Udp.parsePacket() )
  {
    // We've received a packet, read the data from it
    Udp.read(packetBuffer,NTP_PACKET_SIZE);  // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, extract the two words:
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);  
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;  
    const unsigned long seventyYears = 2208988800UL;    
    // subtract seventy years and 4 hours for EST:
    unsigned long epoch = secsSince1900 - seventyYears - (60 * 60 * 4);
    setTime(epoch);
  }
}

// ***** PRINT TIME ***** //    
void printTime()
{
  Serial.print("The time is ");
  Serial.print(hour());
  Serial.print(':');  
  if ( minute() < 10 ) {
    Serial.print('0');
  }
  Serial.print(minute());
  Serial.print(':');
  if ( second() < 10 ) {
       Serial.print('0');
  }
  Serial.println(second());
}


// ***** PRINT DAY ***** //    
void printDay()
{

  Serial.print("The day is ");
  Serial.print(day());
  Serial.print('/');  
  Serial.print(month());
  Serial.print('/');
  Serial.println(year());

}
// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address)
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
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer,NTP_PACKET_SIZE);
  Udp.endPacket();
}
