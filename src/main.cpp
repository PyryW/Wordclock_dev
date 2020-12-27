#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <FastLED.h>
#include <array>

#define NUM_LEDS  140
#define LED_PIN   4
CRGB leds[NUM_LEDS];
#define BRIGHTNESS 128

uint8_t currentHue = 32;

int mainClock = 20;     //Delay between updates, 20ms = 50fps

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

int fadeSpeed = 100;                // Fade on/off milliseconds

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
  Serial.println("Wordclock v0.3");
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


int round5(int in) {               //00  01  02 03 04
  const signed char round5delta[5] = {0, -1, -2, 2, 1};  // difference to the "rounded to nearest 5" value
  int rounded = in + round5delta[in%5];     //hh:38%5 = 3 -> 38 + 2 = 40    
  return rounded;
}


void colorCycle() {
  static bool dirUp = true;
  static unsigned long lastUpdate = 0;

  unsigned long now = millis();

  for (int i = 0; i < NUM_LEDS; i++) {
    if (activeLEDs[i]) { leds[i] = CHSV(currentHue, 255, 255); }
  }

  if (dirUp) {
    currentHue++;
  } else {
    currentHue--;
  }

  if (dirUp && currentHue == 32) {
    dirUp = false;
  } else if (!dirUp && currentHue == 192) {
    dirUp = true;
  }
}


void fade() {
  static uint8_t fadeHi = 255;  //Starting value of fade high to low
  static uint8_t fadeLo = 0;    //Starting value of fade low to hight


  //Fadedown, high to low
  if (fadeHi != 0) {
    fadeHi--;
    for (int i = 0; i < NUM_LEDS; i++) {
      if (!activeLEDs[i] && tmpLEDs[i]) { //If currently off, previously on
        leds[i] = CHSV(currentHue, 255, fadeHi);
      }
    } 
  } else {
    fadeHi = 255;  //If fadeHi reaches 00, then the fadedown is complete, and fadeHi can be returned to 255 for the next fade.
  }

  //Fadeup, low to high
  if (fadeLo != 255) {
    fadeLo++;
    for (int i = 0; i < NUM_LEDS; i++) {
      if (activeLEDs[i] && !tmpLEDs[i]) { //if currently on, previously off
        leds[i] = CHSV(currentHue, 255, fadeLo);
      }
    }
  } else {
    fadeLo = 0;  //If fadeLo reaches 255, then the fadeup is complete, and fadeLo can be returned to 0 for the next fade.
  }

  //If both the fadeup and fadedown have completed, then set fade pending to false to stop calling fade()
  if (fadeHi == 255 && fadeLo == 0) {  
    fadePending = false;
    Serial.println("Fade now false");
  }
}

void loop() {
  static unsigned long lastUpdate = 0;
  unsigned long now = millis();
  static time_t prevDisplay = 0;

  if (now > lastUpdate + mainClock) {
    if (timeStatus() != timeNotSet) {
      if (minute() != prevDisplay) {  //Refresh time only if the minute has changed
        prevDisplay = minute();
        refreshTime();
      }
    }

    if (now > lastUpdate + colorCycleSpeed && effectMode == 1) { colorCycle(); }
    if (fadePending) { fade(); }

    lastUpdate = now;
    FastLED.show();
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

const int words[][2] = {
  {0, 4},       //Kello
  {6, 7},       //On
  {9, 13},      //Puoli
  {50, 55},     //Vaille
  {45, 49},     //Tasan
  {42, 44}      //Yli
};

const int minutePos[][2] = {
  {9, 13},      //Puoli
  {0, 0},       //Placeholder
  {15, 27},     //Kahtakymmentä
  {28, 34},     //Varttia
  {15, 22},     //Kymmentä
  {36, 41}      //Viittä
};

const int hourPos[][2] = {
  {0, 0},       //Placeholder, there is no hour 0 in 12-hour clock
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
  uint8_t nowHour = hourFormat12();
  uint8_t roundMinute = round5(minute());

  if(showTimeIs) {
    show(words[0]);  //Kello
    show(words[1]);  //On
  }

  int helperMinute = abs( (roundMinute - 30)/5 ); //E.g. roundedMinute = 20, helperMinute = |(20-30)/5| = |-2| = 2, minutePos[2] = Kahtakymmentä

  if (roundMinute%60 == 0) {      //hh:00 
    show(words[4]);               //Tasan
    if (roundMinute == 60) {      //hh:58 or hh:59
      show(hourPos[nowHour+1]);   //Current hour+1, because 5:58 = 6:00
    } else {                      //hh:00, hh:01, or hh:02
      show(hourPos[nowHour]);     //Current hour
    }
  } else {
    if(helperMinute != 1) {       //helperMinute 1 = 25 minutes, which is a special case due to being split on two lines
      show(minutePos[helperMinute]);  
    } else {
      show(minutePos[2]);         //Kahtakymmentä
      show(minutePos[5]);         //Viittä
    } 

    if (roundMinute < 30) {
      show(words[5]);             //Yli
      show(hourPos[nowHour]);
    } else if (roundMinute >= 30) {
      if (helperMinute != 0) {
        show(words[3]);           //Vaille
      }
      show(hourPos[nowHour+1]);   //Current hour+1, because 5:45 = quarter to six, not quarter to five
    }
  } 

  if (activeLEDs != tmpLEDs) {    //If words to be displayed has changed, activate the fade effect
    fadePending = true;
  }
}