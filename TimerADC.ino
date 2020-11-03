/*
 * タイマ割込みでADCをいじる
*/
#include "ADCAccessor.h"

#define PGA 1
#define VREF 2.048
#define VFSR VREF/PGA
#define FSR (((long int)1<<23)-1)
#define INTVECT_SIZE 5

#define ADS1220_CS_PIN    16
#define ADS1220_DRDY_PIN  4

//
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
volatile unsigned int interruptCounter[INTVECT_SIZE] = {};
void (*intVect[])() = {
    dumpADCValue,
    nopVect,
    nopVect,
    nopVect,
    nopVect
};

// 割り込みベクタのパディング
void nopVect(){}

//
ADCAccessor adc(ADS1220_CS_PIN, ADS1220_DRDY_PIN);

// ISR: AD変換終了
void IRAM_ATTR onDataReady() {
    portENTER_CRITICAL_ISR(&timerMux);
    interruptCounter[0]++;
    portEXIT_CRITICAL_ISR(&timerMux);
}

// 変換終了時の処理
void dumpADCValue(){
    // ADCから値を取得し、表示
    int32_t adcValue = adc.readADCValue();
    float Vout = (float)((adcValue * VFSR * 1000) / FSR);
    Serial.println(Vout);
}

void setup(){
    Serial.begin(115200);

    pinMode(32, OUTPUT);

    // ADC初期化
    adc.begin(DR_1000SPS, PGA_GAIN_1, MUX_AIN0_AIN1);

    // 割込み有効化
    attachInterrupt(ADS1220_DRDY_PIN, &onDataReady, FALLING);
}

void loop(){
    // 割込みが発生しているか走査する
    for(int i = 0; i < INTVECT_SIZE; i++){
        if(interruptCounter[i] > 0){
            portENTER_CRITICAL(&timerMux);
            interruptCounter[i]--;
            // 割り込みベクタを呼び出す
            (*intVect[i])();
            portEXIT_CRITICAL(&timerMux);
        }
    }
}

