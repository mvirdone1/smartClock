// Overall design goal
// Display time on a display
// Time will be tracked remotely versus using a local RTC - This requires a network connection
// Example(s):
// https://playground.arduino.cc/Code/NTPclient
// Using amazon ESP8266 board
// https://www.amazon.com/gp/product/B010N1SPRK/ 


/************************************************************/
// Application Specific
/************************************************************/

#define UTC_OFFSET -4  // -5 is EST


#define MILITARY_TIME 0 // 1 = 24 hour, 0 = 12 hour

// TODO: Need to take time-zone into account -- DONE
// TODO: Military versus standard time -- DONE
// TODO: DST
// TODO: Dealing with stale NTP time so that the clock isn't wrong
// TODO: Fix alignment of characters

#define BRIGHTNESS 1 // Max is 15 in the MD_MAX72xx.h library, default is Max/2


/************************************************************/
// Network
/************************************************************/

// Required for network connectivity
#include <ESP8266WiFi.h>


// Most importantly needs to define the 'ssid' and 'password' variables for connecting to your local network
#include "UserNetworkSettings.h"

/************************************************************/
// Time
/************************************************************/
// Project example from: https://www.geekstips.com/arduino-time-sync-ntp-server-esp8266-udp/

#include <WiFiUdp.h>

IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";
unsigned int localPort = 2390;      // local port to listen for UDP packets

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

WiFiUDP udp;						// A UDP instance to let us send and receive packets over UDP




/************************************************************/
// Display
/************************************************************/

// Design uses a 4x8x8 LED array using the MAX7219 I/O expander (e.g. https://www.amazon.com/gp/product/B01EJ1AFW8/)
// Downloaded on 8/29/18 from: https://github.com/MajicDesigns/MD_MAX72XX
#include <MD_MAX72xx.h>
#include <SPI.h>


// Code from an example at: https://github.com/MajicDesigns/MD_MAX72XX/blob/master/examples/MD_MAX72xx_PrintText/MD_MAX72xx_PrintText.ino
/****** <EXAMPLE> *****/

// Define the number of devices we have in the chain and the hardware interface

#include <MD_MAX72xx.h>
#include <SPI.h>

#define PRINT(s, v) { Serial.print(F(s)); Serial.print(v); }

// Define the number of devices we have in the chain and the hardware interface
// NOTE: These pin numbers will probably not work with your hardware and may
// need to be adapted
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW //PAROLA_HW
// Changed to FC16_HW for my amazondisplay

// #define MAX_DEVICES 8
#define MAX_DEVICES 4 // Changed from example

// Pinouts from here: https://learn.sparkfun.com/tutorials/esp8266-thing-hookup-guide/using-the-arduino-addon


#define CLK_PIN   14 // or SCK
#define DATA_PIN  13 // or MOSI
#define CS_PIN    15 // or SS

// WARNING - I connected these to the SCK/MOSI/SS pins, but really I needed HSCK/HMOSI/HSS pins!

// SPI hardware interface
MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
// Arbitrary pins
//MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// Text parameters
#define CHAR_SPACING  2 // pixels between characters

void printText(uint8_t modStart, uint8_t modEnd, char *pMsg)
// Print the text string to the LED matrix modules specified.
// Message area is padded with blank columns after printing.
{
    uint8_t   state = 0;
    uint8_t   curLen;
    uint16_t  showLen;
    uint8_t   cBuf[8];
    int16_t   col = ((modEnd + 1) * COL_SIZE) - 1;

    mx.control(modStart, modEnd, MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);

    do     // finite state machine to print the characters in the space available
    {
        switch (state)
        {
        case 0: // Load the next character from the font table
                // if we reached end of message, reset the message pointer
            if (*pMsg == '\0')
            {
                showLen = col - (modEnd * COL_SIZE);  // padding characters
                state = 2;
                break;
            }

            // retrieve the next character form the font file
            showLen = mx.getChar(*pMsg++, sizeof(cBuf) / sizeof(cBuf[0]), cBuf);
            curLen = 0;
            state++;
            // !! deliberately fall through to next state to start displaying

        case 1: // display the next part of the character
            mx.setColumn(col--, cBuf[curLen++]);

            // done with font character, now display the space between chars
            if (curLen == showLen)
            {
                showLen = CHAR_SPACING;
                state = 2;
            }
            break;

        case 2: // initialize state for displaying empty columns
            curLen = 0;
            state++;
            // fall through

        case 3:	// display inter-character spacing or end of message padding (blank columns)
            mx.setColumn(col--, 0);
            curLen++;
            if (curLen == showLen)
                state = 0;
            break;

        default:
            col = -1;   // this definitely ends the do loop
        }
    } while (col >= (modStart * COL_SIZE));

    mx.control(modStart, modEnd, MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}

void setup()
{
    // Display and serial terminal
    mx.begin();
    // mx.control(modStart, modEnd, MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
    Serial.begin(57600);
    Serial.print("\n[MD_MAX72XX Message Display]\nType a message for the scrolling display\nEnd message line with a newline");

    /***** Network and timing *****/
    // We start by connecting to a WiFi network
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password); // Defined inside of UserNetworkSettings.h

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");

    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    Serial.println("Starting UDP");
    udp.begin(localPort);
    Serial.print("Local port: ");
    Serial.println(udp.localPort());

    // Application Code
    mx.control(0, MAX_DEVICES - 1, MD_MAX72XX::INTENSITY, BRIGHTNESS);
}

