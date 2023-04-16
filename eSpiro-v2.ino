#include "globals.h"
#include "spirometer.h"

timer debouncer;
timer ambient_threshold_test;
timer general_purpose;
timer FVC_timer;
timer FEV_timer;
volatile uint32_t sample_counter = 0;

Button button_trigger = { CAPTURE_BUTTON, 0,  false };
static spirometer mic_spirometer;
spirometer* ptr_mic_spirometer = &mic_spirometer;
uint8_t measure_flag = 0;

void IRAM_ATTR button_ISR(){
  button_trigger.pressed = true;        // Set true.
  button_trigger.pressed_count += 1;    // Increment the counter.
  detachInterrupt(button_trigger.PIN);  // Disable interrupt to prevent context switches if user presses during measurement phase.
}

void setup() {
  Serial.begin(115200);
  p_strip->begin();
  p_strip->setBrightness(50);
  preset_LED(p_strip);

  // I2S Configuration. 
  const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),  // Receive, not transfer
    .sample_rate = SAMPLE_RATE,                         // 44.1KHz
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,       // could only get it to work with 32bits
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,        // Explicitly use the LEFT channel, as the right channel was not responsive.
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,           // Interrupt level 1
    .dma_buf_count = 4,                                 // number of buffers
    .dma_buf_len = 8                                    // 8 samples per buffer (minimum)
  };

  // The pin config as per the setup.
  const i2s_pin_config_t pin_config = {
    .bck_io_num = 26,   // Serial Clock (SCK)
    .ws_io_num = 25,    // Word Select (WS)
    .data_out_num = I2S_PIN_NO_CHANGE, // not used (only for speakers)
    .data_in_num = 33   // Serial Data (SD)
  };

  // Configuring the I2S driver and pins.
  // This function must be called before any I2S driver read/write operations.
  err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Failed installing driver: %d\n", err);
    while (true);
  }
  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("Failed setting pin: %d\n", err);
    while (true);
  }
  
  // Calibrate. Obtain the ambient threshold value.
  while ( !ambient_threshold_test.utimer(5000000) ) {
    sample = 0;
    output = 0;

    // Get data.
    (void) i2s_read(I2S_PORT, (char *)&sample, sizeof(int32_t), &bytes_read, xDelay);
    
    // Determine sample thresholds to set min and max ambient noise thresholds.
    if ( sample > ptr_mic_spirometer->get_max_ambient() )
      ptr_mic_spirometer->set_max_ambient(sample);

    if ( sample < ptr_mic_spirometer->get_min_ambient() )
      ptr_mic_spirometer->set_min_ambient(sample);
  }

  pinMode(button_trigger.PIN, INPUT);
  attachInterrupt(button_trigger.PIN, button_ISR, FALLING);
  
  // This will be used much more later.
  Serial.printf("Calibrated - mx: %ld, mn: %ld\n\r", ptr_mic_spirometer->get_max_ambient(), ptr_mic_spirometer->get_min_ambient());
  sample = 0;
  
  pset_LED(p_strip, 255,0,0);  
  set_device_info();
  get_device_info();
    
  connect_to_wifi(); 
  send_message();
   
  preset_LED(p_strip);
  Serial.printf("%s\n\r", get_local_time());
  Serial.printf("Ready.\n\r");
}

void loop() { 
  // Every 250 microseconds do sampling, or if the measure_flag is true.
  if (  !general_purpose.utimer(5000) && measure_flag >= START_READING ) {
    // Sample data.
    (void) i2s_read(I2S_PORT, (char *)&sample, sizeof(int32_t), &bytes_read, xDelay);
    if ( ( sample > ptr_mic_spirometer->get_max_ambient() || sample < ptr_mic_spirometer->get_min_ambient() ) ) {
      Serial.printf("Measuring.\n\r");
      // Set to blue during measurement capture.
      pset_LED(p_strip,0,0,255);
      measure_flag = ptr_mic_spirometer->capture_data(p_strip,&sample);
      attachInterrupt(button_trigger.PIN, button_ISR, FALLING);   // Reattach Interrupt.
    }
  }
 
  // Check for button press. If pressed, start measurement.
  if ( debouncer.utimer(20000) && button_trigger.pressed && button_trigger.pressed_count && measure_flag == END_READING ){     
    measure_flag = START_READING;
    button_trigger.pressed = false;
    button_trigger.pressed_count = 0;
    // Flash Yellow 3 times. This function should take 3 seconds.
    ptr_mic_spirometer->hold_breath_indicator(p_strip);
  }
}
