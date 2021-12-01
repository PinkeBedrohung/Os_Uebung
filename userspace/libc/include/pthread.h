#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

//pthread typedefs
typedef size_t pthread_t;
typedef unsigned int pthread_attr_t;
enum
{
  PTHREAD_CANCEL_ENABLE,PTHREAD_CANCEL_DISABLE

};
enum
{
  PTHREAD_CANCEL_DEFERRED, PTHREAD_CANCEL_ASYNCHRONOUS
};
//pthread mutex typedefs
//typedef unsigned int pthread_mutex_t;
typedef unsigned int pthread_mutexattr_t;

//pthread spinlock typedefs
typedef unsigned int pthread_spinlock_t;

//pthread cond typedefs
typedef unsigned int pthread_cond_t;
typedef unsigned int pthread_condattr_t;

typedef struct
{
    int value;
    int init;
    int owner;
} pthread_mutex_t;

extern int pthread_create(pthread_t *thread,
         const pthread_attr_t *attr, void *(*start_routine)(void *),
         void *arg);

extern void entry_function(void* (*start_routine)(void*), void* arg);

extern void pthread_exit(void *value_ptr);

extern int pthread_cancel(pthread_t thread);

extern int pthread_setcanceltype(int type, int *oldtype);
extern int pthread_setcancelstate(int type, int *oldtype);
extern pthread_t pthread_self(void);
extern int pthread_join(pthread_t thread, void **value_ptr);

extern int pthread_detach(pthread_t thread);

extern int pthread_self(void);

extern int pthread_mutex_init(pthread_mutex_t *mutex,
                              const pthread_mutexattr_t *attr);

extern int pthread_mutex_destroy(pthread_mutex_t *mutex);

extern int pthread_mutex_lock(pthread_mutex_t *mutex);

extern int pthread_mutex_unlock(pthread_mutex_t *mutex);

extern int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr);

extern int pthread_cond_destroy(pthread_cond_t *cond);

extern int pthread_cond_signal(pthread_cond_t *cond);

extern int pthread_cond_broadcast(pthread_cond_t *cond);

extern int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);

#ifdef __cplusplus
}
#endif


