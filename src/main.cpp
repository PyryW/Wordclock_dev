#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <FastLED.h>
#include <array>

#define NUM_LEDS  140
#define LED_PIN   4
CRGB leds[NUM_LEDS];
#define BRIGHTNESS 128

int cycleHue = 32;

int mainClock = 10;

bool fadePending = false;

std::array<bool, NUM_LEDS> activeLEDs;
std::array<bool, NUM_LEDS> tmpLEDs;

#pragma region Userconfig
/*------- Userconfig --------*/

const char ssid[] = "IoToto";       //Wi-Fi Name
const char pass[] = "oweckman";     //Wi-Fi Password 

const int timeZone = 2;             // 2 = EET, GMT+2    3 = EEST, GMT+3

bool showTimeIs = true;             // Show "Kello on"

int effectMode = 1;                 // 1 = Colorcycle
int colorCycleSpeed = 100;          // Milliseconds between hue increments

int fadeSpeed = 100;                 // Fade on/off milliseconds

/*---------------------------*/
#pragma endregion

#pragma region NTP Code

WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

/*-------- NTP code ----------*/

static const char ntpServerName[] = "us.pool.ntp.org";  // NTP Server address
time_t getNtpTime();
void refreshTime();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
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
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

#pragma endregion NTP

void setup()
{

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS).setCorrection( TypicalSMD5050 );
  FastLED.setBrightness(BRIGHTNESS );
  FastLED.setMaxRefreshRate(120);

  FastLED.clear();
  fill_rainbow(leds, NUM_LEDS, 0, 255/NUM_LEDS);
  FastLED.show();

  activeLEDs.fill(false);

  Serial.begin(115200);
  delay(250);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);

  int hue = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(50);
    hue += 5;
    fill_rainbow(leds, NUM_LEDS, hue, 1);
    FastLED.show();
  }

  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);

  FastLED.clear();

}

time_t prevDisplay = 0; // when the digital clock was displayed

int currentMinute;
int roundMinute;
int nowHour;


int round5(int in) {
  const signed char round5delta[5] = {0, -1, -2, 2, 1};  // difference to the "rounded to nearest 5" value
  int rounded = in + round5delta[in%5];
  return rounded;
}
/*
void fade() {
  static unsigned long lastUpdate = 0;
  static uint8_t fadeHi = 255;
  static uint8_t fadeLo = 0;

  unsigned long now = millis();

    //Read arrays of now active LEDs, and compare to previously active LEDs
  if (now > lastUpdate + fadeSpeed) {

    if (fadeHi != 0) {
      fadeHi--;
      for (int i = 0; i < NUM_LEDS; i++) {
        if (!activeLEDs[i] && tmpLEDs[i]) { //If currently off, previously on
          leds[i] = CHSV(cycleHue, 255, fadeHi);
          Serial.println("turning led off");
        }
      } 
      Serial.print("Decreased val by one, now ");
      Serial.println(fadeHi);
      } else {
        fadeHi = 255;
    }

    if (fadeLo != 255) {
      fadeHi++;
      for (int i = 0; i < NUM_LEDS; i++) {
        if (activeLEDs[i] && !tmpLEDs[i]) { //if currently on, previously off
          leds[i] = CHSV(cycleHue, 255, fadeLo);
          Serial.println("turning led on");
        }
      }
    Serial.print("Increased val by one, now ");
    Serial.println(fadeLo);
    } else {
      fadeHi = 0;
    }
    FastLED.show();
    lastUpdate = now;
  }

  if (fadeHi == 255 && fadeLo == 0) {
    fadePending = false;
  }


}
*/

void colorCycle() {
  static unsigned long lastUpdate = 0;
  static bool dirUp = true;
  static uint8_t fadeHi = 255;
  static uint8_t fadeLo = 0;

  unsigned long now = millis();

  if (now > lastUpdate + mainClock) {
    if (now > lastUpdate + colorCycleSpeed) {
      for (int i = 0; i < NUM_LEDS; i++) {
      if (activeLEDs[i]) {
        leds[i] = CHSV(cycleHue, 255, 255);
        }
      }
      lastUpdate = now;

      if (dirUp && cycleHue == 32) {
        dirUp = false;
        cycleHue--;
      } else if (!dirUp && cycleHue == 192) {
        dirUp = true;
        cycleHue++;
      }

      // TODO: Use uint for automatic overflow

      
      if (dirUp) {
        cycleHue++;
        //Serial.print("Increased hue by one, now ");
        //Serial.println(cycleHue);
      } else {
        cycleHue--;
        //Serial.print("Decreased hue by one, now ");
        //Serial.println(cycleHue);
      }

      if (cycleHue == 0) {
        cycleHue = 255;
      } else if (cycleHue == 255) {
        cycleHue = 0;
      }
    }

    //Fadefunction
    //**************
    if (fadePending && activeLEDs != tmpLEDs) {

      if (fadeHi != 0) {
        fadeHi--;
        for (int i = 0; i < NUM_LEDS; i++) {
          if (!activeLEDs[i] && tmpLEDs[i]) { //If currently off, previously on
            leds[i] = CHSV(cycleHue, 255, fadeHi);
          }
        } 
      } else {
        fadeHi = 255;
      }

      if (fadeLo != 255) {
        fadeLo++;
        for (int i = 0; i < NUM_LEDS; i++) {
          if (activeLEDs[i] && !tmpLEDs[i]) { //if currently on, previously off
            leds[i] = CHSV(cycleHue, 255, fadeLo);
          }
        }
      } else {
        fadeLo = 0;
      }

      if (fadeHi == 255 && fadeLo == 0) {
        fadePending = false;
        Serial.println("Fade now false");
      }

    }
    //endfadefunction
    //****************

    FastLED.show();
  }

}
  

