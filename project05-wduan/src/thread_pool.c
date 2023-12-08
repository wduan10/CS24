#include "thread_pool.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "queue.h"

typedef struct work {
    work_function_t work_function;
    void *aux;
} work_t;

typedef struct thread_pool {
    pthread_t *threads;
    queue_t *queue;
    size_t num_worker_threads;
} thread_pool_t;

void *execute_work(void *aux) {
    queue_t *queue = (queue_t *) aux;
    while (1) {
        work_t *work = (work_t *) queue_dequeue(queue);
        if (work == NULL) {
            return NULL;
        }

        work_function_t work_function = work->work_function;
        work_function(work->aux);
        free(work);
    }
    return NULL;
}

thread_pool_t *thread_pool_init(size_t num_worker_threads) {
    thread_pool_t *pool = malloc(sizeof(thread_pool_t));
    pool->queue = queue_init();
    pool->num_worker_threads = num_worker_threads;

    pool->threads = malloc(sizeof(pthread_t) * num_worker_threads);
    for (size_t i = 0; i < num_worker_threads; i++) {
        pthread_create(&(pool->threads[i]), NULL, execute_work, pool->queue);
    }
    return pool;
}

void thread_pool_add_work(thread_pool_t *pool, work_function_t function, void *aux) {
    work_t *work = malloc(sizeof(work_t));
    work->work_function = function;
    work->aux = aux;
    queue_enqueue(pool->queue, (void *) work);
}

void thread_pool_finish(thread_pool_t *pool) {
    for (size_t i = 0; i < pool->num_worker_threads; i++) {
        queue_enqueue(pool->queue, NULL);
    }

    for (size_t i = 0; i < pool->num_worker_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    queue_free(pool->queue);
    free(pool->threads);
    free(pool);
}