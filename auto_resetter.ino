#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#define STASSID       "WiFi_SSID"
#define STAPSK        "WiFi_Pass"
#define LED_PIN       (2)
#define RELAY_PIN     (4)

// How long to keep the router off
#define RESET_PERIOD  (30000ul)

// Sync once an hour
#define SYNC_TIMEOUT  (60*60*1000)
// If time sync was not done for a long time, should blink LED to notify
#define SYNC_ERROR_HOURS  (48)
// Reset every day at RESET_HOUR:RESET_MINUTE. Currently at 2AM UTC
#define RESET_HOUR    (2)
#define RESET_MINUTE  (0)

const char* ssid = STASSID;  // your network SSID (name)
const char* pass = STAPSK;   // your network password

unsigned int localPort = 2390;  // local port to listen for UDP packets

/* Don't hardwire the IP address or we won't get the benefits of the pool.
    Lookup the IP address for the host name instead */
// IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
IPAddress timeServerIP;  // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";

const int NTP_PACKET_SIZE = 48;  // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[NTP_PACKET_SIZE];  // buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;


uint32_t epoch = 0;
uint32_t last_sync_timestamp = 0;
uint32_t reset_timestamp = 0;
uint32_t check_reset_timestamp = 0;
uint32_t led_blink_timestamp = 0;

void check_reset_time(){
  if((millis() - check_reset_timestamp) > 20000){
    uint32_t seconds_passed = (millis() - last_sync_timestamp)/1000;
    uint32_t current_epoch = epoch + seconds_passed;
    uint32_t hour = (current_epoch % 86400L) / 3600;
    uint32_t minute = (current_epoch % 3600) / 60;

    Serial.print("\nTime:");
    Serial.print(hour);
    Serial.print(":");
    Serial.println(minute);

    if((hour == RESET_HOUR) && (minute == RESET_MINUTE) && ((millis() - reset_timestamp) > 70000)){
      // Do the reset
      Serial.println("Resetting");
      digitalWrite(RELAY_PIN, HIGH);  
      delay(RESET_PERIOD);            
      digitalWrite(RELAY_PIN, LOW);
      Serial.println("Done");
      reset_timestamp = millis();
    }
    check_reset_timestamp = millis();
  }
}

void check_sync_ok(){
  uint32_t hours_not_synced = (millis() - last_sync_timestamp)/(60*60*1000);
  if(hours_not_synced > SYNC_ERROR_HOURS){
    // Time sync was too long ago. Should notify
    if((millis() - led_blink_timestamp) > 2000){
      digitalWrite(LED_PIN, HIGH);  
      delay(1000);            
      digitalWrite(LED_PIN, LOW);
      led_blink_timestamp = millis();
    }    
  }
}

void setup() {
  Serial.begin(115200);  
  Serial.println();

  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

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

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  if((last_sync_timestamp == 0) || ((millis() - last_sync_timestamp) > SYNC_TIMEOUT)){
    // get a random server from the pool
    WiFi.hostByName(ntpServerName, timeServerIP);

    sendNTPpacket(timeServerIP);  // send an NTP packet to a time server
    // wait to see if a reply is available
    delay(1000);

    int cb = udp.parsePacket();
    if (cb) {     
      last_sync_timestamp = millis();

      // We've received a packet, read the data from it
      udp.read(packetBuffer, NTP_PACKET_SIZE);  // read the packet into the buffer

      // the timestamp starts at byte 40 of the received packet and is four bytes,
      //  or two words, long. First, esxtract the two words:

      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
      // combine the four bytes (two words) into a long integer
      // this is NTP time (seconds since Jan 1 1900):
      unsigned long secsSince1900 = highWord << 16 | lowWord;

      // now convert NTP time into everyday time:
      // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
      const unsigned long seventyYears = 2208988800UL;
      // subtract seventy years:
      epoch = secsSince1900 - seventyYears;
      // print Unix time:

      // print the hour, minute and second:
      Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
      uint32_t hour = (epoch % 86400L) / 3600;
      Serial.print(hour);  // print the hour (86400 equals secs per day)
      Serial.print(':');

      uint32_t minute = (epoch % 3600) / 60;

      if (minute < 10) {
        // In the first 10 minutes of each hour, we'll want a leading '0'
        Serial.print('0');
      }
      
      Serial.print(minute);  // print the minute (3600 equals secs per minute)
      Serial.print(':');
      if ((epoch % 60) < 10) {
        // In the first 10 seconds of each minute, we'll want a leading '0'
        Serial.print('0');
      }
      Serial.println(epoch % 60);  // print the second
    }

  }

  check_reset_time();

  check_sync_ok();
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress& address) {
  Serial.println("sending NTP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;  // LI, Version, Mode
  packetBuffer[1] = 0;           // Stratum, or type of clock
  packetBuffer[2] = 6;           // Polling Interval
  packetBuffer[3] = 0xEC;        // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123);  // NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}
