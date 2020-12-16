/*
 * タイマ割込みでADCをいじる
*/
#include "Buffer.h"
#include "ADCAccessor.h"
#include "Prediction.h"

// constants
#define PGA 1
#define VREF 2.048

#define ADS1120_CS_PIN   16
#define ADS1120_DRDY_PIN 4

#define ADC_BUFFER_SIZE 100

#define MCP2515_CS_PIN 6 //TODO: 直す
#define MCP2515_INT_PIN 6

// 割り込みベクタサイズ
#define INTVECT_SIZE 5

// インタフェース
ADCAccessor adc(ADS1120_CS_PIN, ADS1120_DRDY_PIN);
CANAccessor can(MCP2515_CS_PIN);
hw_timer_t *timer = NULL, *bufTimer = NULL;

// バッファ
Buffer *B, adcStreamBuffer;
Buffer *CB, canSendBuffer;

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
void nopVect(){};

// ISR 0: AD変換終了
void IRAM_ATTR onDataReady() {
    portENTER_CRITICAL_ISR(&timerMux);
    interruptCounter[0]++;
    portEXIT_CRITICAL_ISR(&timerMux);
}

// ISR 1: バッファリングタイマコンペアマッチ
void IRAM_ATTR onBufTimer(){
    portENTER_CRITICAL_ISR(&timerMux);
    interruptCounter[1]++;
    portEXIT_CRITICAL_ISR(&timerMux);
}

// ISR 2: CANバッファ消化
void IRAM_ATTR onCanTimer(){
    portENTER_CRITICAL_ISR(&timerMux);
    interruptCounter[2]++;
    portEXIT_CRITICAL_ISR(&timerMux);
}

void setup(){
    Serial.begin(115200);

    pinMode(32, OUTPUT);

    // ADC初期化
    adc.begin(DR_1000SPS, MUX_AIN0_AVSS); // 1000SPS, AIN0とAVSSのシングルエンド
    adc.setGain(ADCACCESSOR_GAIN_DISABLED); // PGA無効
    adc.setConvMode(ADCACCESSOR_CONTINUOUS); // 継続変換モード
    adc.startConv(); // 変換開始
    attachInterrupt(ADS1120_DRDY_PIN, &onDataReady, FALLING); // DRDYの立下りに割り込み設定

    // ADCストリームバッファ初期化
    B = &adcStreamBuffer;
    initBuffer(B, ADC_BUFFER_SIZE);

    // ADCバッファリング割り込み(1ms)有効化
    bufTimer = timerBegin(0, 80, true);
    timerAttachInterrupt(bufTimer, &onBufTimer, true);
    timerAlarmWrite(bufTimer, 1000, true);
    timerAlarmEnable(bufTimer);

    // CAN初期化
    can.begin();
    
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

    // バッファがロックされていて、バッファフルを検知したら
    if(adcStreamBuffer.isLocked && adcStreamBuffer.currentStatus == BUFFER_OVER){
        // 終了点推定してブレーキ時間に変換し、
        int endPoint = getEndPoint(B, -20);

        // CAN送信バッファに送りつける


        // アンロック
        unlockBuffer(B);
    }
}

// AD変換終了時
void dumpADCValue(){
    // ADCの測定値を更新
    adc.updateADCValue();
}

// バッファリングタイマによる割り込み
void buffering(){
    static unsigned int pushStat = BUFFER_OK;

    // 電圧を計算して
    float volts = (float)((adc.getADCValue() * VREF/PGA * 1000) / (((long int)1<<23)-1));
    Serial.println(volts); // シリアルに吐き出す

    // バッファに突っ込む
    Item item;
    initItem(&item);
    item.value = volts;
    pushStat = push(B, item);

    // 開始点を検知したらバッファをロック
    if(B->isLocked == 0 && detectStartPoint(B, 4, 17, 2)){
        lockBuffer(B);
    }
}