void loop() {

  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();
      
      nowHour = hourFormat12();
      roundMinute = round5(minute());

      if (currentMinute != minute()) {
        currentMinute = minute();
        refreshTime();
      }
    }
  }

  if (effectMode == 1) {
    colorCycle();
  }
}

/*
   0 1 2 3 4 5 6 7 8 9 10111213 
0  K E L L O - O N - P U O L I  PUOLI(9-13)
14 K A H T A K Y M M E N T Ä -  KAHTA(14-18)KYMMENTÄ(19-26)
28 V A R T T I A - V I I T T Ä  VARTTIA(28-34) VIITTÄ(36-41)
42 V A I L L E T A S A N Y L I  VAILLE(42-47) TASAN(48-52) YLI(53-55)
56 N E L J Ä - K Y M M E N E N  NELJÄ(56-60) KYMMENEN(62-69)
70 - - - K A K S I T O I S T A  KAKSI(73-77)TOISTA(78-83)
84 V I I S I - Y H D E K S Ä N  VIISI(84-88) YHDEKSÄN(90-97)
98 S E I T S E M Ä N K O L M E  SEITSEMÄN(98-106) KOLME(107-111)
112K U U S I K A H D E K S A N  KUUSI(112-116) KAHDEKSAN(117-125)
126- Y K S I T O I S T A - - -  YKSI(127-130)TOISTA(131-136)
*/

String sanat = "KELLO-ON-PUOLIKAHTAKYMMENTA-VARTTIA-VIITTAVAILLETASANYLINELJA-KYMMENEN---KAKSITOISTAVIISI-YHDEKSANSEITSEMANKOLMEKUUSIKAHDEKSAN-YKSITOISTA---";

const int words[][2] = {
  {0, 4},       //Kello
  {6, 7},       //On
  {9, 13},      //Puoli
  {50, 55},     //Vaille
  {45, 49},     //Tasan
  {42, 44}      //Yli
};

const int minutePos[][2] = {
  {9, 13},  //Puoli
  {0, 0},   //Placeholder
  {15, 27}, //Kahtakymmentä
  {28, 34}, //Varttia
  {15, 22}, //Kymmentä
  {36, 41}  //Viittä
};

const int hourPos[][2] = {
  {0, 0},       //Placeholder
  {135, 138},   //1
  {76, 80},     //2
  {98, 102},    //3
  {56, 60},     //4
  {84, 88},     //5
  {112, 116},   //6
  {103, 111},   //7
  {117, 125},   //8
  {90, 97},     //9
  {62, 69},     //10
  {129, 138},   //11
  {70, 80}      //12
};


void show(const int word[]) {
  for(int led=word[0]; led <= word[1]; led++) {
    activeLEDs[led] = true;
  }
}


void refreshTime() {
  tmpLEDs = activeLEDs;
  activeLEDs.fill(false);

  if(showTimeIs) {
    show(words[0]);  //Kello
    show(words[1]);  //On
  }

  int helperMinute = (roundMinute - 30)/5;

  if (roundMinute == 0) {
    show(words[4]); //Tasan
    show(hourPos[nowHour]);
  } else if (roundMinute == 60) {
    show(words[4]); //Tasan
    show(hourPos[nowHour+1]);  //Current hour+1, because 5:45 = quarter to six, not quarter to five
  } else {

    if(abs(helperMinute) != 1) {
      show(minutePos[abs(helperMinute)]); 
      //show(minutePos[abs(helperMinute)]); 
      //The absolute value of the rounded divided minute
      //E.g. roundedMinute = 20, helperMinute = |(20-30)/5| = |-2| = 2, minutePos[2] = Kahtakymmentä
    } else {
      show(minutePos[2]);
      show(minutePos[5]);
    } 

    if (roundMinute < 30) {
      show(words[5]);           //Yli
      show(hourPos[nowHour]);
    } else if (roundMinute >= 30) {
      show(words[3]);           //Vaille
      show(hourPos[nowHour+1]);
    }

  } 
  /*
  //Read arrays of now active LEDs, and compare to previously active LEDs
  for (int i = 0; i < NUM_LEDS; i++) {
    if (!activeLEDs[i] && tmpLEDs[i]) { //If currently off, previously on
      leds[i] = CHSV(0, 0, 0);
      //Todo: trigger dim function here
    } 
    else if (activeLEDs[i] && !tmpLEDs[i]) { //if currently on, previously off
      leds[i] = CHSV(cycleHue, 255, 255);
      //Todo: trigger dim function here
    }
  }
  */

  fadePending = true;
  Serial.println("Fade now true");

  FastLED.show();

}