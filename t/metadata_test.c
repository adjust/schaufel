#include "schaufel.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"
#include "test/test.h"
#include "utils/metadata.h"

bool callback(Message msg)
{
    if(msg == NULL)
        return false;
    return true;
}

int main()
{
    Metadata meta = NULL;
    MDatum res = NULL;
    Datum num;
    num.value = calloc(1,sizeof(num.value));
    *num.value = 0xffff;

    Datum d;
    d.func = &callback;
    MDatum md0 = mdatum_init(MTYPE_FUNC,d,sizeof(d.func));
    res = metadata_insert(&meta,"callback",md0);
    pretty_assert(res != NULL);

    d.string = strdup("hurz");
    MDatum md1 = mdatum_init(MTYPE_STRING,d,strlen("hurz"));
    res = metadata_insert(&meta,"1",md1);
    pretty_assert(res != NULL);
    if (res == NULL) goto error;
    pretty_assert(strncmp((res->value).string,"hurz",4) == 0);

    d.string = strdup("huch");
    MDatum md2 = mdatum_init(MTYPE_STRING,d,strlen("huch"));
    metadata_insert(&meta,"2",md2);
    d.string = strdup("moep");
    MDatum md3 = mdatum_init(MTYPE_STRING,d,strlen("moep"));
    metadata_insert(&meta,"3",md3);
    d.string = strdup("argh");
    MDatum md4 = mdatum_init(MTYPE_STRING,d,strlen("argh"));
    metadata_insert(&meta,"4",md4);
    d.string = strdup("blah");
    MDatum md5 = mdatum_init(MTYPE_STRING,d,strlen("blah"));
    metadata_insert(&meta,"5",md5);
    d.string = strdup("blub");
    MDatum md6 = mdatum_init(MTYPE_STRING,d,strlen("blub"));
    metadata_insert(&meta,"6",md6);
    MDatum md7 = mdatum_init(MTYPE_INT,num,sizeof(*num.value));
    metadata_insert(&meta,"7",md7);

    /*
    // Test max elements (8)
    MDatum md8 = mdatum_init(MTYPE_STRING,strdup("qoui"),strlen("quoi"));
    res = metadata_insert(&meta,"8",md8);
    pretty_assert(res != NULL);
    //res = metadata_insert(&meta,"9",md8);
    //pretty_assert(res == NULL);
    */

    // Find integer
    res = metadata_find(&meta,"7");
    pretty_assert(res != NULL);
    if (res == NULL) goto error;
    pretty_assert(res->type == MTYPE_INT);
    pretty_assert(*(res->value).value == 0xffff);

    // Find string
    res = metadata_find(&meta,"4");
    pretty_assert(res != NULL);
    pretty_assert(res->type == MTYPE_STRING);
    pretty_assert(strncmp((res->value).string,"argh",4) == 0);

    res = metadata_find(&meta,"callback");
    pretty_assert(res != NULL);
    if (res == NULL) goto error;
    pretty_assert(res->type == MTYPE_FUNC);
    Message msg = message_init();
    pretty_assert(res->value.func(msg) == true);
    pretty_assert(metadata_callback_run(&meta,msg) == true);
    message_free(&msg);

    error:
    metadata_free(&meta);
    return 0;
}
