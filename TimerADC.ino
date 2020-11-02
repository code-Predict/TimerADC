/*
 * タイマ割込みでADCをいじる
 */

#include "Protocentral_ADS1220.h"
#include <SPI.h>

#define PGA 1
#define VREF 2.048
#define VFSR VREF/PGA
#define FSR (((long int)1<<23)-1)

#define ADS1220_CS_PIN    16
#define ADS1220_DRDY_PIN  4

float readADCValue(int ledPin);

// variables
volatile int interruptCounter;
int totalInterruptCounter;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

Protocentral_ADS1220 pc_ads1220;
int32_t adc_data;

// ISR
void IRAM_ATTR onDataReady() {
    portENTER_CRITICAL_ISR(&timerMux);
    interruptCounter++;
    portEXIT_CRITICAL_ISR(&timerMux);
}

void setup(){
    Serial.begin(115200);

    pinMode(32, OUTPUT);

    // ADc初期化
    pc_ads1220.begin(ADS1220_CS_PIN,ADS1220_DRDY_PIN);
    pc_ads1220.set_data_rate(DR_1000SPS);
    pc_ads1220.set_pga_gain(PGA_GAIN_1);
    pc_ads1220.select_mux_channels(MUX_AIN0_AIN1);

    pc_ads1220.Start_Conv();

    // 割込み有効化
    attachInterrupt(ADS1220_DRDY_PIN, &onDataReady, FALLING);
//    attachInterrupt(ADS1220_DRDY_PIN, &onDataReady, LOW);
}

unsigned long prevMicros = micros(); // 変換前のmillis

void loop(){

    if (interruptCounter > 0) {
        portENTER_CRITICAL(&timerMux);
        interruptCounter--;
        portEXIT_CRITICAL(&timerMux);

        totalInterruptCounter++;

        adc_data = readADCValue(32);
        float Vout = (float)((adc_data * VFSR * 1000) / FSR);
        
        unsigned long startMicros = micros();
        Serial.print(Vout);
        Serial.print("         ");
        Serial.print(",");
        Serial.println((startMicros - prevMicros)/1000.0);
        prevMicros = micros();
    }
}

// ADCから値を読み込む 値が無効なら直近のものを返す
float readADCValue(int ledPin = -1){
    static float value = 0;

    float tmp = pc_ads1220.Read_WaitForData();
    if(tmp == 0){
      if(ledPin > 0){
          digitalWrite(ledPin, 1); 
      }
      return value;
    }
    value = tmp;
    return value;
}
