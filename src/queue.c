#include "queue.h"
#include <assert.h>
#include <stdlib.h>

// test whether an unsigned value is a power of 2 (or zero)
#define ispow2(x)	(((x) & ((x) - 1)) == 0)

struct queue {
	int size;
	atomic_int head;
	atomic_int tail;
	void * data[1];
};

static inline int *
queue_int(struct queue *q) {
	return (int *)(q->data[0]);
}

static inline void **
queue_ptr(struct queue *q) {
	return (void **)(&q->data[0]);
}

static struct queue *
queue_new(int size, int stride) {
	assert(ispow2((unsigned)size));
	size_t sz = offsetof(struct queue, data) + stride * size;
	struct queue *q = (struct queue *)malloc(sz);
	if (q == NULL)
		return NULL;
	q->size = size;
	atomic_int_init(&q->head, 0);
	atomic_int_init(&q->tail, 0);
	return q;
}

struct queue *
queue_new_int(int size) {
	return queue_new(size, sizeof(int));
}

struct queue *
queue_new_ptr(int size) {
	return queue_new(size, sizeof(void *));
}

void
queue_delete(struct queue *q) {
	free(q);
}

static inline int
queue_position(struct queue *q, int p) {
	return p & (q->size - 1);
}

static inline int
queue_push_open(struct queue *q) {
	int tail = atomic_int_load(&q->tail);
	if (queue_position(q, tail + 1) == atomic_int_load(&q->head))
		return -1;
	return tail;
}

static inline void
queue_push_close(struct queue *q, int tail) {
	int succ = atomic_int_cas(&q->tail, tail, queue_position(q, tail + 1));
	assert(succ);	// Allow only one writer
}

static inline int
queue_pop_open(struct queue *q) {
	int head = atomic_int_load(&q->head);
	if (head == atomic_int_load(&q->tail))
		return -1;
	return head;
}

static inline void
queue_pop_close(struct queue *q, int head) {
	int succ = atomic_int_cas(&q->head, head, queue_position(q, head + 1));
	assert(succ);	// Allow only one reader
}

int
queue_push_int(struct queue *q, int v) {
	assert(v != 0);
	int tail = queue_push_open(q);
	if (tail < 0)
		return 1;
	int *data = queue_int(q);
	data[tail] = v;
	queue_push_close(q, tail);
	return 0;
}

int
queue_pop_int(struct queue *q) {
	int head = queue_pop_open(q);
	if (head < 0)
		return 0;
	int *data = queue_int(q);
	int v = data[head];
	queue_pop_close(q, head);
	return v;
}

int
queue_push_ptr(struct queue *q, void *v) {
	assert(v != NULL);
	int tail = queue_push_open(q);
	if (tail < 0)
		return 1;
	void **data = queue_ptr(q);
	data[tail] = v;
	queue_push_close(q, tail);
	return 0;
}

void *
queue_pop_ptr(struct queue *q) {
	int head = queue_pop_open(q);
	if (head < 0)
		return 0;
	void **data = queue_ptr(q);
	void *v = data[head];
	queue_pop_close(q, head);
	return v;
}

int
queue_length(struct queue *q) {
	int len = q->tail - q->head;
	if (len < 0)
		len += q->size;
	return len;
}