/*
 * ダンプ
*/
#include "Buffer.h"

// require: max 31 * buffer->size  bytes
void dumpBuffer(Buffer *buffer, char* strBuffer){
    int idx = 0;
    for(int i = 0; i < buffer->size; i++){
        char itemBuf[35];
        dumpItem(&(buffer->data[i]), itemBuf);
        idx += sprintf(strBuffer + idx, "%s\n", itemBuf);
    }
}

// require: max 31 bytes
void dumpItem(Item *item, char* strBuffer){
    char dataBuf[30];
    dumpu8Array(item->data, 8, dataBuf);
    sprintf(strBuffer, "%02X: %s", item->id, dataBuf);
}

// require: max 25 bytes
void dumpu8Array(uint8_t *data, int length, char *strBuffer){
    int idx = 0;
    for(int i = 0; i < length; i++){
        idx += sprintf(strBuffer + idx, "%02X ", data[i]);
    }
}
