/* Very simple queue
*  * These are FIFO queues which discard the new data when full.
*   *
*    * Queue is empty when in == out.
*     * If in != out, then 
*      *  - items are placed into in before incrementing in
*       *  - items are removed from out before incrementing out
*        * Queue is full when in == (out-1 + QUEUE_SIZE) % QUEUE_SIZE;
*         *
*          * The queue will hold QUEUE_ELEMENTS number of items before the
*           * calls to QueuePut fail.
*            */

#include "queue.h"

void QueueInit(Queue_t *q ) {
	    q->QueueIn = q->QueueOut = 0;
}

int QueuePut(Queue_t *q,void *new)
{
	    if(q->QueueIn == (( q->QueueOut - 1 + QUEUE_SIZE) % QUEUE_SIZE))
		        {
				        return -1; /* Queue Full*/
					    }

	        q->Queue[q->QueueIn] = new;

		    q->QueueIn = (q->QueueIn + 1) % QUEUE_SIZE;

		        return 0; // No errors
}

int QueueGet(Queue_t *q,void **old)
{
	    if(q->QueueIn == q->QueueOut)
		        {
				        return -1; /* Queue Empty - nothing to get*/
					    }

	        *old = q->Queue[q->QueueOut];

		    q->QueueOut = (q->QueueOut + 1) % QUEUE_SIZE;

		        return 0; // No errors
}
int QueueCount(Queue_t *q ) {
	int diff = q->QueueIn - q->QueueOut;
	diff += ( 0 > diff ) ? QUEUE_SIZE : 0;
	return	diff;
}
