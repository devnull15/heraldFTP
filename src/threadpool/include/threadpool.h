// ref: https://github.com/Pithikos/C-Thread-Pool/blob/master/thpool.h

#ifndef _THREADPOOL_H
#define _THREADPOOL_H

#include <stdint.h>
#include <pthread.h>
#include <ll.h>
#include <stdatomic.h>

/**
 * @brief struct for a job or task to be put into the job queue
 *        and executed by the threads in the pool
 *
 * @param jobdef - function definition of the job
 *
 * @param args - pointer to arguments for the job function
 *
 */
typedef struct job_
{
    void (*jobdef)(void *);
    void *args;
} job;

/**
 * @brief contains a linked list of jobs and a lock to ensure
 *        thread safety
 *
 * @param lock - mutex to prevent race conditions on the queue
 *
 * @param queue - pointer to the head of the linked list / queue
 *
 * @param len - current number of job nodes in the queue
 *
 */
typedef struct jobqueue_
{
    pthread_mutex_t lock;
    ll *            queue;
    atomic_uint     len;
} jobqueue;

/**
 * @brief struct for the thread pool contains a number of worker
 *        threads and a job queue; threads execute jobs on the queue
 *        in a FIFO manner; the number of threads cannot be changed
 *        after initialization
 *
 * @param threads - array of worker threads
 *
 * @param jq - pointer to the job queue
 *
 * @param keepalive - used to signal the threads to stop waiting for jobs
 *
 * @param nthreads - number of worker threads in the pool
 *
 */
typedef struct threadpool_
{
    pthread_t * threads;
    jobqueue *  jq;
    atomic_bool keepalive;
    atomic_uint nthreads;
} threadpool;
/* STRUCTS */

/**
 * @brief initilizes the thread pool
 *
 * @param num_threads - number of threads for the pool
 *
 * @return pointer to intilized thread pool or NULL on error
 *
 */
threadpool *thpool_init(int num_threads);

/**
 * @brief add jobs to the thread pool
 *
 * @param pool - pointer to thread pool
 *
 * @param jobdef - pointer to the function definition of the job being added
 *
 * @param args - pointer to args for the job function
 *
 * @return 0 on success; nonzero on error
 *
 */
int thpool_add_job(threadpool *pool, void (*jobdef)(void *), void *args);

/**
 * @brief stops execution of the thread pool;
 *        frees resources and join threads
 *
 * @param pool - thread pool to be stopped and freed
 *
 * @return 0 on success; nonzero on error
 *
 */
int thpool_destroy(threadpool *pool);

#endif /* _THREADPOOL_H */
