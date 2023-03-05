#define H   67.0
#define A   26.0
#define MILLI 1000.00
#define VOLUME 1

class spirometer{
  String mac_address;
  volatile uint32_t sample_counter = 0;
  
  double output = 0;
  double energy = 0;
  double snapshot_one_second = 0;
  
  int32_t max_ambient_energy_threshold;
  int32_t min_ambient_energy_threshold;
   
  public:
    // GETTER and SETTER Functions
    // Ambient threshold functions.
      void set_ambients(int32_t mx, int32_t mn){
        this->max_ambient_energy_threshold = mx;
        this->max_ambient_energy_threshold = mn;
      }
      void set_min_ambient(int32_t mn){
        this->min_ambient_energy_threshold = mn ;
      }
      void set_max_ambient(int32_t mx){
        this->max_ambient_energy_threshold = mx;
      }
      int32_t get_min_ambient(){
        return this->min_ambient_energy_threshold ;
      }
      int32_t get_max_ambient(){
        return this->max_ambient_energy_threshold;
      }
    // Energy Storage functions.
      void set_energy(double e){
        this->energy = e;
      }
      double get_energy(){
        return this->energy;
      }
      void set_snapshot_one_second(double s){
        this->snapshot_one_second = s;
      }
      double get_snapshot_one_second(){
        return this->snapshot_one_second;
      }


    // Functions
    double calc_energy(double& in){
      return ( in * in );
    }
    
    double FVC_MALE(double energy){
      double numerator = (double) 15*energy;
      double denominator = (double) 100;
      // Must convert to floats to use in FPU.
      float ratio = (float)numerator / (float)denominator;
      double fvc_est = ((0.1524*H)-(0.0214*A)-4.65);
      return VOLUME*((ratio * fvc_est)/ MILLI); 
    }      
    
    void hold_breath_indicator(Freenove_ESP32_WS2812* indicator){
      Serial.printf("Hold breath for 3 seconds.");
      // Turn Yellow! Busy wait delay works, but nice timer based delay doesn't :O WHY!
      pset_LED(indicator, 255, 255, 0);
      Serial.printf(" .");
      delay(1000);
      preset_LED(indicator);
      Serial.printf(" .");
      delay(1000);
      pset_LED(indicator, 255, 255, 0);
      Serial.printf(" .");
      delay(1000);
      preset_LED(indicator);
      Serial.printf(" Exhale!\r\n");
    }
    
    void bad_reading_indicator(Freenove_ESP32_WS2812* indicator){
      Serial.printf("Bad reading! Retry\n\r");
      pset_LED(indicator, 255, 0, 0);
      delay(1000);
      this->hold_breath_indicator(indicator);
    }
    
    
    uint8_t capture_data(Freenove_ESP32_WS2812* indicator, int32_t* sample){
      // Lock measurement in for 6 seconds, this is done by checking to see if the number of samples read in is equivalent to the SAMPLE RATE * 6.
      // This should account for 6 seconds. For some reason, using the timer method does not work very well for this. 
      while ( this->sample_counter != (6 * SAMPLE_RATE) ) {
        err = i2s_read(I2S_PORT, (char *)sample, sizeof(int32_t), &bytes_read, xDelay);
        this->output = (double) *sample / (double) LONG_MAX;
        this->energy += this->calc_energy(this->output);
        
        if( this->sample_counter == SAMPLE_RATE ){
          this->snapshot_one_second = this->energy;
        }
        
        this->sample_counter += 1;      
      }
      
      // Display once.
      Serial.printf("FEV1: %lf\r\n", this->FVC_MALE(this->snapshot_one_second));
      Serial.printf("FVC6: %lf\r\n", this->FVC_MALE(this->energy));
      // ToDo: Show status of measurement VIA LED.
      
      // Reset LED.
      preset_LED(indicator);
      this->energy = 0.0;
      this->output = 0.0;
      this->snapshot_one_second = 0.0;
      this->sample_counter = 0;
      
      // Measure Flag is returned.
      return 0;      
    }
    
};
