#include <utils/logger.h>
#include <queue.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <producer.h>
#include <schaufel.h>

int run = 1;
Queue q;

static void
stop(int sig)
{
    logger_log("received signal to stop");
    run = 0;
}

void
print_usage()
{
    printf("lala\n");
    exit(1);
}

void *
consume(void *arg)
{
    Message msg = message_init();
    Consumer c = consumer_init(((Options *)arg)->input, arg);
    if (c == NULL)
    {
        logger_log("%s %d: could not init consumer", __FILE__, __LINE__);
        return NULL;
    }
    while(run)
    {
        consumer_consume(c, msg);
        queue_add(q, message_get_data(msg), 1);
    }
    message_free(&msg);
    consumer_free(&c);
    return NULL;
}

void *
produce(void *arg)
{
    Message msg = message_init();
    Producer p = producer_init(((Options *)arg)->output, arg);
    if (p == NULL)
    {
        logger_log("%s %d: could not init producer", __FILE__, __LINE__);
        return NULL;
    }
    int ret = 0;
    while(42)
    {
        if (!run && ret == ETIMEDOUT)
            break;
        ret = queue_get(q, msg);
        if (ret == ETIMEDOUT)
            continue;
        producer_produce(p, msg);
        free(message_get_data(msg));
    }
    message_free(&msg);
    producer_free(&p);
    return NULL;
}

int
main(int argc, char **argv)
{
    logger_init();
    int opt;
    int consumer_threads = 0,
        producer_threads = 0;

    void *res;

    pthread_t *c_thread;
    pthread_t *p_thread;

    Options o;
    memset(&o, '\0', sizeof(o));

    while ((opt = getopt(argc, argv, "i:o:c:p:b:h:q:g:t:f:B:H:Q:G:T:F:")) != -1)
    {
        switch (opt)
        {
            case 'i':
                o.input = optarg[0];
                break;
            case 'o':
                o.output = optarg[0];
                break;
            case 'c':
                consumer_threads += atoi(optarg);
                break;
            case 'p':
                producer_threads += atoi(optarg);
                break;
            case 'b':
                o.in_broker = optarg;
                break;
            case 'h':
                o.in_host = optarg;
                break;
            case 'q':
                o.in_port = atoi(optarg);
                break;
            case 'g':
                o.in_groupid = optarg;
                break;
            case 't':
                o.in_topic = optarg;
                break;
            case 'f':
                o.in_file = optarg;
                break;
            case 'B':
                o.out_broker = optarg;
                break;
            case 'H':
                o.out_host = optarg;
                break;
            case 'Q':
                o.out_port = atoi(optarg);
                break;
            case 'G':
                o.out_groupid = optarg;
                break;
            case 'T':
                o.out_topic = optarg;
                break;
            case 'F':
                o.out_file = optarg;
                break;
            default:
                print_usage();
        }
    }

    if (!consumer_threads || !producer_threads)
        print_usage();

    signal(SIGINT, stop);

    q = queue_init();

    c_thread = calloc(consumer_threads, sizeof(*c_thread));
    for (int i = 0; i < consumer_threads; ++i)
        pthread_create(&(c_thread[i]), NULL, consume, &o);

    p_thread = calloc(producer_threads, sizeof(*p_thread));
    for (int i = 0; i < producer_threads; ++i)
        pthread_create(&(p_thread[i]), NULL, produce, &o);

    for (int i = 0; i < consumer_threads; ++i)
        pthread_join(c_thread[i], &res);

    for (int i = 0; i < producer_threads; ++i)
        pthread_join(p_thread[i], &res);

    free(c_thread);
    free(p_thread);
    queue_free(&q);

    logger_log("done");
    return 0;
}
