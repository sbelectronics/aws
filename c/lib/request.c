/* request.c - making a request from a request block, as CTOS's own request
 * interface does: the response comes back to the process's default
 * response exchange, as a message pointing at the block. Any other message
 * waiting there is thrown away, so make one request at a time. */
#include <ctosmsg.h>

ErcType DoRequest(void *pRq)
{
    RqHeader *rq = pRq;
    void __far *msg;
    Exch exch;
    ErcType erc;

    if ((erc = QueryDefaultRespExch(&exch)) != 0)
        return erc;
    rq->exchResp = exch;
    rq->ercRet = 0;
    if ((erc = Request(rq)) != 0)
        return erc;
    do {
        if ((erc = Wait(exch, &msg)) != 0)
            return erc;
    } while (msg != (void __far *)rq);
    return rq->ercRet;
}
