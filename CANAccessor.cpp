/*
 * CANアクセサ
*/
#include "CANAccessor.h"

/// CSピンを設定してCANアクセサを初期化
///  - Parameters:
///     - csPin: CSピンの番号
CANAccessor::CANAccessor(byte csPin, byte intPin){
    this->csPin = csPin;
    this->intPin = intPin;
}

/// モード、速度、クロック周波数を指定してCANインタフェースを有効化
///  - Parameters:
///     - mode: 動作モード (定数 MCP_XXX)
///     - speedRate: 通信速度 (定数 CAN_XXXBPS)
///     - clockFreq: クロック周波数 (定数 MCP_XXXMHZ)
///  - Return: 初期化に成功したら0を返す
int CANAccessor::begin(uint8_t mode, uint8_t speedRate, uint8_t clockFreq){
    *(this->can) = MCP_CAN(csPin);
    return !(this->can->begin(mode, speedRate, clockFreq) == CAN_OK);
}

/// 通常、スリープ、ループバックなどの動作モードを設定
///  - Parameter:
///     - mode: 動作モード (定数 MCP_XXX)
void CANAccessor::setMode(uint8_t mode){
    this->can->setMode(mode);
}

/// ID, データ, データ長を設定してCANフレームを送信
///  - Parameters:
///     - id: 調停ID
///     - data: 送信フレーム
///     - length: フレーム長
///  - Return: 送信結果
int CANAccessor::send(uint32_t id, uint8_t *data, uint8_t length){
    return this->can->sendMsgBuf(id, 0, length, data);
}

/// 受信フレームを取得(INTピン割り込みによる利用を想定)
///  - Parameters:
///     - id: 調停ID
///     - data: 送信フレーム
///     - length: フレーム長
///  - Return: 未定
int CANAccessor::recv(uint32_t *id, uint8_t *data, uint8_t *length){
    return 0;
}
