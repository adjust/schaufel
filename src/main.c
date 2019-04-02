#include <utils/logger.h>
#include <queue.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <producer.h>
#include <utils/helper.h>
#include <version.h>

/* Schaufel keeps track of consume and produce states.
 *
 * consume_state is statically initialised to true (for convenience).
 * produce_state is initialised on runtime when at least one producer is up.
 *
 * Use get_state(&state) to query a state atomically.
 * Use set_state(&state, true|false) to set/unset a state atomically.
 */
static volatile atomic_bool consume_state = ATOMIC_VAR_INIT(true);
static volatile atomic_bool produce_state;

Queue q;

static void
stop(UNUSED int sig)
{
    logger_log("received signal to stop");
    set_state(&consume_state, false);
}

NORETURN void
print_usage()
{
    printf("usage\n"
           "-C      : config file\n"
           "-i      : input  (consumer)\n"
           "-o      : output (producer)\n"
           "                d : dummy\n"
           "                f : file\n"
           "                k : kafka\n"
           "                p : postgres\n"
           "                r : redis\n"
           "-c      : consumer_threads\n"
           "-p      : producer_threads\n"
           "-b | -B : consumer / producer broker (only kafka)\n"
           "-g | -G : consumer / producer groupid (only kafka)\n"
           "-h | -H : consumer / producer host (only redis and postgres)\n"
           "        :       Format is a comma separated list of <host>:<port>\n"
           "        :       For postgres, replicas may be submitted as a secondary\n"
           "        :       list in the same format. Two lists must be separated\n"
           "        :       by a semicolon.\n"
           "-t | -T : consumer / producer topic\n"
           "                (used as list name for redis)\n"
           "                (used as topic name for kafka)\n"
           "                (used as generation id for postgres)\n"
           "-f | -F : consumer / producer filename (only file)\n"
           "-s | -S : consumer / producer pipeline batch size (only redis)\n"
           "        :       Requires exactly one integer argument (default: 0)\n"
           "        :       0 disables pipelining\n"
           "        :       Redis: 10k is upstream recommended max\n"
           "-l      : path to the log file\n"
           "-V      : print version\n"
           "\n");
    exit(1);
}

NORETURN void
print_version()
{
    printf("schaufel version: "
            _SCHAUFEL_VERSION
            "\n");
    exit(0);
}

void *
stats(UNUSED void *arg)
{
    long added     = 0;
    long delivered = 0;
    while (get_state(&consume_state))
    {
        long secs_used,micros_used;
        struct timeval start, end;
        gettimeofday(&start, NULL);
        added = queue_added(q);
        delivered = queue_delivered(q);
        sleep(5);
        gettimeofday(&end, NULL);
        secs_used=(end.tv_sec - start.tv_sec);
        micros_used= ((secs_used*1000000) + end.tv_usec) - (start.tv_usec);
        logger_log("added / s: %ld delivered / s: %ld",added * 1000000 / micros_used, delivered * 1000000 / micros_used);
    }
    return NULL;
}

void *
consume(void *config)
{
    Message msg = message_init();
    const char *consumer_type = NULL;
    config_setting_lookup_string((config_setting_t *) config,
        "type", &consumer_type);

    if (msg == NULL)
    {
        logger_log("%s %d: could not init message", __FILE__, __LINE__);
        return NULL;
    }
    Consumer c = consumer_init(*consumer_type,
        (config_setting_t *) config);
    if (c == NULL)
    {
        logger_log("%s %d: could not init consumer", __FILE__, __LINE__);
        return NULL;
    }

    logger_log("waiting for producer to come up");
    while (!get_state(&produce_state))
        sleep(1);
    logger_log("producer are up");

    while (get_state(&consume_state))
    {
        if (consumer_consume(c, msg) == -1)
            break;
        if (message_get_data(msg) != NULL)
        {
            queue_add(q, message_get_data(msg), message_get_len(msg), 1);
            //give up ownership
            message_set_data(msg, NULL);
        }
    }
    message_free(&msg);
    consumer_free(&c);
    return NULL;
}

