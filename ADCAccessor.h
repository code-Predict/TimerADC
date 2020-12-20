/*
 * ADCアクセサ
*/
#include "Protocentral_ADS1220.h"
#include <SPI.h>

#define ADCACCESSOR_SINGLESHOT 0
#define ADCACCESSOR_CONTINUOUS 1
#define ADCACCESSOR_GAIN_DISABLED -1

#define ADCACCESSOR_REF_INTERNAL 0x00
#define ADCACCESSOR_REF_REF0 0x01
#define ADCACCESSOR_REF_REF1 0x02
#define ADCACCESSOR_REF_AVDD 0x03

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
        void setReference(uint8_t ref);
        void startConv();

        float getADCValue();
        void updateADCValue();

};

