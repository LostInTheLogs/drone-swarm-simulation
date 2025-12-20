#include "thread_utils.h"

#include <pthread.h>

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
