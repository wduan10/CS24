#include "queue.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct node_t {
    void *value;
    struct node_t *left;
    struct node_t *right;
} node_t;

typedef struct queue {
    node_t *head;
    node_t *tail;
    size_t size;

    pthread_mutex_t lock;
    pthread_cond_t cv;
} queue_t;

node_t *node_init(void *value) {
    node_t *node = malloc(sizeof(node_t));
    node->value = value;
    node->left = NULL;
    node->right = NULL;
    return node;
}

queue_t *queue_init() {
    queue_t *queue = malloc(sizeof(queue_t));
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    pthread_mutex_init(&(queue->lock), NULL);
    pthread_cond_init(&(queue->cv), NULL);
    return queue;
}

void queue_enqueue(queue_t *queue, void *value) {
    pthread_mutex_lock(&(queue->lock));

    node_t *node = node_init(value);
    if (queue->head == NULL) {
        queue->head = node;
        queue->tail = node;
        queue->size = 1;
    }
    else {
        node_t *tail = queue->tail;
        tail->right = node;
        node->left = tail;
        queue->tail = node;
        queue->size++;
    }

    pthread_cond_signal(&(queue->cv));
    pthread_mutex_unlock(&(queue->lock));
}

void *queue_dequeue(queue_t *queue) {
    pthread_mutex_lock(&(queue->lock));
    if (queue == NULL) {
        return NULL;
    }

    while (queue->size == 0) {
        pthread_cond_wait(&(queue->cv), &(queue->lock));
    }

    if (queue->size == 1) {
        node_t *head = queue->head;
        void *output = head->value;
        free(head);

        queue->head = NULL;
        queue->tail = NULL;
        queue->size = 0;

        pthread_mutex_unlock(&(queue->lock));
        return output;
    }
    else {
        node_t *head = queue->head;
        void *output = head->value;
        node_t *nxt = head->right;
        nxt->left = NULL;
        free(head);

        queue->head = nxt;
        queue->size--;

        pthread_mutex_unlock(&(queue->lock));
        return output;
    }
}

void queue_free(queue_t *queue) {
    free(queue);
}
