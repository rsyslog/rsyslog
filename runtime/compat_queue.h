/*
 * Compatibility helpers for BSD sys/queue.h users.
 *
 * Use the platform STAILQ macros when configure found them.  Some platforms
 * provide sys/queue.h without that subset, so keep the local fallback limited
 * to the macros used by rsyslog.
 */
#ifndef INCLUDED_COMPAT_QUEUE_H
#define INCLUDED_COMPAT_QUEUE_H

#include <stddef.h>
#ifdef HAVE_SYS_QUEUE_H
    #include <sys/queue.h>
#endif

#ifndef HAVE_SYS_QUEUE_STAILQ
    #ifndef STAILQ_HEAD
        #define STAILQ_HEAD(name, type)  \
            struct name {                \
                struct type *stqh_first; \
                struct type **stqh_last; \
            }
    #endif

    #ifndef STAILQ_ENTRY
        #define STAILQ_ENTRY(type)      \
            struct {                    \
                struct type *stqe_next; \
            }
    #endif

    #ifndef STAILQ_INIT
        #define STAILQ_INIT(head)                        \
            do {                                         \
                (head)->stqh_first = NULL;               \
                (head)->stqh_last = &(head)->stqh_first; \
            } while (0)
    #endif

    #ifndef STAILQ_EMPTY
        #define STAILQ_EMPTY(head) ((head)->stqh_first == NULL)
    #endif

    #ifndef STAILQ_FIRST
        #define STAILQ_FIRST(head) ((head)->stqh_first)
    #endif

    #ifndef STAILQ_INSERT_TAIL
        #define STAILQ_INSERT_TAIL(head, elm, field)         \
            do {                                             \
                (elm)->field.stqe_next = NULL;               \
                *(head)->stqh_last = (elm);                  \
                (head)->stqh_last = &(elm)->field.stqe_next; \
            } while (0)
    #endif

    #ifndef STAILQ_REMOVE_HEAD
        #define STAILQ_REMOVE_HEAD(head, field)                                           \
            do {                                                                          \
                if (((head)->stqh_first = (head)->stqh_first->field.stqe_next) == NULL) { \
                    (head)->stqh_last = &(head)->stqh_first;                              \
                }                                                                         \
            } while (0)
    #endif

    #ifndef STAILQ_NEXT
        #define STAILQ_NEXT(elm, field) ((elm)->field.stqe_next)
    #endif

    #ifndef STAILQ_FOREACH
        #define STAILQ_FOREACH(var, head, field) \
            for ((var) = STAILQ_FIRST(head); (var); (var) = STAILQ_NEXT((var), field))
    #endif
#endif

#endif /* #ifndef INCLUDED_COMPAT_QUEUE_H */
