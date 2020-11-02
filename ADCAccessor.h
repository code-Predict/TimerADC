/*
 * ADCアクセサ
*/
#include "Protocentral_ADS1220.h"
#include <SPI.h>

class ADCAccessor{
    private:

        // ADC
        bool isEnabled;
        int DRDYpin, CSpin;
        Protocentral_ADS1220 pc_ads1220;

    public:
        ADCAccessor(uint8_t DRDYpin, uint8_t CSpin);
        void begin(uint8_t speed, uint8_t gain, uint8_t mux);
        float readADCValue();
        void enable();

};

