const float H = 67.0;
const float A = 26.0;
const float MILLI = 1000.00;
const uint8_t VOLUME = 1;

const uint8_t NUM_SAMPLES = 4;
const uint8_t SAMPLING_SECONDS = 6;
const uint8_t INSTANT_VOLUME_ENTRIES = ((NUM_SAMPLES*SAMPLING_SECONDS)+2);

// COUGH threshold may neeed to be tuned more.
const uint8_t COUGH_COUNTER_THRESH = 5;
const uint8_t PEAK_WINDOW = 200;
const uint8_t SAMPLES_A_SECOND = 4;

// Number of instantaneous samples in a second (44100/4)
const uint16_t SAMPLES_SNAPSHOT_SECOND =  11025;
const float LOWERLIMITOFNORMAL_SCALAR = 0.82;
const float UPPERLIMITOFNORMAL_SCALAR = ( 1.0 + (1.0 - LOWERLIMITOFNORMAL_SCALAR ) );
const float LOWERLIMIT_FEV = 0.30;
const float MAX_THRESH_FVC_SCALAR =  1.5;
const float MAX_THRESH_PEFR_SCALAR = 1.70;

class spirometer{
  String mac_address;
  volatile uint32_t sample_counter = 0;
  char time_stamp[26];
  char* ptr_time_stamp;
  char sex = 'M';

  int32_t max_ambient_energy_threshold;
  int32_t min_ambient_energy_threshold;
  uint32_t window_counter = 0;
  uint16_t instant_volume_counter = 0;
  uint16_t cough_counter = 0;
    
  double output = 0;
  double energy = 0;
  double snapshot_one_second = 0;
  double FVC = 0.0;
  double FEV = 0.0;
  double PEFR = 0.0;
  double inst_volume[INSTANT_VOLUME_ENTRIES];  // 4 samples per second.
  double window_samples[PEAK_WINDOW];  

 bool peak_detection(double current, double previous, double next ){
    // If Left side is larger or right side is larger, this is not a peak.
    if ( (previous > current) || (next > current) ){
      return false;
    }
    if( previous == current && current == next ){
      return false;     // Weird instance, but sometimes there is no slope at the tail end of the exhalation.
    }
    return true;    
  }

  bool pefr_peak_detection(double current, double previous, double next ){
    // If Left side is larger or right side is larger, this is not a peak.
    if ( (previous > current) || (next > current) ){
      return false;
    }
    if( previous == current && current == next ){
      return false;     // Weird instance, but sometimes there is no slope at the tail end of the exhalation.
    }
    return true;    
  }

   
  public:
    void reset_spirometer_calculations(){
      this->sample_counter = 0;    
      this->window_counter = 0;
      this->instant_volume_counter = 0;
      this->cough_counter = 0;
      this->output = 0;
      this->energy = 0;
      this->snapshot_one_second = 0;
      this->FVC = 0.0;
      this->FEV = 0.0;
      this->PEFR = 0.0;
    }

    
    // GETTER and SETTER Functions
    // --------------------------------------------
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
    // --------------------------------------------
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


    // Spirometer Functions
    // --------------------------------------------
    double calc_energy(double& in){
      return ( in * in );
    }

    // Estimate PEFR
    double PEFR_EST(){
      if ( this->sex == 'M' ){
        double height_m = H * 0.0254;   // Convert inches to meters.
        return ( (((height_m * 3.72) + 2.24) - (A * 0.03)) * 60);
      }
      else {
        return 0.0;
      }
    }
    
    // Estimate FVC
    double FVC_EST(){
      if ( this->sex == 'M' ){
        return (((0.1524*H)-(0.0214*A)-4.6500));
      }
      else {
        return 0.0;
      }
    }
    // Estimate FEV
    double FEV_EST(){
      if ( this->sex == 'M' ){
        return ((0.1052*H)-(0.0244*A)-2.1900);
      }
      else { 
        return 0.0;
      }
    }

    // Calculate FVC from energy.
    double calc_FVC(double energy){
      if ( this->sex == 'M' ){
        double numerator = (double) 15*energy;
        double denominator = (double) 100;
        // Must convert to floats to use in FPU.
        float ratio = (float)numerator / (float)denominator;
        double fvc_est = ((0.1524*H)-(0.0214*A)-4.65);
        return VOLUME*((ratio * fvc_est)/ MILLI);         
      }
      // Not implemented yet.
      else {
        return 0.0;
      }
    }

    // Calculate PEFR from energy.
    double calc_PEFR(double energy){
      if( this->sex == 'M' ){
        double numerator = (double) 15*energy;
        double denominator = (double) 100;
        // Must convert to floats to use in FPU.
        float ratio = (float)numerator / (float)denominator;
        double height_m = H * 0.0254;   // Convert inches to meters.
        double pefr_male = ( (((height_m * 5.48) + 1.58) - (A * 0.041)) * 60);
        
        return VOLUME*((ratio * pefr_male) / MILLI);
      }
      else {
        return 0.0;
      }
    }

