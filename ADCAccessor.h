/*
 * ADCアクセサ
*/
#include "Protocentral_ADS1220.h"
#include <SPI.h>

#define ADCACCESSOR_SINGLESHOT 0
#define ADCACCESSOR_CONTINUOUS 1
#define ADCACCESSOR_GAIN_DISABLED -1

class ADCAccessor{
    private:
        int DRDYpin, CSpin;
        Protocentral_ADS1220 pc_ads1220;
        float adcValue;

    public:
        ADCAccessor(uint8_t CSpin, uint8_t DRDYpin);
        void begin(uint8_t speedRate, uint8_t mux);
        void setGain(int gain);
        void setConvMode(int mode);
        void startConv();

        float getADCValue();
        void updateADCValue();

};

