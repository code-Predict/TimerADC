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

// vars
ADCAccessor adc(ADS1220_CS_PIN, ADS1220_DRDY_PIN);
hw_timer_t *timer = NULL;

// ユーザ定義割込み関数
void dumpADCValue();
void pushDataToBuffer();

// 割込みテーブル設定
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
volatile unsigned int interruptCounter[INTVECT_SIZE] = {};
void nopVect();
void (*intVect[])() = {
    dumpADCValue,
    pushDataToBuffer,
    nopVect,
    nopVect,
    nopVect
};
void nopVect(){}

// ISR 0: AD変換終了
void IRAM_ATTR onDataReady() {
    portENTER_CRITICAL_ISR(&timerMux);
    interruptCounter[0]++;
    portEXIT_CRITICAL_ISR(&timerMux);
}

// ISR 1: バッファリングタイマコンペアマッチ
void IRAM_ATTR onTimer(){
    portENTER_CRITICAL_ISR(&timerMux);
    interruptCounter[1]++;
    portEXIT_CRITICAL_ISR(&timerMux);
}

void setup(){
    Serial.begin(115200);

    pinMode(32, OUTPUT);

    // ADC初期化
    adc.begin(DR_1000SPS, PGA_GAIN_1, MUX_AIN0_AIN1);

    // DRDY割込み有効化
    attachInterrupt(ADS1220_DRDY_PIN, &onDataReady, FALLING);

    // タイマ割り込み(1ms)有効化
    timer = timerBegin(0, 80. true);
    timerAttachInterruot(timer, &onTimer, true);
    timerAlarmWriter(timer, 1000, true);
    timerAlarmEnable(timer);
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

// 変換終了時の処理
void dumpADCValue(){
    // ADCの測定値を更新
    adc.updateADCValue();
}

// バッファリングタイママッチ時の処理
void pushDataToBuffer(){
    float adcValue = adc.getADCValue();
    float Vout = (float)((adcValue * VFSR * 1000) / FSR);
    Serial.println(Vout);
}