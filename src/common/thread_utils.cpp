#include "thread_utils.h"

#include <pthread.h>

#include <cassert>
#include <cstdio>

void ThreadMutex::Lock() {
    pthread_mutex_lock(&mutex_);
}
void ThreadMutex::Unlock() {
    pthread_mutex_unlock(&mutex_);
}

void ThreadCond::Broadcast() {
    pthread_cond_broadcast(&cond_);
}
void ThreadCond::Wait(ThreadMutex& mutex) {
    pthread_cond_wait(&cond_, &mutex.mutex_);
}

PubThreadMutex::PubThreadMutex() : mutex_() {
    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);

    pthread_mutex_init(&mutex_, &mattr);
    pthread_mutexattr_destroy(&mattr);
}
void PubThreadMutex::Lock() {
    pthread_mutex_lock(&mutex_);
}
void PubThreadMutex::Unlock() {
    pthread_mutex_unlock(&mutex_);
}

PubThreadCond::PubThreadCond() : cond_() {
    pthread_condattr_t cattr;
    pthread_condattr_init(&cattr);
    pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);

    pthread_cond_init(&cond_, &cattr);
    pthread_condattr_destroy(&cattr);
}
void PubThreadCond::Broadcast() {
    pthread_cond_broadcast(&cond_);
}
void PubThreadCond::Wait(PubThreadMutex& mutex) {
    pthread_cond_wait(&cond_, &mutex.mutex_);
}
