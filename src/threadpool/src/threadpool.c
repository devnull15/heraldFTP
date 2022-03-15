#include <threadpool.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <limits.h>

#define MAX_NSEC 999999999

/**
 * @brief custom free function to be supplied as a pointer
 *         to the linked list to free the job appropriately
 *
 * @param p - pointer to the node to be freed
 *
 * @return nothing
 *
 */
static void _jq_free_job(void *p);

/**
 * @brief initialized the job queue and the underlying linked
 *        list
 *
 * @return pointer to the initialized job queue; NULL on error
 *
 */
static jobqueue *_jq_init();

/**
 * @brief frees all resources used by the supplied job queue
 *
 * @param jq - job queue to be freed
 *
 * @return 0 on suceess; nonzero on error
 *
 */
static int _jq_destroy(jobqueue *jq);

/**
 * @brief helper function to join all threads in pool->threads as
 *        part of the shutdown process
 *
 * @param pool - pointer to thread pool
 *
 * @return 0 on suceess; nonzero on error
 *
 */
static int _thread_joinall(threadpool *pool);

/**
 * @brief helper function to implement an exponential backoff
 *        for worker threads waiting for a job
 *
 * @param req - struct for nanosleep() that contains the current
 *        amount of time the thread is sleeping
 *
 * @return a struct timespec with the new amount of time for nanosleep()
 *
 */
static struct timespec _thread_exp_backoff(struct timespec req);

/**
 * @brief function provided to worker threads; causes threads to either
 *        dequeue a job on job queue and execute it or wait for a
 *        job to populate the queue if the queue is empty
 *
 * @param threadpool_in - pointer to thread pool the thread is a part of
 *
 * @return always NULL
 *
 */
static void *_thread_exec(void *threadpool_in);

/* PUBLIC FUNCTION DEFINTIONS */
int
thpool_add_job(threadpool *pool, void (*jobdef)(void *), void *args)
{
    int       ret = 0;
    job *     j   = NULL;
    jobqueue *jq  = NULL;
    node_free f   = _jq_free_job;

    if (NULL == pool || NULL == jobdef)
    {
        fprintf(stderr,
                "! threadpool_add_job: can't have NULL pool or NULL task\n");
        ret = -1;
        goto ERR;
    }

    jq = pool->jq;
    if (NULL == jq)
    {
        fprintf(stderr, "! threadpool_add_job: can't have NULL jq\n");
        ret = -1;
        goto ERR;
    }

    j         = calloc(1, sizeof(struct job_));
    j->jobdef = jobdef;
    j->args   = args;

    pthread_mutex_lock(&(jq->lock));
    push_back(jq->queue, j, f);
    jq->len++;
    pthread_mutex_unlock(&(jq->lock));

ERR:
    return ret;
}

threadpool *
thpool_init(int nthreads)
{
    threadpool *pool = NULL;
    threadpool *ret  = NULL;
    int         err  = 0;

    pool = calloc(1, sizeof(struct threadpool_));
    if (NULL == pool)
    {
        fprintf(stderr, "! threadpool_init: couln't malloc pool\n");
        goto ERR;
    }

    pool->threads = calloc(nthreads, sizeof(pthread_t));
    if (NULL == pool->threads)
    {
        perror("! threadpool_init: couln't calloc jobqueue\n");
        goto ERR;
    }

    pool->jq = _jq_init();
    if (NULL == pool->jq)
    {
        fprintf(stderr, "! threadpool_init: error with _jq_init\n");
        goto ERR;
    }

    pool->keepalive = 1;
    pool->nthreads  = nthreads;

    for (int i = 0; i < nthreads; i++)
    {
#ifndef NDEBUG
        fprintf(stderr, "\n ** starting thread %u **\n", i);
#endif /* NDEBUG */

        err = pthread_create(&(pool->threads[i]), NULL, _thread_exec, pool);
        if (0 != err)
        {
            perror("! threadpool_init: couln't calloc jobqueue\n");
            goto ERR;
        }
    }

    ret  = pool;
    pool = NULL;

ERR:
    err = _thread_joinall(pool);
    if (0 != err)
    {
        fprintf(stderr, "! threadpool_destroy: error in joining threads\n");
    }
    if (NULL != pool)
    {
        err = _jq_destroy(pool->jq);
        if (0 != err)
        {
            perror("! threadpool_init: couldn't destroy jobqueue\n");
        }
        pool->jq = NULL;
        free(pool->threads);
        pool->threads = NULL;
    }
    free(pool);
    pool = NULL;
    return ret;
}

int
thpool_destroy(threadpool *pool)
{
    int ret = 0;

    if (NULL == pool)
    {
        fprintf(stderr, "! threadpool_destroy: NULL pool\n");
        ret = -1;
        goto ERR;
    }

    pool->keepalive = 0;

    ret = _thread_joinall(pool);
    if (0 != ret)
    {
        fprintf(stderr, "! threadpool_destroy: error in joining threads\n");
        goto ERR;
    }

ERR:
    ret = _jq_destroy(pool->jq);
    if (0 != ret)
    {
        perror("! threadpool_destroy: error destroying job queue\n");
    }
    pool->jq = NULL;
    free(pool->threads);
    pool->threads = NULL;
    free(pool);
    pool = NULL;
    return ret;
}
/* PUBLIC FUNCTION DEFINTIONS */

