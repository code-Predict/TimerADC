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
hw_timer_t *bufTimer = NULL, *canTimer = NULL;

// ユーザ定義割込み関数
void dumpADCValue();
void buffering();
bool detectStartPoint();
void sendCanBuffer();
bool detectEndPoint();

// 割込みテーブル
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
volatile unsigned int interruptCounter[INTVECT_SIZE] = {};
void nopVect();
void (*intVect[])() = {
    dumpADCValue,
    buffering,
    sendCanBuffer,
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
    adc.begin(DR_1000SPS, PGA_GAIN_1, MUX_AIN0_AIN1);
    attachInterrupt(ADS1220_DRDY_PIN, &onDataReady, FALLING);

    // ADCストリームバッファ初期化
    B = &adcStreamBuffer;
    initBuffer(B, BUFFER_SIZE);

    // ADCバッファリング割り込み(1ms)有効化
    bufTimer = timerBegin(0, 80, true);
    timerAttachInterrupt(bufTimer, &onBufTimer, true);
    timerAlarmWrite(bufTimer, 1000, true);
    timerAlarmEnable(bufTimer);

    // CAN送信バッファ消化部(1s)有効化
    canTimer = timerBegin(1, 80, true);
    timerAttachInterrupt(canTimer, &onCanTimer, true);
    timerAlarmWrite(canTimer, 1000000, true);
    timerAlarmEnable(canTimer);
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

    // バッファロック後のバッファーフルを検知したら終了点推定
    if(buffer.isLocked && buffer.currentStatus == BUFFER_FULL){
        // 終了点が見つかったらCAN送信バッファに追加
        if(detectEndPoint()){
            // TODO: CANバッファ作る
        }
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

    // バッファに突っ込む
    Item item;
    initItem(*item);
    item.value = volts; // TODO: Item構造体の中身どうする?
    pushStat = push(B, item);

    // 開始点を検知したらバッファをロック
    if(detectStartPoint()){
        lockBuffer(B);
    }    
}

// TODO: 開始点と終了点の関数ほとんど同じじゃない? 比較部だけ関数化して分けちゃって良くねえか

// 開始点推定 (Return: 現在のheadが開始点かどうか)
bool detectStartPoint(){
    // n個取り出して、平均増加率がプラスかつデータの増加量が一定以上なら開始点と断定
    const unsigned int sampleLength = 10; // 参照するサンプルの長さ
    double aveDiff = 0.00; // 平均変化率
    double offset = 0.00; // 増加量
    const unsigned double border = 300; // 閾値(mV)

    // TODO: initItem、可変長引数サポートしたい(embedBufferと互換切りしてCPPでクラスにしちまう手はある)
    Item item, prevItem; // 現在参照しているアイテムと、その一つ前のアイテム
    Item firstItem, lastItem; // 開始点検知に用いるサンプル範囲の最初と最後の一のアイテム
    initItem(&item);
    initItem(&prevItem);
    initItem(&firstItem);
    initItem(&lastItem);
    
    // サンプル範囲の最初と最後のアイテムを取得し、offsetを計算
    getItemAt(B, 0, &firstItem);
    getItemAt(B, sampleLength, &lastItem);
    offset = lastItem.value - firstItem.value;

    // 平均変化率を計算
    for (unsigned int i = 0; i < sampleLength; i++){
        getItemAt(B, i, &prevItem);
        getItemAt(B, i + 1, &item);
        double diff = item.value - prevItem.value;
        aveDiff = (aveDiff + diff) / 2;
    }

    // TODO: aveDiffデバッグ出力した方が良くない?
    bool result = (aveDiff > 0) && (offset > border);
    return result;
}

// 終了点推定 (Return: 終了点のheadから数えたインデックス)
unsigned int detectEndPoint(){
    // n個取り出して、平均変化率がマイナスかつ一定以上低下していれば終了点と確定
    const unsigned int sampleLength = 10; // 参照するサンプルの長さ
    double aveDiff = 0.00; // 平均変化率
    double offset = 0.00; // 増加量
    const unsigned double border = -120; // 閾値(mV)

    Item item, prevItem; // 現在参照しているアイテムと、その一つ前のアイテム
    Item firstItem, lastItem; // 開始点検知に用いるサンプル範囲の最初と最後の一のアイテム
    initItem(&item);
    initItem(&prevItem);
    initItem(&firstItem);
    initItem(&lastItem);
    
    // サンプル範囲の最初と最後のアイテムを取得し、offsetを計算
    getItemAt(B, 0, &firstItem);
    getItemAt(B, sampleLength, &lastItem);
    offset = lastItem.value - firstItem.value;

    // 平均変化率を計算
    for (unsigned int i = 0; i < sampleLength; i++){
        getItemAt(B, i, &prevItem);
        getItemAt(B, i + 1, &item);
        double diff = item.value - prevItem.value;
        aveDiff = (aveDiff + diff) / 2;
    }

    // TODO: aveDiffデバッグ出力した方が良くない?
    bool result = (aveDiff < 0) && (offset < border);
    return result;

}

// CANバッファ消化
void sendCanBuffer(){
    // TODO: バッファ作ってここでpopさせてcansendする
    // TODO: CANライブラリ調達
}

