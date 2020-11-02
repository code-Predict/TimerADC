/*
 * タイマ割込みでADCをいじる
*/
#include "ADCAccessor.h"

#define PGA 1
#define VREF 2.048
#define VFSR VREF/PGA
#define FSR (((long int)1<<23)-1)

#define ADS1220_CS_PIN    16
#define ADS1220_DRDY_PIN  4

//
volatile unsigned int interruptCounter;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

//
ADCAccessor adc(ADS1220_DRDY_PIN, ADS1220_CS_PIN);

// ISR
void IRAM_ATTR onDataReady() {
    portENTER_CRITICAL_ISR(&timerMux);
    interruptCounter++;
    portEXIT_CRITICAL_ISR(&timerMux);
}

void setup(){
    Serial.begin(115200);

    pinMode(32, OUTPUT);

    // ADC初期化
    adc.begin(DR_1000SPS, PGA_GAIN_1, MUX_AIN0_AIN1);
    adc.enable();

    // 割込み有効化
    attachInterrupt(ADS1220_DRDY_PIN, &onDataReady, FALLING);
}

void loop(){

    if (interruptCounter > 0) {
        // 割込みカウンタを減らす
        portENTER_CRITICAL(&timerMux);
        interruptCounter--;
        portEXIT_CRITICAL(&timerMux);

        // ADCから値を取得し、表示
        int32_t adcValue = adc.readADCValue();
        float Vout = (float)((adcValue * VFSR * 1000) / FSR);
        Serial.println(Vout);
    }
}
