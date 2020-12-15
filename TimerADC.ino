/*
 * タイマ割込みでADCをいじる
*/
#include "Buffer.h"
#include "ADCAccessor.h"

// ADC定数
#define PGA 1
#define VREF 2.048
#define ADS1120_CS_PIN   16
#define ADS1120_DRDY_PIN 4
#define ADC_BUFFER_SIZE 100

// 割り込みベクタサイズ
#define INTVECT_SIZE 5

// インタフェース
ADCAccessor adc(ADS1120_CS_PIN, ADS1120_DRDY_PIN);
hw_timer_t *timer = NULL;

// ユーザ定義割込み関数
void dumpADCValue();
void buffering();

// ブレーキ時間推定関数
bool detectStartPoint(Buffer *B, int sampleLen, int sense, int lowerBorder);
int getEndPoint(Buffer *B, int nSense);
void getDiff(Buffer *B, Buffer *Diff);

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

    // タイマ割り込み(1ms)設定
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &onBufTimer, true);
    timerAlarmWrite(timer, 1000, true);
    timerAlarmEnable(timer);

/*

    // ADCストリームバッファ初期化
    B = &adcStreamBuffer;
    initBuffer(B, ADC_BUFFER_SIZE);

    // ADCバッファリング割り込み(1ms)有効化
    bufTimer = timerBegin(0, 80, true);
    timerAttachInterrupt(bufTimer, &onBufTimer, true);
    timerAlarmWrite(bufTimer, 1000, true);
    timerAlarmEnable(bufTimer);
*/
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

    /*

    // バッファがロックされていて、バッファフルを検知したら
    if(buffer.isLocked && buffer.currentStatus == BUFFER_OVER){
        // 終了点推定してブレーキ時間に変換し、
        int endPoint = getEndPoint(B, -20);

        // CAN送信バッファに送りつける


        // アンロック
        unlockBuffer(B);
    }

    */

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

/*
    // バッファに突っ込む
    Item item;
    initItem(*item);
    item.value = volts;
    pushStat = push(B, item);

    // 開始点を検知したらバッファをロック
    if(B->isLocked == 0 && detectStartPoint(B, 4, 17, 2)){
        lockBuffer(B);
    }
*/
}

/// 測定開始点を推定する。
/// サンプル範囲の最初と最後の差が一定以上 && 最初の値が一定以下なら開始点と判断する。
///  - Parameters:
///     - *B : 操作対象のバッファ
///     - sampleLen: サンプル長
///     - sense: 最初と最後の差
///     - lowerBorder: 最初の値の上限値
///  - Return: 開始点ならばtrue そうでなければfalse
bool detectStartPoint(Buffer *B, int sampleLen, int sense, int lowerBorder){
    Item firstItem, lastItem;
    initItem(&firstItem);
    initItem(&lastItem);

    // サンプル取得
    int bufStatus = BUFFER_OK;
    bufStatus = getItemAt(B, 0, &firstItem);
    bufStatus = getItemAt(B, sampleLen, &lastItem);

    // サンプリングできなければ早期return
    if (bufStatus == BUFFER_OVER){
        return false;
    }

    // 計算
    double diff = lastItem.value - firstItem.value;
    bool result = (diff > sense) && (firstItem.value < lowerBorder);

    if(result){
        printf("startpoint: diff %.3lf\n", diff);
    }

    return result;
}

/// 測定終了点を推定する。
/// 二階微分した値の最小値を終了点と判断する。
///  - Parameters:
///     - *B : 操作対象のバッファ
///     - nSense: 最小値上限(この値より小さいものはスパイクとして扱い、無視)
///     - sense: 最初と最後の差
///     - lowerBorder: 最初の値の上限値
///  - Return: 開始点ならばtrue そうでなければfalse
int getEndPoint(Buffer *B, int nSense){
    Buffer diff, bydiff, *Diff, *Bydiff;
    Diff = &diff;
    Bydiff = &bydiff;
    initBuffer(Diff, ADC_BUFFER_SIZE);
    initBuffer(Bydiff, ADC_BUFFER_SIZE);

    // 二階微分
    getDiff(B, Diff);
    getDiff(Diff, Bydiff);

    // 最小値を終了点とする
    Item item;
    initItem(&item);
    getItemAt(Bydiff, 0, &item);
    double tmp = item.value;
    int minValue = -1;
    for (int i = 0; i < ADC_BUFFER_SIZE; i++){
        initItem(&item);
        getItemAt(Bydiff, i, &item);
        // 前の値より小さく, nSenseは下回らない -> minValue
        if (tmp > item.value && item.value > nSense && item.value < 0){
            minValue = i;
            tmp = item.value;
        }
    }
    return minValue;
}

/// 入力バッファを微分したバッファを返す。
///  - Parameters:
///     - *B : 操作対象のバッファ
///     - *Diff : Bを微分したバッファ
void getDiff(Buffer *B, Buffer *Diff){
    Item BItem, DItem;
    initItem(&BItem);
    initItem(&DItem);
    getItemAt(B, 0, &BItem);
    double tmp = BItem.value;

    for (int i = 0; i < B->size; i++){
        // 元のバッファから取得
        getItemAt(B, i, &BItem);

        // 微分して格納
        DItem.value = BItem.value - tmp;
        push(Diff, DItem);
        tmp = BItem.value;
    }
}

