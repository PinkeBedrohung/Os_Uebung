#include "pthread.h"
#include "../../../common/include/kernel/syscall-definitions.h"
#include "sys/syscall.h"
#include "sched.h"
/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg)
{
  return (int)__syscall(sc_pthread_create, (size_t)thread, (size_t)attr, (size_t)start_routine, (size_t)arg, (size_t) entry_function);
}

void entry_function(void* (*start_routine)(void*), void* arg)
{
    pthread_exit(start_routine(arg));
}

// return the tid of the current thread aka the one calling the function
pthread_t pthread_self(void)
{
  return __syscall(sc_pthreadself, 0x0, 0x0, 0x0, 0x0, 0x0);
}
/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
void pthread_exit(void *value_ptr)
{
  __syscall(sc_pthread_exit, (size_t)value_ptr, 0, 0, 0, 0);
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_cancel(pthread_t thread)
{
  return (int)__syscall(sc_pthread_cancel, (size_t)thread, 0x0, 0x0, 0x0, 0x0);
}

int pthread_setcancelstate(int state, int *oldstate)
{
  return (int)__syscall(sc_pthread_setcancelstate, (size_t) state, (size_t)oldstate, 0x0, 0x0, 0x0);
}
int pthread_setcanceltype(int type, int *oldtype)
{
  return (int)__syscall(sc_pthread_setcanceltype, (size_t) type, (size_t)oldtype, 0x0, 0x0, 0x0);
}
/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_join(pthread_t thread, void **value_ptr)
{
  return (int)__syscall(sc_pthread_join, (size_t) thread, (size_t) value_ptr, 0x0, 0x0, 0x0);
}


/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_detach(pthread_t thread)
{
  return (int)__syscall(sc_pthread_detach, (size_t) thread, 0x0, 0x0, 0x0, 0x0);
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
  size_t val = 0;
  if(attr == 0) //default
  {
    if(!mutex || mutex->init == 1) return -1;
    val = 1;
    asm("xchg %0,%1"
    : "=r" (val)
    : "m" (mutex->init), "0" (val)
    : "memory");
    val = 0;
    asm("xchg %0,%1"
    : "=r" (val)
    : "m" (mutex->value), "0" (val)
    : "memory");
    val = 0;
    asm("xchg %0,%1"
    : "=r" (val)
    : "m" (mutex->owner), "0" (val)
    : "memory");
  }
  else{
    // dunno what to do with the attr
    //checks needed
    return -1;
  }
  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
  size_t val = 0;
  // check if used, exist and inited
  if(!mutex || mutex->init == 0 || mutex->owner)
          return -1;
  //reverse init -> set vals to 0
    asm("xchg %0,%1"
    : "=r" (val)
    : "m" (mutex->init), "0" (val)
    : "memory");
    val = 0;
    asm("xchg %0,%1"
    : "=r" (val)
    : "m" (mutex->value), "0" (val)
    : "memory");
    val = 0;
    asm("xchg %0,%1"
    : "=r" (val)
    : "m" (mutex->owner), "0" (val)
    : "memory");

    return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_mutex_lock(pthread_mutex_t *mutex)
{
  if(!mutex || mutex->init == 0 || mutex->owner == pthread_self())
    return -1;
  size_t val = 1;
  if(mutex->owner)
  {
    do
    {
      asm("xchg %0,%1"
      : "=r" (val)
      : "m" (mutex->value), "0" (val)
      : "memory");
    }while(val && !sched_yield());
  }
  val = 1; 
  asm("xchg %0,%1"
  : "=r" (val)
  : "m" (mutex->value), "0" (val)
  : "memory");

  val =  (size_t)pthread_self();
  asm("xchg %0,%1"
  : "=r" (val)
  : "m" (mutex->owner), "0" (val)
  : "memory");

  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
  if(!mutex || mutex->init == 0 || mutex->owner == 0 || mutex->owner != pthread_self())
    return -1;


  size_t val = 0;
  asm("xchg %0,%1"
  : "=r" (val)
  : "m" (mutex->value), "0" (val)
  : "memory");

  val = 0;
  asm("xchg %0,%1"
  : "=r" (val)
  : "m" (mutex->owner), "0" (val)
  : "memory");

  return 0;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
  return -1;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_cond_destroy(pthread_cond_t *cond)
{
  return -1;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_cond_signal(pthread_cond_t *cond)
{
  return -1;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_cond_broadcast(pthread_cond_t *cond)
{
  return -1;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
  return -1;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_spin_destroy(pthread_spinlock_t *lock)
{
  return -1;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_spin_init(pthread_spinlock_t *lock, int pshared)
{
  return -1;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_spin_lock(pthread_spinlock_t *lock)
{
  return -1;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_spin_trylock(pthread_spinlock_t *lock)
{
  return -1;
}

/**
 * function stub
 * posix compatible signature - do not change the signature!
 */
int pthread_spin_unlock(pthread_spinlock_t *lock)
{
  return -1;
}

