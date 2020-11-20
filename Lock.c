/*
 * バッファの操作ロック
*/
#include "Buffer.h"

void lockBuffer(Buffer *buffer){
    buffer->isLocked = 1;
}

void unlockBuffer(Buffer *buffer){
    buffer->isLocked = 0;
    buffer->currentIndex = 0; // バッファがアンロックされたらインデックスを初期化する
}
