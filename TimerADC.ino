/*
 * タイマ割込みでADCをいじる
*/
#include "Buffer.h"
#include "ADCAccessor.h"

#include "Prediction.h" // 推定関数の分割先

// constants
#define PGA 1
#define VREF 1.65

#define ADS1120_CS_PIN   16
#define ADS1120_DRDY_PIN 4

#define BUFFER_LOCK_PIN 25 // このピンが立ち下がるとバッファがロックされる  

#define ADC_BUFFER_SIZE 2000

// 割り込みベクタサイズ
#define INTVECT_SIZE 5

// タイマ割り込み関数
void dumpADCValue();
void buffering();
void bufLockInterrupt();

void dumpBuffer(Buffer *B);

// インタフェース
ADCAccessor adc(ADS1120_CS_PIN, ADS1120_DRDY_PIN);
hw_timer_t *timer = NULL, *bufTimer = NULL;

// バッファ
Buffer *B, adcStreamBuffer;
unsigned int pushCnt = 0; // push回数

// 割込みテーブル
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
volatile unsigned int interruptCounter[INTVECT_SIZE] = {};
void nopVect();
void (*intVect[])() = {
    dumpADCValue,
    buffering,
    bufLockInterrupt,
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

// ISR 2: バッファロック割り込み
void IRAM_ATTR onBufLock(){
    portENTER_CRITICAL_ISR(&timerMux);
    interruptCounter[2]++;
    portEXIT_CRITICAL_ISR(&timerMux);
}

void setup(){
    Serial.begin(115200);

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

    // タイマ起動
    timerAlarmEnable(bufTimer);

    // バッファロック割り込み有効化
    pinMode(BUFFER_LOCK_PIN, INPUT_PULLUP);
    attachInterrupt(BUFFER_LOCK_PIN, &onBufLock, FALLING);
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

    // バッファがロックされてからバッファがいっぱいになったら
    if(adcStreamBuffer.isLocked && adcStreamBuffer.currentStatus == BUFFER_FULL){
        // シリアルに吐き出してアンロック
        dumpBuffer(B);
        unlockBuffer(B);
    }

}

// BUFLOCKピン立下り
void bufLockInterrupt(){
    // 全てpopして、
    int status = BUFFER_OK;
    while(status == BUFFER_OK){
        Item item;
        status = pop(B, &item);
    }

    Serial.println("!!");

    // ロック
    lockBuffer(B);
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

    // バッファに突っ込む
    Item item;
    initItem(&item);
    item.value = volts - (VREF / 2);
    pushStat = push(B, item);

    // ロックされていれば加算
    if(B->isLocked){
        pushCnt++;
    }

/*
    // 開始点を検知したらバッファをロック
    if(B->isLocked == 0 && detectStartPoint(B, 4, 17, 2)){
        lockBuffer(B);
    }
*/
}

// バッファをシリアルにダンプする
void dumpBuffer(Buffer *B){
    int status = BUFFER_OK, idx = 0;
    Item item;
    initItem(&item);

    // 平均値を求めながらバッファを吐き出す
    getItemAt(B, 0, &item);
    double average = item.value;

    while(status == BUFFER_OK){
        status = getItemAt(B, idx, &item);
        if(status == BUFFER_OK){
            average = ((average + item.value) / 2);
            Serial.print("RawValue:");
            Serial.print(item.value);
            Serial.print("\t");
            Serial.print("Average:");
            Serial.println(average);
            idx++;
        }
    }
}
