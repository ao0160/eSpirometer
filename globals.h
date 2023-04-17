#ifdef USE_API
  #include <WiFi.h>
#else
  #include <WiFiClientSecure.h>
#endif

#include <math.h>
#include <limits.h>
#include <pgmspace.h>
#include "mbedtls/md.h"
#include "esp_wpa2.h"
#include "WiFi.h"
#include "driver/i2s.h"
#include "soc/i2s_reg.h"
#include "time.h"
#include "freertos/queue.h"
#include "Freenove_WS2812_Lib_for_ESP32.h"
#include "credentials.h"

#define DEBUG true

const uint8_t END_READING  =  0;
const uint8_t START_READING = 1;
const uint8_t BAD_READING  =  2;
const uint8_t COUGH_DETECTED = 3;

// LED
const uint8_t LEDS_COUNT = 1;
const uint8_t LEDS_PIN = 18;
const uint8_t CHANNEL  = 0;
// Button
const uint8_t CAPTURE_BUTTON = 32;
const uint8_t BUTTON_THRESH = 1;
const uint16_t DEBOUNCE_US = 250000;
// I2S definitions
const uint16_t SAMPLE_RATE = 44100;
TickType_t xDelay = 32;
size_t bytes_read = 0;
esp_err_t err;

// Networking.
const char* server = "api.olvera-dev.com";   // Example utilizing public webserver.
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;
// Get MAC ADDRESS and SHA256 of MACADDRESS to submit for memo in account creation.
const char *mac_address = (char*) WiFi.macAddress().c_str();
byte sha_256_result[32];
char sha_256_result_string[64];

#ifdef USE_API 
  WiFiClient net = WiFiClientSecure();
#else
  WiFiClientSecure net = WiFiClientSecure();
#endif

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

void set_device_info(){
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
  
  const size_t mac_length = strlen(mac_address);
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
  
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char *) mac_address, mac_length);
  mbedtls_md_finish(&ctx, sha_256_result);
  mbedtls_md_free(&ctx);
}

void get_device_info(){
  int j = 0;
  
  for(int i= 0; i< sizeof(sha_256_result); i++){
    j += sprintf(sha_256_result_string+j, "%02x", (int)sha_256_result[i]);
  }
  Serial.printf("Hash: %s\n\r", sha_256_result_string);
}

#ifdef USE_API
void send_device_data(String payload){
  if( !net.connect(api_end_point, 9090) ){
    Serial.printf("Failed to connect.\n\r");
  }
  Serial.printf("Connecting to %s\n\r", api_end_point);

  // Make HTTP request.
  net.printf("POST http://%s/v1/send-espirometer-data HTTP/1.0\n", api_end_point);
  net.printf("Host: %s\n", api_end_point); 
  net.printf("User-Agent: IoT\n");
  net.printf("Content-Type: application/x-www-form-urlencoded\n");
  
  // Length will be calculated, not a fan of this method since it will constantly allocate memory leading to memory leaks at some point.
  String message = "fileid=4127209";  // First section. Hardcoded file.
  //message = message + "&memo=" + String(sha_256_result_string);    // Add HASH.
  message = message + "&filecontents=" + payload;
  message = message + "&pubkey=" + String(DER_PUB_KEY) + "&privkey=" + String(DER_PRIV_KEY);
  
  Serial.printf("Message: %s, Message Length: %d\n\r", message.c_str(), message.length());
  net.printf("Content-Length: %d\n\n", message.length());
  net.printf("%s", message.c_str());
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

// Basic example posting to my server. Will leave this here for examples sake, but will leave commented out.
// This servers as a verification that account exists.
void send_test_message(){ 
  if( !net.connect(api_end_point, 9090) ){
    Serial.printf("Failed to connect.\n\r");
  }
  Serial.printf("Connecting to %s\n\r", api_end_point);

  // Make HTTP request.
  net.printf("POST http://%s/v1/get-account/memo HTTP/1.0\n", api_end_point);
  net.printf("Host: %s\n", api_end_point); 
  net.printf("User-Agent: IoT\n");
  net.printf("Content-Type: application/x-www-form-urlencoded\n");
  
  // Length will be calculated. 
  String message = "accountid=3590940&memo=";   // Definitely don't like doing this.
  //message = message + String("auto-created account");
  message = message + String(sha_256_result_string);
  Serial.printf("Message: %s, Message Length: %d\n\r", message.c_str(), message.length());
  net.printf("Content-Length: %d\n\n", message.length());
  net.printf("%s", message.c_str());
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
#else
// Basic example posting to my server. Will leave this here for examples sake, but will leave commented out.
void send_test_message(){
  net.setCACert(ODEV_CERT_CA);
  if( !net.connect(server, 443) ){
    Serial.printf("Failed to connect.\n\r");
  }
  Serial.printf("Connecting to %s\n\r", server);
  // Make HTTP request.
  net.printf("POST https://%s/spirometer/check.php?test=hello HTTP/1.0\n", server);
  net.printf("Host: %s\n", server); 
  net.printf("User-Agent: IoT\n");
  net.printf("Content-Type: application/x-www-form-urlencoded\n");
  // Length will be calculated. 
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
#endif

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
  uint16_t networks = 0;

  WiFi.mode(WIFI_STA);
  networks = WiFi.scanNetworks();
  if (networks == 0) {
    Serial.println("No reachable networks found.\n\r");
  } 
  else {
    Serial.printf("%d reachable networks found.\n\r", networks);
    for (int i = 0; i < networks; ++i) {
        if( WiFi.SSID(i) == ssid ){
          HOME_NET = 1;
        }
        else if( WiFi.SSID(i) == school_ssid ){
          SCHOOL_NET = 1;
        }
    }
  }
  Serial.println("");
  // Delete the scan result to free memory for code below.
  WiFi.scanDelete();
  
  //connect to WiFi
  if ( HOME_NET ){
    WiFi.disconnect(true);
    Serial.printf("Connecting to %s", ssid);
    WiFi.begin(ssid, wifi_password);
  }
  else if ( SCHOOL_NET ){
    WiFi.disconnect(true);
    Serial.printf("Connecting to %s", school_ssid);
    esp_wpa2_config_t config = WPA2_CONFIG_INIT_DEFAULT();
    esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)EAP_IDENTITY, strlen(EAP_IDENTITY)); //provide identity
    esp_wifi_sta_wpa2_ent_set_username((uint8_t *)EAP_USERNAME, strlen(EAP_USERNAME)); //provide username
    esp_wifi_sta_wpa2_ent_set_password((uint8_t *)EAP_PASSWORD, strlen(EAP_PASSWORD)); //provide password
    esp_wifi_sta_wpa2_ent_enable(&config);  
    
    WiFi.begin(school_ssid);
  }
  
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
  Serial.printf("Connected to: %s\n\r", WiFi.SSID());

  send_test_message(); 
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
