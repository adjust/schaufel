#include <utils/logger.h>
#include <queue.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <producer.h>
#include <schaufel.h>

int run   = 1;
int ready = 0;
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
    printf("usage\n"
           "-i      : input  (consumer)\n"
           "-o      : output (producer)\n"
           "                k : kafka\n"
           "                d : dummy\n"
           "                r : redis\n"
           "                f : file\n"
           "-c      : consumer_threads\n"
           "-p      : producer_threads\n"
           "-b | -B : consumer / producer broker (only kafka)\n"
           "-g | -G : consumer / producer groupid (only kafka)\n"
           "-h | -H : consumer / producer host (only redis)\n"
           "-q | -Q : consumer / producer port (only redis)\n"
           "-t | -T : consumer / producer topic\n"
           "                (used as list name for redis)\n"
           "                (used as topic name for kafka)\n"
           "-f | -F : consumer / producer filename (only file)\n"
           "\n");
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

    logger_log("waiting for producer to come up");
    while (!ready)
        sleep(1);
    logger_log("producer are up");

    while(run)
    {
        if (consumer_consume(c, msg) == -1)
            return NULL;
        if (message_get_data(msg) != NULL)
        {
            queue_add(q, message_get_data(msg), 1);
            //give up ownership
            message_set_data(msg, NULL);
        }
    }
    message_free(&msg);
    consumer_free(&c);
    run = 0;
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

    // at least on producer ready
    ready = 1;

    int ret = 0;
    while(42)
    {
        if (!run && ret == ETIMEDOUT)
            break;
        ret = queue_get(q, msg);

        if (ret == ETIMEDOUT)
            continue;

        if (message_get_data(msg) != NULL)
        {
            //TODO: check success
            producer_produce(p, msg);
            //message was handled: free it
            free(message_get_data(msg));
            message_set_data(msg, NULL);
        }
    }
    message_free(&msg);
    producer_free(&p);
    return NULL;
}

int
main(int argc, char **argv)
{
    int opt;
    int consumer_threads = 0,
        producer_threads = 0;

    void *res;

    pthread_t *c_thread;
    pthread_t *p_thread;

    Options o;
    memset(&o, '\0', sizeof(o));

    while ((opt = getopt(argc, argv, "l:i:o:c:p:b:h:q:g:t:f:B:H:Q:G:T:F:")) != -1)
    {
        switch (opt)
        {
            case 'l':
                o.logger = optarg;
                break;
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
    signal(SIGTERM, stop);

    logger_init(o.logger);

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
