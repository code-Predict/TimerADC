/*
 * ADCアクセサ
*/
#include "Protocentral_ADS1220.h"
#include <SPI.h>

#define PGA 1
#define VREF 2.048
#define VFSR VREF/PGA
#define FSR (((long int)1<<23)-1)

class ADCAccessor{
    private:
        // 割込み管理
        volatile int interruptCounter;
        int totalInterruptCounter;
        portMUX_TYPE timerMux;

        // ADC
        int DRDYpin, CSpin;
        Protocentral_ADS1220 pc_ads1220;
        int32_t adc_data;

    public:
        ADCAccessor(uint8_t DRDYpin, uint8_t CSpin);
        void begin(uint8_t speed, uint8_t gain, uint8_t mux);
        float readADCValue();

};

