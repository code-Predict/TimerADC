/*
 * タイマ割込みでADCをいじる
*/
#include "Buffer.h"
#include "ADCAccessor.h"
#include "CANAccessor.h"

#include "Prediction.h" // 推定関数の分割先

#define DEBUG

// constants
#define PGA 1
#define VREF 1.65

#define ADS1120_CS_PIN   16
#define ADS1120_DRDY_PIN 4

#define ADC_BUFFER_SIZE 100
#define CAN_BUFFER_SIZE 10

#define MCP2515_CS_PIN 6 //TODO: 直す
#define MCP2515_INT_PIN 6

// 割り込みベクタサイズ
#define INTVECT_SIZE 5

// タイマ割り込み関数
void dumpADCValue();
void buffering();
void popCANBuffer();

// インタフェース
ADCAccessor adc(ADS1120_CS_PIN, ADS1120_DRDY_PIN);
CANAccessor can(MCP2515_CS_PIN, MCP2515_INT_PIN);
hw_timer_t *timer = NULL, *bufTimer = NULL, *canSendTimer = NULL;

// バッファ
Buffer *B, adcStreamBuffer;
Buffer *CB, canSendBuffer;
unsigned int pushCnt = 0; // push回数

// ハードウェア固有ID
uint8_t deviceID[6]; // setupで代入

// 割込みテーブル
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
volatile unsigned int interruptCounter[INTVECT_SIZE] = {};
void nopVect();
void (*intVect[])() = {
    dumpADCValue,
    buffering,
    popCANBuffer,
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

    // CAN送信バッファ初期化
    CB = &canSendBuffer;
    initBuffer(CB, CAN_BUFFER_SIZE);

    canSendTimer = timerBegin(1, 80, true);
    timerAttachInterrupt(canSendTimer, &onCanTimer, true);
    timerAlarmWrite(canSendTimer, 1E+6, true); // ここは遅くてもいいと思う

    // デバイス固有ID取得
    getDeviceID(deviceID);

    // タイマ起動
    timerAlarmEnable(bufTimer);
    timerAlarmEnable(canSendTimer);
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

    // バッファがロックされてから一定回数pushしたら
    if(adcStreamBuffer.isLocked && pushCnt > 100){
        // 終了点推定してブレーキ時間に変換し、
        int endPoint = getEndPoint(B, -20);
        float breakTime = endPoint;

        // CAN送信バッファに送りつける
        Item item;
        item.value = breakTime;
        push(CB, item);

        // アンロック
        unlockBuffer(B);
        pushCnt = 0;
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

    #ifdef DEBUG

    Serial.println(volts); // シリアルに吐き出す

    #else

    // バッファに突っ込む
    Item item;
    initItem(&item);
    item.value = volts;
    pushStat = push(B, item);

    // ロックされていれば加算
    if(B->isLocked){
        pushCnt++;
    }

    // 開始点を検知したらバッファをロック
    if(B->isLocked == 0 && detectStartPoint(B, 4, 17, 2)){
        lockBuffer(B);
    }
    
    #endif
}

// CAN送信バッファ消化
void popCANBuffer(){
    // バッファから値をもらってくる
    Item item;
    initItem(&item);
    int status = pop(CB, &item);
    if(status == BUFFER_EMPTY){
        return;
    }

    // デバイスID取得して
    uint8_t deviceID[6];
    getDeviceID(deviceID);

    // CANメッセージを作って
    union CANDataFrame {
        struct Message {
            uint8_t partialDeviceID[4];
            float breakTime;
        } message;
        uint8_t rawValue[8];
    };

    CANDataFrame dataFrame;
    dataFrame.message.breakTime = item.value;    
    memcpy(dataFrame.message.partialDeviceID, deviceID, 4);

    // 投げる
    int result = can.sendFrame(deviceID[0], dataFrame.rawValue, 8);
    if(result != CAN_OK){
        Serial.println("Failed to send can frame.");
    }
}

// デバイスID取得
void getDeviceID(uint8_t *id){
    // 電源投入以降更新しない
    static bool isRead = false;
    static unsigned char internalID[6];
    if(!isRead){
        esp_efuse_mac_get_default(internalID);
        isRead = true;
    }
    id = internalID;
}