/* PRIVATE FUNCTION DEFINTIONS */
static void
_jq_free_job(void *p)
{
    node *n = (node *)p;
    free(n->data);
    n->data = NULL;
    n->next = NULL;
    n->f    = NULL;
    free(n);
    return;
}

static jobqueue *
_jq_init()
{
    jobqueue *ret = NULL;
    jobqueue *jq  = NULL;
    int       err = 0;

    jq = calloc(1, sizeof(struct jobqueue_));
    if (NULL == jq)
    {
        fprintf(stderr, "! _jq_init: couln't calloc jobqueue\n");
        goto ERR;
    }

    jq->queue = ll_init();
    if (NULL == jq->queue)
    {
        fprintf(stderr, "! _jq_init: couln't init queue\n");
        goto ERR;
    }

    err = pthread_mutex_init(&(jq->lock), NULL);
    if (0 != err)
    {
        perror("! jq_init: couln't init mutex\n");
        goto ERR;
    }

    ret = jq;
    jq  = NULL;
ERR:
    if (NULL != jq)
    {
        err = ll_destroy(jq->queue);
        if (0 != err)
        {
            fprintf(stderr, "! jq_init: couldn't destroy queue\n");
        }

        err = pthread_mutex_destroy(&(jq->lock));
        if (0 != err)
        {
            fprintf(stderr, "! jq_init: couldn't destroy mutex\n");
        }
    }
    free(jq);
    jq = NULL;

    return ret;
}

static int
_jq_destroy(jobqueue *jq)
{
    int ret = 0;

    if (NULL == jq)
    {
        fprintf(stderr, "! jq_destroy: NULL job queue\n");
        ret = -1;
        goto ERR;
    }

    ret = ll_destroy(jq->queue);
    if (0 > ret)
    {
        fprintf(stderr, "! jq_destroy: couln't destroy queue\n");
    }
    jq->queue = NULL;

    ret = pthread_mutex_destroy(&(jq->lock));
    if (0 != ret)
    {
        perror("! jq_destroy: couln't destroy mutex\n");
    }

ERR:
    free(jq);
    jq = NULL;

    return ret;
}

static int
_thread_joinall(threadpool *pool)
{
    int ret = 0;

    if (NULL == pool)
    {
        goto RET;
    }

    for (uint i = 0; i < pool->nthreads; i++)
    {
#ifndef NDEBUG
        fprintf(stderr, " ** join thread %i **\n", i);
#endif // NDEBUG
        ret = pthread_join(pool->threads[i], NULL);
        if (0 != ret)
        {
            perror("! threadpool_destroy: join error\n");
        }
    }

RET:
    return ret;
}

static struct timespec
_thread_exp_backoff(struct timespec req)
{
    req.tv_nsec = req.tv_nsec * 2;
    if (MAX_NSEC < req.tv_nsec)
    {
        if (INT_MAX > req.tv_sec)
        {
            req.tv_sec++;
            req.tv_nsec = 0;
        }
    }

    return req;
}

static void *
_thread_exec(void *threadpool_in)
{
    threadpool *    pool = (threadpool *)threadpool_in;
    void *          ret  = NULL;
    int             err  = 0;
    struct timespec req  = { 0, 1 };
    struct timespec rem  = { 0, 0 };
    job *           j    = NULL;
    jobqueue *      jq   = NULL;

#ifndef NDEBUG
    fprintf(stderr, " ** _thread_exec **\n");
#endif // NDEBUG

    if (pool == NULL || pool->jq == NULL)
    {
        fprintf(stderr, "! _thread_exec: pool or jq is NULL\n");
        goto ERR;
    }
    jq = pool->jq;

    while (pool->keepalive)
    {
        if (0 >= jq->len)
        {
            err = nanosleep(&req, &rem);
            if (0 != err)
            {
                perror("! _thread_exec: nanosleep terminated early\n");
            }
            req = _thread_exp_backoff(req);
            sched_yield();
        }
        else
        {
            pthread_mutex_lock(&(jq->lock));
            if (0 < jq->len)
            {
                j = pop_front(jq->queue);
                jq->len--;
            }
            pthread_mutex_unlock(&(jq->lock));
            if (NULL == j)
            {
#ifndef NDEBUG
                fprintf(stderr, "! _thread_exec: jq empty\n");
#endif // NDEBUG
                goto ERR;
            }

            (j->jobdef)(j->args);
            free(j);
            j = NULL;
        }
    }

#ifndef NDEBUG
    fprintf(stderr, " ** _thread_exec done **\n");
#endif // NDEBUG

ERR:
    return ret;
}
/* PRIVATE FUNCTION DEFINITIONS */