void loop()
{
    
    delay(5000); // 5 second delay between requests
    // According to this, minimum sampling time should be 4 seconds: https://tf.nist.gov/tf-cgi/servers.cgi

    /* Time */
    // Request the host by name and get the server assignment out of the server pool
    WiFi.hostByName(ntpServerName, timeServerIP);

    

    

    sendNTPpacket(timeServerIP); // send an NTP packet to a time server
                                 // wait to see if a reply is available

    
    
    

    int cb = udp.parsePacket();
    if (!cb) {
        Serial.println("no packet yet");
    }
    else {
        Serial.print("packet received, length=");
        Serial.println(cb);
        // We've received a packet, read the data from it
        udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

                                                 //the timestamp starts at byte 40 of the received packet and is four bytes,
                                                 // or two words, long. First, esxtract the two words:

        unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
        unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
        // combine the four bytes (two words) into a long integer
        // this is NTP time (seconds since Jan 1 1900):
        unsigned long secsSince1900 = highWord << 16 | lowWord;
        Serial.print("Seconds since Jan 1 1900 = ");
        Serial.println(secsSince1900);

        // now convert NTP time into everyday time:
        Serial.print("Unix time = ");
        // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
        const unsigned long seventyYears = 2208988800UL;
        // subtract seventy years:
        unsigned long epoch = secsSince1900 - seventyYears;
        // print Unix time:
        Serial.println(epoch);


        // print the hour, minute and second:
        Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
        Serial.print((epoch % 86400L) / 3600); // print the hour (86400 equals secs per day)
        Serial.print(':');
        if (((epoch % 3600) / 60) < 10) {
            // In the first 10 minutes of each hour, we'll want a leading '0'
            Serial.print('0');
        }
        Serial.print((epoch % 3600) / 60); // print the minute (3600 equals secs per minute)
        Serial.print(':');
        if ((epoch % 60) < 10) {
            // In the first 10 seconds of each minute, we'll want a leading '0'
            Serial.print('0');
        }
        Serial.println(epoch % 60); // print the second


        /***********************************/
        // Application Code
        /***********************************/

        // Create hour and minute integers
        int hour = ((epoch % 86400L) / 3600) + UTC_OFFSET;
        int minute = ((epoch % 3600) / 60);

        // Correct hours for both time zones and military time
        // Fix time zone problems - if we have a negative time, add 24 hours to get us positve again
        if (hour < 0)
        {
            hour += 24;
        }

        // Civililian time adjustment
        if (MILITARY_TIME == 0)
        {
            // See if we're in the afternoon, if so, correct for the 12 hour offset
            if (hour > 13)
            {
                hour -= 12;
            }

            // See if we're at zero-hundred, if so, make it 12 midnight
            if (hour == 0)
            {
                hour = 12;
            }
        }
        
        // Create strings for display
        String hourString = String(hour);
        // Account for character alignment and/or military zero time
        if (hour < 10)
        {
            // If less than 10, and military time add a leading zero
            if (MILITARY_TIME == 1)
            {
                hourString = String('0' + hourString);
            }
            // If less than 10, and not military time, add a leading space
            else
            {
                hourString = String(' ' + hourString);
            }


        }

        String minuteString = String(minute);
        if (minute < 10)
        {
            minuteString = String('0' + minuteString);
        }

        // Create our final formatted string
        String timeString = String(hourString + ":");
        timeString = String(timeString + minuteString);

        // Print our string to the serial terminal
        Serial.print("Local Time: ");
        Serial.println(timeString);

        // Create a char array for displaying on the LED display
        char displayChars[10];
        timeString.toCharArray(displayChars, 10);

        // Print the display
        printText(0, MAX_DEVICES - 1, displayChars);


        
        /***********************************/
        // END Application Code
        /***********************************/

        


        
    }
    // wait ten seconds before asking for the time again
    delay(10000);
}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address)
{
    Serial.println("sending NTP packet...");
    // set all bytes in the buffer to 0
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    // Initialize values needed to form NTP request
    // (see URL above for details on the packets)
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    packetBuffer[1] = 0;     // Stratum, or type of clock
    packetBuffer[2] = 6;     // Polling Interval
    packetBuffer[3] = 0xEC;  // Peer Clock Precision
                             // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;

    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
    udp.beginPacket(address, 123); //NTP requests are to port 123
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();
}