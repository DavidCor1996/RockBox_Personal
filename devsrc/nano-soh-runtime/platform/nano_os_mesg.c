/* Cooperative queues for the single-thread game loop, following LUS's
 * desktop queue contract. OS_MESG_BLOCK never sleeps the RetailOS UI task. */
#include "libultraship/libultra.h"
#include <stddef.h>

__OSEventState __osEventStateTab[OS_NUM_EVENTS];

void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *buffer, s32 count)
{
    mq->validCount = 0;
    mq->first = 0;
    mq->msgCount = buffer && count > 0 ? count : 0;
    mq->msg = buffer;
}

s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flag)
{
    s32 index;
    (void)flag;
    if (mq->msgCount <= 0 || mq->validCount >= mq->msgCount) return -1;
    index = (mq->first + mq->validCount) % mq->msgCount;
    mq->msg[index] = msg;
    mq->validCount++;
    return 0;
}

s32 osJamMesg(OSMesgQueue *mq, OSMesg msg, s32 flag)
{
    (void)flag;
    if (mq->msgCount <= 0 || mq->validCount >= mq->msgCount) return -1;
    mq->first = (mq->first + mq->msgCount - 1) % mq->msgCount;
    mq->msg[mq->first] = msg;
    mq->validCount++;
    return 0;
}

s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flag)
{
    (void)flag;
    if (mq->validCount <= 0 || mq->msgCount <= 0) return -1;
    if (msg) *msg = mq->msg[mq->first];
    mq->first = (mq->first + 1) % mq->msgCount;
    mq->validCount--;
    return 0;
}

void osSetEventMesg(OSEvent event, OSMesgQueue *mq, OSMesg msg)
{
    if ((unsigned)event >= OS_NUM_EVENTS) return;
    __osEventStateTab[event].queue = mq;
    __osEventStateTab[event].msg = msg;
}
