/*
 * 処理
*/
#include "Buffer.h"

int push(Buffer *buffer, Item item){
    int *tail = &(buffer->tail);
    int *head = &(buffer->head);

    // tailの次=head -> 満杯
    if (((*tail) + 1) % (buffer->size + 1) == *head){
        // ロックされていればBUFFER_FULLを返す
        if (buffer->isLocked != 0){
            buffer->currentStatus = BUFFER_FULL;
            return buffer->currentStatus;
        }

        // popすることでバッファがいっぱいになるのを防ぐ
        Item dust;
        pop(buffer, &dust);
    }

    // index自動振り
    item.index = (buffer->currentIndex)++;

    // 末尾に追加し、ポインタを進める 配列の終端にきたら先頭に戻る
    buffer->data[*tail] = item;
    (*tail)++;
    if (*tail > buffer->size){
        *tail = 0;
    }
    buffer->currentStatus = BUFFER_OK;
    return buffer->currentStatus;
}

int pop(Buffer *buffer, Item *item){
    int *tail = &(buffer->tail);
    int *head = &(buffer->head);

    // head==tail -> キューは空
    if (*head == *tail){
        buffer->currentStatus = BUFFER_EMPTY;
        return buffer->currentStatus;
    }

    // 先頭から取り出し、ポインタを進める 配列の終端にきたら先頭に戻る
    *item = buffer->data[*head];
    (*head)++;
    if (*head > buffer->size){
        *head = 0;
    }
    buffer->currentStatus = BUFFER_OK;
    return buffer->currentStatus;
}

// headから指定位置先のデータを読み出す
int getItemAt(Buffer *buffer, unsigned int advanced, Item *item){
    int tail = (buffer->tail);
    int head = (buffer->head);
    int size = buffer->size;

    // headとtailを比較し、有効データ数を計算
    unsigned int validRange = 0;
    if (tail > head){
        // tailの方が後にある -> tailまで読み出し可能
        validRange = (tail - 1) - head;
    }else{
        // headの方が後か、headとtailが同じところにある -> sizeまで読み出した後、0~tailまで読み出し可能
        validRange = size - head + tail;
    }

    // 有効データ数の範囲外
    if (advanced > validRange){
        return BUFFER_OVER;
    }

    // advancedを加算して返す
    *item = buffer->data[(head + advanced) % (size + 1)];
    return BUFFER_OK;
}
