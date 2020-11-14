/*
 * 初期化
*/
#include "Buffer.h"

int initBuffer(Buffer* buffer, unsigned int size){
    // バッファアイテム格納用のメモリを確保
    Item *bufmem;
    bufmem = (Item *)malloc(sizeof(Item) * size);
    if(bufmem == NULL) {
        return 1;
    }
    
    // 各アイテムを初期化
    for (int i = 0; i < size; i++){
        initItem(bufmem + i);
    }

    // プロパティ設定
    buffer->head = 0;
    buffer->tail = 0;
    buffer->data = bufmem;
    buffer->size = size;
    return 0;
}

int deinitBuffer(Buffer* buffer){
    free(buffer->data);
    return 0;
}

void initItem(Item* item){
    item->id = 0xFF;
    item->value = 0;
}