#include <stdlib.h>
#include <stdbool.h>

typedef int64_t ITEM;

struct circ_buf {
    ITEM *buf;
    int head;
    int tail;
};

struct ringbuf_s {
    struct circ_buf cb;
    size_t size;
};
typedef struct ringbuf_s ringbuf_t;

ringbuf_t *ringbuf_new(size_t count);
void ringbuf_del(ringbuf_t *rb);
int ringbuf_append(ringbuf_t *rb, ITEM item);
int ringbuf_append_with_overwrite(ringbuf_t *rb, ITEM item);
int ringbuf_read(ringbuf_t *rb, ITEM *buf, size_t count);
size_t ringbuf_read_to_end(ringbuf_t *rb, ITEM *buf, size_t count);
bool ringbuf_peek(ringbuf_t *rb, ITEM *item);
size_t ringbuf_capacity(ringbuf_t *rb);

/* ringbuffer tests */
void ringbuf_init_test(void);
void ringbuf_simple_test(void);
void ringbuf_append_test(void);
void ringbuf_append_wrap_test(void);
void ringbuf_append_overwrite_test(void);
void ringbuf_read_test(void);
void ringbuf_read_to_end_test(void);
