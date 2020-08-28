
/* Queue structure */
#define QUEUE_ELEMENTS 100
#define QUEUE_SIZE (QUEUE_ELEMENTS + 1)
typedef struct {
	void *Queue[QUEUE_SIZE];
	int QueueIn, QueueOut;
} Queue_t;

extern void QueueInit(Queue_t *q );
extern int QueuePut(Queue_t *q,void *new);
extern int QueueGet(Queue_t *q,void **old);
extern int QueueCount(Queue_t *q );
