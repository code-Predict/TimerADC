/*
 * FIFOキュー 
*/
#ifndef _BUFFER_H_
#define _BUFFER_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* -------- */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NDEBUG

// #define BUFFER_SIZE 100

#define BUFFER_OK 0
#define BUFFER_FULL 1
#define BUFFER_EMPTY 2
#define BUFFER_OVER 3

#ifndef uint8_t
    #define uint8_t unsigned char
#endif

/* -------- */
typedef struct item {
    int id;
    uint8_t data[8];
} Item;

typedef struct buffer {
    // Data
    Item *data;
    int head;
    int tail;
    unsigned int size;
    unsigned int isLocked; // 0以外の値を代入するとバッファがロックされる
} Buffer;

/* -------- */

// Initializer.c
int initBuffer(Buffer* buffer, unsigned int length);
int deinitBuffer(Buffer* buffer);
void initItem(Item *item);

// Dump.c
void dumpItem(Item *item, char* strBuffer);
void dumpBuffer(Buffer *buffer, char* strBuffer);
void dumpu8Array(uint8_t *data, int length, char *strBuffer);

// Operate.c
int push(Buffer* buffer, Item item);
int pop(Buffer* buffer, Item* item);
int getItemAt(Buffer* buffer, unsigned int advanced, Item* item);

// Lock.c
void lockBuffer(Buffer *buffer);
void unlockBuffer(Buffer *buffer);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _BUFFER_H_ */