    void pefr_peak(){
      int i = 1;
      int j = 0;
      int max_val_index = 0;
      double slope[INSTANT_VOLUME_ENTRIES];
      double PEFR_peak = 0.0;
      // Use FVC samples. 
      // Get slope between each sample.
      for( i = 1; i < INSTANT_VOLUME_ENTRIES; i++){
        slope[i] = this->inst_volume[i] - this->inst_volume[i-1]; // Get RISE
        slope[i] = slope[i] * SAMPLES_A_SECOND;    // Since time is segmented into 4 samples per second, rather than dividing you can multiple by the reciprocal I.E y2-y1 / ( 1 /4 ) = ( y2 - y1 ) * ( 4 / 1 ) 
      }
  
      for(i = 0; i < INSTANT_VOLUME_ENTRIES; i++){
        if( i > 0 && i < INSTANT_VOLUME_ENTRIES ){
          // Get entries to left and right of current sample.
          if ( this->pefr_peak_detection(slope[i], slope[i-1], slope[i+1] ) ){
               PEFR_peak = slope[i] > PEFR_peak ? slope[i] : PEFR_peak;
          }
        }
      }
      this->PEFR = this->calc_PEFR(PEFR_peak);
    }
    

    // Determine if the user coughed during exhalation. This will skew results and requires another measurements.
    uint8_t cough_detection(){
      int i = 1;
      int j = 0;
      int max_val_index = 0;
      double slope[INSTANT_VOLUME_ENTRIES];
      // Use FVC samples. 
      // Get slope between each sample.
      for( i = 1; i < INSTANT_VOLUME_ENTRIES; i++){
        slope[i] = this->inst_volume[i] - this->inst_volume[i-1]; // Get RISE
        slope[i] = slope[i] * SAMPLES_A_SECOND;    // Since time is segmented into 4 samples per second, rather than dividing you can multiple by the reciprocal I.E y2-y1 / ( 1 /4 ) = ( y2 - y1 ) * ( 4 / 1 ) 
      }
      
      for(i = 0; i < INSTANT_VOLUME_ENTRIES; i++){
        if( i > 0 && i < INSTANT_VOLUME_ENTRIES ){
          // Get entries to left and right of current sample.
          if ( this->peak_detection(slope[i], slope[i-1], slope[i+1] ) ){
            this->cough_counter = this->cough_counter + 1;      // Increment cough counter.
          }
        }
      }

      if ( this->cough_counter > COUGH_COUNTER_THRESH ){
        return COUGH_DETECTED;
      }
      else{
        return END_READING;
      }
      
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
      Serial.printf(" Exhale!\n\r");
    }
    
    void bad_reading_indicator(Freenove_ESP32_WS2812* indicator, uint8_t error){
      uint8_t i = 0;
      Serial.printf("Bad reading! Retry\n\r");
      for( i = 0; i < error; i++){
        // Alternate RED and YELLOW.
        pset_LED(indicator, 255, 0, 0);     // Red
        delay(250);
        pset_LED(indicator, 255, 255, 0);     // Yellow
        delay(250);
        pset_LED(indicator, 255, 0, 0);     // Red
        delay(250);
        pset_LED(indicator, 255, 255, 0);     // Yellow
        delay(250);
      }
      this->hold_breath_indicator(indicator);
    }
    
    uint8_t capture_data(Freenove_ESP32_WS2812* indicator, int32_t* sample){
      // Lock measurement in for 6 seconds, this is done by checking to see if the number of samples read in is equivalent to the SAMPLE RATE * 6.
      // This should account for 6 seconds. For some reason, using the timer method does not work very well for this. 
      while ( this->sample_counter != (6 * SAMPLE_RATE) ) {
        err = i2s_read(I2S_PORT, (char *)sample, sizeof(int32_t), &bytes_read, xDelay);
        this->output = (double) *sample / (double) LONG_MAX;
        this->energy += this->calc_energy(this->output);

        // Grab snapshot of energy for FEV.
        if( this->sample_counter == SAMPLE_RATE ){
          this->snapshot_one_second = this->energy;
        }

        // Grab snapshot of energy for the graph.
        if( this->sample_counter % SAMPLES_SNAPSHOT_SECOND == 0 ){
          this->inst_volume[this->instant_volume_counter] = this->energy;
          this->instant_volume_counter++;          
        }
        
        this->sample_counter += 1;      
      }

      this->pefr_peak();    // Determine the PEAK expiratory flow rate. The information is stored in this->PEFR.
      this->FEV = this->calc_FVC(this->snapshot_one_second);   // Calculate FEV from the energy at 1 second.
      this->FVC = this->calc_FVC(this->energy);                // Calculate FVC from final energy total.
      
      // Display once.
      Serial.printf("FEV1: %lf L\n\r", this->FEV);
      Serial.printf("FVC6: %lf L\n\r", this->FVC);
      Serial.printf("PEFR: %lf L/min\n\r\n\r", this->PEFR);
      // ToDo: Show status of measurement VIA LED.
      this->reset_spirometer_calculations();
      // Reset LED.
      preset_LED(indicator);

      // Determine if there was a COUGH.
      if ( this->cough_detection() ){
        this->bad_reading_indicator(indicator, COUGH_DETECTED);
        return COUGH_DETECTED;
      }
      if ( this->FVC <= (LOWERLIMITOFNORMAL_SCALAR*FVC_EST()) || this->FEV < (LOWERLIMIT_FEV*FEV_EST()) ){
        this->bad_reading_indicator(indicator, BAD_READING);
        return BAD_READING;
      }
      if ( this->FVC >= (MAX_THRESH_FVC_SCALAR*FVC_EST()) ){
        this->bad_reading_indicator(indicator, BAD_READING);   
        return BAD_READING;
      }
      // Send time stamp with the message.
      //this->ptr_time_stamp = (char*) get_local_time();
      //Serial.printf("%s\n\r", this->ptr_time_stamp);
      
      // Measure Flag is returned, this terminates the sampling.
      return END_READING;         

    }
    
};
