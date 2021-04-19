#ifndef LOKER_H
#define LOKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

class sem{
private:
    sem_t semaphore_sem;
public:
    sem(){
        if(sem_init(&semaphore_sem , 0 , 0) != 0 ){
            throw std::exception();
        }
    }
    sem(int num){
        if(sem_init(&semaphore_sem , 0 , num) != 0){
            throw std::exception();
        }
    }
    ~sem(){
        sem_destroy(&semaphore_sem);
    }
    bool wait(){
        return sem_wait(&semaphore_sem)==0;
    }
    bool post(){
        return sem_post(&semaphore_sem)==0;
    }
};

class locker{
private:
    pthread_mutex_t mutex;
public:
    locker(){
        if(pthread_mutex_init(&mutex , NULL) != 0){
            throw std::exception();
        }
    }
    ~locker(){
        pthread_mutex_destroy(&mutex);
    }
    bool lock(){
        return pthread_mutex_lock(&mutex)==0;
    }
    bool unlock(){
        return pthread_mutex_unlock(&mutex)==0;
    }
    pthread_mutex_t *get(){
        return &mutex;
    }
};



#endif /*LOKER_H*/