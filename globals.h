//#include <WiFiClientSecure.h>
//#include <MQTTClient.h>
//#include <ArduinoJson.h>
#include <math.h>
#include <limits.h>

//#include "esp_wpa2.h"
//#include "WiFi.h"
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

const i2s_port_t I2S_PORT = I2S_NUM_0;
const char* server = "olvera-dev.com";
int32_t sample = 0;
double output = 0;
double energy = 0;
double snap_one_second = 0;

Freenove_ESP32_WS2812 strip = Freenove_ESP32_WS2812(LEDS_COUNT, LEDS_PIN, CHANNEL, TYPE_RGB);
Freenove_ESP32_WS2812* p_strip = &strip;

// Expires Tue, 04 Apr 2023 09:26:32 GMT
const uint8_t sha1_fingerprint[20] = {
0x38, 0x59, 0x26, 0xE4, 0xC4, 0x73, 0x19, 0x41, 0xFC, 0x76, 0x61, 0xF5 , 0xA0, 0x6A, 0x28, 0x72, 0x8E, 0x39, 0x72, 0xB2  
};

/*
 *  Frequently used functions, set up to be function pointers.
 * 
 */
double calc_energy(double& in){
  return ( in * in );
}
double (*pcalc_energy)(double&) = &calc_energy;

/*
 * Button Structure.
 * 
 */
struct Button {
  const uint8_t PIN;
  volatile uint8_t pressed_count;
  volatile bool pressed;
};

/*
 *  Timer class, general purpose.
 * 
 */
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

/*
 *  LED Statuses
 * 
 */
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
