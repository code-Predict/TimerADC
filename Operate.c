/*
 * 処理
*/
#include "Buffer.h"

int push(Buffer* buffer, Item item){
    int *tail = &(buffer->tail);
    int *head = &(buffer->head);

    // tailの次=head -> 満杯
    if(((*tail) + 1) % (buffer->size + 1) == *head){
        // popすることでバッファがいっぱいになるのを防ぐ
        Item dust;
        pop(buffer, &dust);
        // return BUFFER_FULL;
    }

    // 末尾に追加し、ポインタを進める 配列の終端にきたら先頭に戻る
    buffer->data[*tail] = item;
    (*tail)++;
    if(*tail > buffer->size){
        *tail = 0;
    }
    return BUFFER_OK;
}

int pop(Buffer* buffer, Item* item){
    int *tail = &(buffer->tail);
    int *head = &(buffer->head);

    // head==tail -> キューは空
    if(*head == *tail){
        return BUFFER_EMPTY;
    }

    // 先頭から取り出し、ポインタを進める 配列の終端にきたら先頭に戻る
    *item = buffer->data[*head];
    (*head)++;
    if(*head > buffer->size){
        *head = 0;
    }
    return BUFFER_OK;
}