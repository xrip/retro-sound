#ifndef __fifo_H__
#define __fifo_H__

#define fifo_typedef(T, NAME) \
  typedef struct { \
    int size; \
    int start; \
    int end; \
    int write_count;\
    int read_count;\
    T* elems; \
  } NAME

#define fifo_init(BUF, S, T, BUFMEM) \
  BUF->size = S; \
  BUF->start = 0; \
  BUF->end = 0; \
  BUF->read_count=0;\
  BUF->write_count=0;\
  BUF->elems = (T*)BUFMEM

#define fifo_write(BUF, ELEM)\
    BUF->elems[BUF->end]=ELEM;\
    BUF->write_count++;\
    BUF->end=(BUF->end+1)%BUF->size;

#define fifo_read(BUF, ELEM)\
    ELEM=BUF->elems[BUF->start];\
    BUF->read_count++;\
    BUF->start=(BUF->start+1)%BUF->size;

#define fifo_peek(BUF,ELEM,INDEX)\
    ELEM=BUF->elems[BUF->start+INDEX];

#define fifo_flush(BUF)\
    BUF->start = 0; \
    BUF->end = 0; \
    BUF->read_count=0;\
    BUF->write_count=0;

#define fifo_count(BUF) (BUF->write_count-BUF->read_count)
#define fifo_is_full(BUF) (fifo_count(BUF)==BUF->size)
#define fifo_is_empty(BUF) (fifo_count(BUF)==0)
#define fifo_overflow(BUF) (fifo_count(BUF)>=BUF->size)

#endif