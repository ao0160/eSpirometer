#include <WiFiClientSecure.h>
//#include <MQTTClient.h>
//#include <ArduinoJson.h>
#include <math.h>
#include <limits.h>
#include <pgmspace.h>

#include "esp_wpa2.h"
#include "WiFi.h"
#include "driver/i2s.h"
#include "soc/i2s_reg.h"
#include "time.h"
#include "freertos/queue.h"
#include "Freenove_WS2812_Lib_for_ESP32.h"
#include "credentials.h"

// LED
#define LEDS_COUNT  1
#define LEDS_PIN  18
#define CHANNEL   0
// Button
#define CAPTURE_BUTTON 32
#define BUTTON_THRESH 1
#define DEBOUNCE_US 250000
// I2S definitions
#define SAMPLE_RATE 44100
TickType_t xDelay = 32;
size_t bytes_read = 0;
esp_err_t err;

// Networking.
const char* server = "api.olvera-dev.com";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;
WiFiClientSecure net = WiFiClientSecure();

const i2s_port_t I2S_PORT = I2S_NUM_0;
int32_t sample = 0;
double output = 0;
double energy = 0;
double snap_one_second = 0;

Freenove_ESP32_WS2812 strip = Freenove_ESP32_WS2812(LEDS_COUNT, LEDS_PIN, CHANNEL, TYPE_RGB);
Freenove_ESP32_WS2812* p_strip = &strip;

static const char ODEV_CERT_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIF3jCCA8agAwIBAgIQAf1tMPyjylGoG7xkDjUDLTANBgkqhkiG9w0BAQwFADCB
iDELMAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0pl
cnNleSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNV
BAMTJVVTRVJUcnVzdCBSU0EgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTAw
MjAxMDAwMDAwWhcNMzgwMTE4MjM1OTU5WjCBiDELMAkGA1UEBhMCVVMxEzARBgNV
BAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNleSBDaXR5MR4wHAYDVQQKExVU
aGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMTJVVTRVJUcnVzdCBSU0EgQ2Vy
dGlmaWNhdGlvbiBBdXRob3JpdHkwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIK
AoICAQCAEmUXNg7D2wiz0KxXDXbtzSfTTK1Qg2HiqiBNCS1kCdzOiZ/MPans9s/B
3PHTsdZ7NygRK0faOca8Ohm0X6a9fZ2jY0K2dvKpOyuR+OJv0OwWIJAJPuLodMkY
tJHUYmTbf6MG8YgYapAiPLz+E/CHFHv25B+O1ORRxhFnRghRy4YUVD+8M/5+bJz/
Fp0YvVGONaanZshyZ9shZrHUm3gDwFA66Mzw3LyeTP6vBZY1H1dat//O+T23LLb2
VN3I5xI6Ta5MirdcmrS3ID3KfyI0rn47aGYBROcBTkZTmzNg95S+UzeQc0PzMsNT
79uq/nROacdrjGCT3sTHDN/hMq7MkztReJVni+49Vv4M0GkPGw/zJSZrM233bkf6
c0Plfg6lZrEpfDKEY1WJxA3Bk1QwGROs0303p+tdOmw1XNtB1xLaqUkL39iAigmT
Yo61Zs8liM2EuLE/pDkP2QKe6xJMlXzzawWpXhaDzLhn4ugTncxbgtNMs+1b/97l
c6wjOy0AvzVVdAlJ2ElYGn+SNuZRkg7zJn0cTRe8yexDJtC/QV9AqURE9JnnV4ee
UB9XVKg+/XRjL7FQZQnmWEIuQxpMtPAlR1n6BB6T1CZGSlCBst6+eLf8ZxXhyVeE
Hg9j1uliutZfVS7qXMYoCAQlObgOK6nyTJccBz8NUvXt7y+CDwIDAQABo0IwQDAd
BgNVHQ4EFgQUU3m/WqorSs9UgOHYm8Cd8rIDZsswDgYDVR0PAQH/BAQDAgEGMA8G
A1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQEMBQADggIBAFzUfA3P9wF9QZllDHPF
Up/L+M+ZBn8b2kMVn54CVVeWFPFSPCeHlCjtHzoBN6J2/FNQwISbxmtOuowhT6KO
VWKR82kV2LyI48SqC/3vqOlLVSoGIG1VeCkZ7l8wXEskEVX/JJpuXior7gtNn3/3
ATiUFJVDBwn7YKnuHKsSjKCaXqeYalltiz8I+8jRRa8YFWSQEg9zKC7F4iRO/Fjs
8PRF/iKz6y+O0tlFYQXBl2+odnKPi4w2r78NBc5xjeambx9spnFixdjQg3IM8WcR
iQycE0xyNN+81XHfqnHd4blsjDwSXWXavVcStkNr/+XeTWYRUc+ZruwXtuhxkYze
Sf7dNXGiFSeUHM9h4ya7b6NnJSFd5t0dCy5oGzuCr+yDZ4XUmFF0sbmZgIn/f3gZ
XHlKYC6SQK5MNyosycdiyA5d9zZbyuAlJQG03RoHnHcAP9Dc1ew91Pq7P8yF1m9/
qS3fuQL39ZeatTXaw2ewh0qpKJ4jjv9cJ2vhsE/zB+4ALtRZh8tSQZXq9EfX7mRB
VXyNWQKV3WKdwrnuWih0hKWbt5DHDAff9Yk2dDLWKMGwsAvgnEzDHNb842m1R0aB
L6KCq9NjRHDEjf8tM7qtj3u1cIiuPhnPQCjY/MiQu12ZIvVS5ljFH4gxQ+6IHdfG
jjxDah2nGN59PRbxYvnKkKj9
-----END CERTIFICATE-----

)EOF";