void *
produce(void *config)
{
    Message msg = message_init();
    const char *producer_type = NULL;
    config_setting_lookup_string((config_setting_t *) config,
        "type", &producer_type);
    Producer p = producer_init(*producer_type,
        (config_setting_t *) config);
    if (p == NULL)
    {
        logger_log("%s %d: could not init producer", __FILE__, __LINE__);
        return NULL;
    }

    // at least one producer ready
    set_state(&produce_state, true);

    int ret = 0;
    while(42)
    {
        if (!get_state(&consume_state) && ret == ETIMEDOUT)
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

void
init_threads(config_t* config, int type, pthread_t *threads)
{
    uint64_t list, thread_index;
    void *(*function)(void *);
    config_setting_t *croot = NULL, *parent = NULL, *instance = NULL;
    croot = config_root_setting(config);

    switch(type)
    {
        case (SCHAUFEL_TYPE_CONSUMER):
            parent = config_setting_get_member(croot, "consumers");
            function = &consume;
            break;
        case (SCHAUFEL_TYPE_PRODUCER):
            parent = config_setting_get_member(croot, "producers");
            function = &produce;
            break;
        default:
            abort();
    }

    list = config_setting_length(parent);
    for (uint64_t i = 0; i < list; ++i)
    {
        instance = config_setting_get_elem(parent, i);
        if(instance == NULL)
            abort();
        config_setting_lookup_int(instance, "threads", (int *) &thread_index);
        for (uint64_t j = 0; j < thread_index; j++)
        {
            if(pthread_create(threads++, NULL, function, instance))
            {
                fprintf(stderr, "%s %d: pthread create failed!\n",
                    __FILE__, __LINE__);
                abort();
            }
        }
    }
}

int
main(int argc, char **argv)
{
    int opt;
    int consumer_threads = 0,
        producer_threads = 0,
        r_c_threads   = 0,
        r_p_threads   = 0;

    void *res;

    pthread_t *c_thread;
    pthread_t *p_thread;
    pthread_t stat_thread;

    Options o;
    memset(&o, '\0', sizeof(o));

    config_t config;
    config_init(&config);

    while ((opt = getopt(argc, argv, "l:i:o:c:p:b:h:g:t:f:s:B:C:H:G:T:F:S:V")) != -1)
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
                o.consumer_threads = consumer_threads;
                break;
            case 'p':
                producer_threads += atoi(optarg);
                o.producer_threads = producer_threads;
                break;
            case 'b':
                o.in_broker = optarg;
                break;
            case 'h':
                o.in_host = optarg;
                o.in_hosts =  parse_hostinfo_master(o.in_host);
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
            case 's':
                o.in_pipeline = atoi(optarg);
                break;
            case 'B':
                o.out_broker = optarg;
                break;
            case 'C':
                o.config = optarg;
                break;
            case 'H':
                o.out_host = optarg;
                o.out_hosts =  parse_hostinfo_master(o.out_host);
                o.out_hosts_replica =  parse_hostinfo_replica(o.out_host);
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
            case 'S':
                o.out_pipeline = atoi(optarg);
                break;
            case 'V':
                print_version();
            default:
                print_usage();
        }
    }

    config_merge(&config, o);

    if(!config_validate(&config)) {
        if(!o.config)
            print_usage();
        // else suggest manpage?
        exit(1);
    }

    logger_init(config_lookup(&config, "logger"));

    signal(SIGINT, stop);
    signal(SIGTERM, stop);

    q = queue_init();
    if (!q) {
        logger_log("%s %d: Failed to init queue\n", __FILE__, __LINE__);
        abort();
    }

    r_c_threads = get_thread_count(&config, "consumers");
    c_thread = calloc(r_c_threads, sizeof(*c_thread));
    if (!c_thread) {
        logger_log("%s %d: Failed to calloc: %s\n", __FILE__, __LINE__, strerror(errno));
        abort();
    }

    init_threads(&config, SCHAUFEL_TYPE_CONSUMER, c_thread);

    r_p_threads = get_thread_count(&config, "producers");
    p_thread = calloc(r_p_threads, sizeof(*p_thread));
    if (!p_thread) {
        logger_log("%s %d: Failed to calloc: %s\n", __FILE__, __LINE__, strerror(errno));
        abort();
    }

    init_threads(&config, SCHAUFEL_TYPE_PRODUCER, p_thread);
    pthread_create(&stat_thread, NULL, stats, NULL);

    for (int i = 0; i < r_c_threads; ++i)
        pthread_join(c_thread[i], &res);
    set_state(&consume_state, false);

    for (int i = 0; i < r_p_threads; ++i)
        pthread_join(p_thread[i], &res);
    set_state(&produce_state, false);

    pthread_join(stat_thread, &res);

    free(c_thread);
    free(p_thread);
    queue_free(&q);
    config_destroy(&config);
    logger_log("done");
    return 0;
}
