/* ctosmsg.h - CTOS exchanges and messages, for processes and programs that
 * talk to each other.
 *
 * An exchange is a numbered mailbox. A message is any 4-byte value, usually
 * a far pointer to memory both sides can reach; CTOS queues it without
 * looking at what it points to, and the sender must leave that memory alone
 * until the receiver is done with it.
 *
 * Programs in different partitions, on a CTOS with them, find each other's
 * exchanges through the partition calls (see the CTOS manual,
 * "Interpartition communication").
 * Between cluster workstations there is no direct Send: the documented route
 * is the Queue Manager on the master (AddQueueEntry, MarkNextQueueEntry). */
#ifndef CTOSMSG_H
#define CTOSMSG_H

#include <ctos.h>

typedef unsigned Exch;

/* Kernel primitives */
extern ErcType __far Send(Exch exchange, void __far *pMsg);
extern ErcType __far Wait(Exch exchange, void __far * __far *ppMsgRet);
extern ErcType __far Check(Exch exchange, void __far * __far *ppMsgRet);
extern ErcType __far Request(void __far *pRq);
#pragma aux (CTOSCALL) Send;
#pragma aux (CTOSCALL) Wait;
#pragma aux (CTOSCALL) Check;
#pragma aux (CTOSCALL) Request;

/* A request block starts with this header. After it come sCntInfo bytes of
 * control information, then nReqPbCb and nRespPbCb pairs of a far pointer
 * and a word count. */
#pragma pack(push, 1)
typedef struct {
    unsigned char sCntInfo, rtCode, nReqPbCb, nRespPbCb;
    unsigned userNum;       /* 0: CTOS fills it in */
    Exch exchResp;
    ErcType ercRet;
    unsigned rqCode;
} RqHeader;
#pragma pack(pop)

/* Sends the request block and waits for the response; returns its error
 * code. This reaches any request, including those numbered 1000h and up,
 * which have no fixed address to call. */
ErcType DoRequest(void *pRq);

/* Requests */
extern ErcType __far AllocExch(Exch __far *pExchRet);
extern ErcType __far DeallocExch(Exch exchange);
#pragma aux (CTOSCALL) AllocExch;
#pragma aux (CTOSCALL) DeallocExch;

/* OS subroutine: the exchange CTOS gave this process for responses */
extern ErcType __far QueryDefaultRespExch(Exch __far *pExchRet);
#pragma aux (CTOSCALL) QueryDefaultRespExch;

#define ercNoMessage 14     /* from Check: nothing is waiting */

#endif