// Button structure.
struct Button {
  const uint8_t PIN;
  volatile uint8_t pressed_count;
  volatile bool pressed;
};

// Timer Class. 
class timer{
  int64_t current;
  int64_t previous;

  int64_t mcurrent;
  int64_t mprevious;
  
  public:
    int utimer(int64_t delta_t){
      this->current = esp_timer_get_time();     // Returns the time in microseconds.
      if( (this->current - this->previous) >= delta_t ){
        this->previous = this->current;      
        return 1;
      }

      return 0;
    }
    
    int mtimer(int64_t delta_t){
      this->mcurrent = esp_timer_get_time() / 1000; 
      if( (this->mcurrent - this->mprevious) >= delta_t ){

        this->mprevious = this->mcurrent;
        return 1;
      }   
      return 0;
    }
 };


void send_message(){
  net.setCACert(ODEV_CERT_CA);
  if( !net.connect(server, 443) ){
    Serial.printf("Failed to connect.\n\r");
  }

  // Make HTTP request.
  net.printf("POST https://%s/spirometer/check.php?test=hello HTTP/1.0\n", server);
  net.printf("Host: %s\n", server); 
  net.printf("User-Agent: IoT\n");
  net.printf("Content-Type: application/x-www-form-urlencoded\n");
  net.printf("Content-Length: 15\n\n");
  net.printf("test=helloworld");
  net.printf("\n");
  
  while (net.connected()) {
    String line = net.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  
  // if there are incoming bytes available
  // from the server, read them and print them:
  while (net.available()) {
    char c = net.read();
    Serial.printf("%c", c);
  }
  Serial.println();
  net.stop();  
}

// LED Status Functions
// --------------------------------------------
// Reset LED and corresponding function pointer
void reset_LED(Freenove_ESP32_WS2812* s){
  s->setLedColorData(0, 0, 0, 0);
  s->show();
}
void (*preset_LED)(Freenove_ESP32_WS2812*) = &reset_LED;

// Set LED.
void set_LED(Freenove_ESP32_WS2812* s, int R, int G, int B){
  s->setLedColorData(0, R, G, B);
  s->show();
}
void (*pset_LED)(Freenove_ESP32_WS2812*, int, int, int) = &set_LED;

// SSID and PASS are stored in a credentials.h, this file is not shared on git for obvious reasons. 
// Simply create your own file.
void connect_to_wifi(){
  timer wifi_timer;
  uint8_t counter = 0;
  //connect to WiFi
  Serial.printf("Connecting to %s", ssid);
  WiFi.begin(ssid, wifi_password);
  while( WiFi.status() != WL_CONNECTED ) {
      if( counter >= 30 ){
        Serial.printf("Taking too long. Likely connection will fail. Restarting.\n\r");
        esp_restart();
      }
      delay(250);
      Serial.printf(" .");
      counter++;
  }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println(" Connected.");

  send_message();
  
}

// https://cplusplus.com/reference/ctime/asctime/
// https://cplusplus.com/reference/ctime/tm/
char* alt_asctime(const struct tm *timeptr){
  static char result[26];
  sprintf(result, "%d-%02d-%02d %d:%d:%d",
   1900 + timeptr->tm_year,
   1 + timeptr->tm_mon,
   timeptr->tm_mday,
   timeptr->tm_hour,
   timeptr->tm_min,
   timeptr->tm_sec  
  );
  return result;
}

char* get_local_time(){
  struct tm timeinfo;
  
  while(!getLocalTime(&timeinfo)){
    Serial.printf("Failed to obtain time.\n\r");
  }
  return alt_asctime(&timeinfo);
}
