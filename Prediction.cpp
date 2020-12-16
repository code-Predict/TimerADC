/*
 * 推定関数 
*/
#include "Prediction.h"

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
    unsigned int bufSize = getBufSize(B);
    initBuffer(Diff, bufSize);
    initBuffer(Bydiff, bufSize);

    // 二階微分
    getDiff(B, Diff);
    getDiff(Diff, Bydiff);

    // 最小値を終了点とする
    Item item;
    initItem(&item);
    getItemAt(Bydiff, 0, &item);
    double tmp = item.value;
    int minValue = -1;
    for (int i = 0; i < bufSize; i++){
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

