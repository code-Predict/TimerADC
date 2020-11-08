/*
 * タイマ割込みでADCをいじる
*/
#include "ADCAccessor.h"
#include "Buffer.h"

// ADC定数
#define PGA 1
#define VREF 2.048
#define ADS1220_CS_PIN    16
#define ADS1220_DRDY_PIN  4
#define BUFFER_SIZE 2000

// 割り込みベクタサイズ
#define INTVECT_SIZE 5

// --
ADCAccessor adc(ADS1220_CS_PIN, ADS1220_DRDY_PIN);
Buffer adcStreamBuffer, *B;
hw_timer_t *timer = NULL;

// ユーザ定義割込み関数
void dumpADCValue();
void buffering();

// 割込みテーブル
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
volatile unsigned int interruptCounter[INTVECT_SIZE] = {};
void nopVect();
void (*intVect[])() = {
    dumpADCValue,
    buffering,
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
    attachInterrupt(ADS1220_DRDY_PIN, &onDataReady, FALLING);
    B = &adcStreamBuffer;
    initBuffer(B, BUFFER_SIZE);

    // タイマ割り込み(1ms)有効化
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &onTimer, true);
    timerAlarmWrite(timer, 1000, true);
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

// AD変換終了時
void dumpADCValue(){
    // ADCの測定値を更新
    adc.updateADCValue();
}

// 毎ミリ秒実行される処理
void buffering(){
    // 電圧を計算して
    float Vout = (float)((adc.getADCValue() * VREF/PGA * 1000) / (((long int)1<<23)-1));
    // バッファに突っ込む
    Item item;
    initItem(*item);
    // TODO: Item構造体の中身どうする?
    push(B, item);
}

