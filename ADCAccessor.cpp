/*
 * ADCアクセサ
*/
#include "ADCAccessor.h"

// コンストラクタ
ADCAccessor::ADCAccessor(uint8_t DRDYpin, uint8_t CSpin){
    // プロパティ初期化
    isEnabled = false;

    // ピン初期化
    DRDYpin = DRDYpin;
    CSpin = CSpin;
    pinMode(DRDYpin, INPUT);
    pinMode(CSpin, OUTPUT);
}

// ADC初期化
void ADCAccessor::begin(uint8_t speed, uint8_t gain, uint8_t mux){
    pc_ads1220.begin(this->DRDYpin, this->CSpin);
    pc_ads1220.set_data_rate(speed);
    pc_ads1220.set_pga_gain(gain);
    pc_ads1220.select_mux_channels(mux);
    pc_ads1220.Start_Conv();
}

// ADC有効化
void ADCAccessor::enable(){
    pc_ads1220.Start_Conv();
    this->isEnabled = true;
}

// ADCから値を読み込む 値が無効なら直近のものを返す
float ADCAccessor::readADCValue(){
    static float value = 0;
    if(isEnabled){
        float tmp = pc_ads1220.Read_WaitForData();
        value = (tmp == 0) ? value : tmp;
        return value;
    }else{
        return 0;
    }
}
