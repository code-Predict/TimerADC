/*
 * ADCアクセサ
*/
#include "ADCAccessor.h"

// コンストラクタ
ADCAccessor::ADCAccessor(uint8_t CSpin, uint8_t DRDYpin){

    // ピン初期化
    this->DRDYpin = DRDYpin;
    this->CSpin = CSpin;
    pinMode(DRDYpin, INPUT);
    pinMode(CSpin, OUTPUT);

    this->adcValue = 0.00;
}

// ADC初期化
void ADCAccessor::begin(uint8_t speed, uint8_t gain, uint8_t mux){
    pc_ads1220.begin(this->CSpin, this->DRDYpin);
    pc_ads1220.set_data_rate(speed);
    pc_ads1220.set_pga_gain(gain);
    pc_ads1220.select_mux_channels(mux);
    pc_ads1220.Start_Conv();
}

// ADCの値を取得
float ADCAccessor::getADCValue(){
    return this->adcValue;
}

// ADCの値を更新 無効な値が返されたら無視
void ADCAccessor::updateADCValue(){
    float tmp = pc_ads1220.Read_WaitForData();
    this->adcValue = (tmp != 0) ? tmp : this->adcValue;
}