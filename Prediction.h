/*
 * 推定関数
*/
#include "Buffer.h"

// ブレーキ時間推定関数
bool detectStartPoint(Buffer *B, int sampleLen, int sense, int lowerBorder);
int getEndPoint(Buffer *B, int nSense);
void getDiff(Buffer *B, Buffer *Diff);

