/*
 * タイマ割込みでADCをいじる
*/
#include <Buffer.h>
#include <ADCAccessor.h>
#include "CANAccessor.h"

#include "Prediction.h" // 推定関数の分割先

#define DEBUG

// constants
#define VREF 3.3

#define ADS1120_CS_PIN   16
#define ADS1120_DRDY_PIN 4

#define BUFFER_LOCK_PIN 25 // このピンが立ち下がるとバッファがロックされる  

#define ADC_BUFFER_SIZE 2000

#define MCP2515_CS_PIN 5
#define MCP2515_INT_PIN 17 // TODO: まだ結線してない(1220(日))

// 割り込みベクタサイズ
#define INTVECT_SIZE 5

// タイマ割り込み関数
void dumpADCValue();
void buffering();
void bufLockInterrupt();

void dumpBufferViaCAN(Buffer *B);
int sendCANDoubleValue(CANAccessor can, double value);

// インタフェース
ADCAccessor adc(ADS1120_CS_PIN, ADS1120_DRDY_PIN);
CANAccessor can(MCP2515_CS_PIN, MCP2515_INT_PIN);
hw_timer_t *timer = NULL, *bufTimer = NULL;

// バッファ
Buffer *B, adcStreamBuffer;

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
    adc.setReference(ADCACCESSOR_REF_AVDD); // AVDD-AVSS間をリファレンス電圧とする
    adc.startConv(); // 変換開始
    attachInterrupt(ADS1120_DRDY_PIN, &onDataReady, FALLING); // DRDYの立下りに割り込み設定

    // ADCストリームバッファ初期化
    B = &adcStreamBuffer;
    initBuffer(B, ADC_BUFFER_SIZE);

    // ADCバッファリング割り込み(1ms)有効化
    bufTimer = timerBegin(0, 80, true);
    timerAttachInterrupt(bufTimer, &onBufTimer, true);
    timerAlarmWrite(bufTimer, 1000, true);

    // CAN初期化
    if(can.begin(MCP_ANY, CAN_100KBPS, MCP_16MHZ) == CAN_OK){
        can.setMode(MCP_NORMAL);
    }else{
        Serial.println("!!!CAUTION!!! CAN device couldn't initialize!!");
    }

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
        // CAN経由で吐き出してアンロック
        dumpBufferViaCAN(B);
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
    float volts = (float)((adc.getADCValue() * VREF * 1000) / (((long int)1<<23)-1));

    // バッファに突っ込む
    Item item;
    initItem(&item);
    item.value = volts - (VREF / 2) * 1000; // INA253A3はVREFを中心に動くので減算
    pushStat = push(B, item);

/*
    // ロックされていれば加算
    if(B->isLocked){
        pushCnt++;
    }

    // 開始点を検知したらバッファをロック
    if(B->isLocked == 0 && detectStartPoint(B, 4, 17, 2)){
        lockBuffer(B);
    }
*/
}

// バッファをCAN経由でダンプする
void dumpBufferViaCAN(Buffer *B){
    int status = BUFFER_OK, idx = 0;
    Item item;
    initItem(&item);

    while(status == BUFFER_OK){
        status = getItemAt(B, idx, &item);
        if(status == BUFFER_OK){
            double current = (item.value / 400.0) * 1000;
            int status = sendCANDoubleValue(can, current);
            if(status != CAN_OK){
                Serial.print("CAN Send Failed:");
                Serial.println(status);
            }
            delay(200);
            idx++;
        }
    }
}

// double型の値をCANで送りつける
int sendCANDoubleValue(CANAccessor can, double value){
    // フレームを作り、値をセットして
    union CANDataFrame {
        double value;
        uint8_t rawValue[8];
    } dataFrame;
    dataFrame.value = value;

    // 投げる
    int result = can.sendFrame(0x000, dataFrame.rawValue, 8);
    return result;
}
