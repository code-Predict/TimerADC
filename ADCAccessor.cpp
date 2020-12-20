/*
 * ADCアクセサ
*/
#include "ADCAccessor.h"

/// CS,DRDYピンを設定してADCアクセサを初期化
///  - Parameters:
///     - CSpin: CSピンの番号
///     - DRDYpin: DRDYピンの番号
ADCAccessor::ADCAccessor(uint8_t CSpin, uint8_t DRDYpin){
    this->DRDYpin = DRDYpin;
    this->CSpin = CSpin;
    pinMode(DRDYpin, INPUT);
    pinMode(CSpin, OUTPUT);

    this->adcValue = 0.00;
}

/// 変換レートとマルチプレクサを設定
///  - Parameters:
///     - speedRate: 変換レート (定数, DR_xxxx_SPS)
///     - mux: マルチプレクサ (定数, MUX_xxx_xxx)
void ADCAccessor::begin(uint8_t speedRate, uint8_t mux){
    pc_ads1220.begin(this->CSpin, this->DRDYpin);
    pc_ads1220.set_data_rate(speedRate);
    pc_ads1220.select_mux_channels(mux);
}

/// PGA設定
///  - Parameters:
///     - gain: ゲイン値 (定数,PGA_GAIN_xxx PGAを使用しない場合は-1)
void ADCAccessor::setGain(int gain){
    if(gain >= 0){
        pc_ads1220.set_pga_gain(gain);
        pc_ads1220.PGA_ON();
    }else{
        pc_ads1220.PGA_OFF();
    }
}

/// 変換モード設定
///  - Parameters:
///     - mode: 変換モード (0: シングルショット 1: 継続)
void ADCAccessor::setConvMode(int mode){
    if(mode == 0){
        pc_ads1220.set_conv_mode_single_shot();
    }else{
        pc_ads1220.set_conv_mode_continuous();
    }
}

/// リファレンス電圧設定
///  - Parameters:
///     - ref:リファレンス電圧(定数: ADCACCESSOR_REF_XXX)
void ADCAccessor::setReference(uint8_t ref){
    // レジスタの値を取得
    uint8_t reg2Value = pc_ads1220.readRegister(CONFIG_REG2_ADDRESS);

    // 上位2bitに持っていく
    uint8_t refValue = ref << 6;

    // クリアしてアサート
    reg2Value &= 0x3F;
    reg2Value |= refValue;

    // 設定
    pc_ads1220.writeRegister(CONFIG_REG2_ADDRESS, reg2Value);

}

/// AD変換開始
void ADCAccessor::startConv(){
    pc_ads1220.Start_Conv();
}

/// 最後に取得した値を返す
float ADCAccessor::getADCValue(){
    return this->adcValue;
}

/// ADCから値を読む
void ADCAccessor::updateADCValue(){
    this->adcValue = pc_ads1220.Read_WaitForData();
}
