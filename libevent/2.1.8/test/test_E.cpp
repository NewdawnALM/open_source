// # 1 "./test.cpp"
// # 1 "<built-in>"
// # 1 "<command-line>"
// # 1 "/usr/include/stdc-predef.h" 1 3 4
// # 1 "<command-line>" 2
// # 1 "./test.cpp"

// # 1 "/data/home/mytest/open_source/libevent/2.1.8/libevent-2.1.8-stable/compat/sys/queue.h" 1
// # 3 "./test.cpp" 2

#include <cstdio>

struct queue_entry_t
{
    int value;
    struct {
        struct queue_entry_t *tqe_next;
        struct queue_entry_t **tqe_prev;
    } entry;
};

struct queue_head_t
{
    struct queue_entry_t *tqh_first;
    struct queue_entry_t **tqh_last;
};

int main(int argc, char **argv)
{
    struct queue_head_t queue_head;
    struct queue_entry_t *q, *p, *s, *new_item;
    int i;

    do { (&queue_head)->tqh_first = NULL; (&queue_head)->tqh_last = &(&queue_head)->tqh_first; } while (0);

    for(i = 0; i < 3; ++i)
    {
        p = (struct queue_entry_t*)malloc(sizeof(struct queue_entry_t));
        p->value = i;


        do { (p)->entry.tqe_next = NULL; (p)->entry.tqe_prev = (&queue_head)->tqh_last; *(&queue_head)->tqh_last = (p); (&queue_head)->tqh_last = &(p)->entry.tqe_next; } while (0);
    }

    q = (struct queue_entry_t*)malloc(sizeof(struct queue_entry_t));
    q->value = 10;
    do { if (((q)->entry.tqe_next = (&queue_head)->tqh_first) != NULL) (&queue_head)->tqh_first->entry.tqe_prev = &(q)->entry.tqe_next; else (&queue_head)->tqh_last = &(q)->entry.tqe_next; (&queue_head)->tqh_first = (q); (q)->entry.tqe_prev = &(&queue_head)->tqh_first; } while (0);



    s = (struct queue_entry_t*)malloc(sizeof(struct queue_entry_t));
    s->value = 20;

    do { if (((s)->entry.tqe_next = (q)->entry.tqe_next) != NULL) (s)->entry.tqe_next->entry.tqe_prev = &(s)->entry.tqe_next; else (&queue_head)->tqh_last = &(s)->entry.tqe_next; (q)->entry.tqe_next = (s); (s)->entry.tqe_prev = &(q)->entry.tqe_next; } while (0);


    s = (struct queue_entry_t*)malloc(sizeof(struct queue_entry_t));
    s->value = 30;

    do { (s)->entry.tqe_prev = (p)->entry.tqe_prev; (s)->entry.tqe_next = (p); *(p)->entry.tqe_prev = (s); (p)->entry.tqe_prev = &(s)->entry.tqe_next; } while (0);



    s = ((&queue_head)->tqh_first);
    printf("the first entry is %d\n", s->value);


    s = ((s)->entry.tqe_next);
    printf("the second entry is %d\n\n", s->value);


    do { if (((s)->entry.tqe_next) != NULL) (s)->entry.tqe_next->entry.tqe_prev = (s)->entry.tqe_prev; else (&queue_head)->tqh_last = (s)->entry.tqe_prev; *(s)->entry.tqe_prev = (s)->entry.tqe_next; } while (0);
    free(s);


    new_item = (struct queue_entry_t*)malloc(sizeof(struct queue_entry_t));
    new_item->value = 100;

    s = ((&queue_head)->tqh_first);

    do { if (((new_item)->entry.tqe_next = (s)->entry.tqe_next) != NULL) (new_item)->entry.tqe_next->entry.tqe_prev = &(new_item)->entry.tqe_next; else (&queue_head)->tqh_last = &(new_item)->entry.tqe_next; (new_item)->entry.tqe_prev = (s)->entry.tqe_prev; *(new_item)->entry.tqe_prev = (new_item); } while (0);


    printf("now, print again\n");
    i = 0;
    for((p) = ((&queue_head)->tqh_first); (p) != NULL; (p) = ((p)->entry.tqe_next))
    {
        printf("the %dth entry is %d\n", i, p->value);
    }

    p = (*(((struct queue_head_t *)((&queue_head)->tqh_last))->tqh_last));
    printf("last is %d\n", p->value);

    p = (*(((struct queue_head_t *)((p)->entry.tqe_prev))->tqh_last));
    printf("the entry before last is %d\n", p->value);
}
