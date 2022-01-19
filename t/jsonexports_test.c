#include "schaufel.h"
#include <arpa/inet.h>
#include "test/test.h"
#include "hooks/jsonexport.h"
#include "queue.h"
#include "utils/config.h"
#include "utils/metadata.h"

int main()
{
    int res = 0;
    config_t root;
    config_setting_t *jsonexports = NULL;
    config_init(&root);
    res = config_read_string(&root,
        "hook = { type = \"jsonexport\","
        "jpointers = ("
        "   \"/text\","
        "   [ \"/timestamp\", \"timestamp\" ], "
        "   [  \"/token\", \"text\", \"store_meta\" ],"
        "   [  \"/recursive/discard\", \"text\", \"discard_true\", \"exists\" ]"
        "   );"
        "};");
    pretty_assert(res == CONFIG_TRUE);
    jsonexports = config_lookup(&root, "hook");

    pretty_assert(h_jsonexport_validate(jsonexports) == true);
    Context ctx = h_jsonexport_init(jsonexports);
    pretty_assert(ctx != NULL);

    // test if message is ingested correctly
    Message msg = message_init();
    message_set_data(msg,
        strdup(
            "{ \"text\": \"hurz\","
            "  \"timestamp\": \"2020-01-01T12:00:00.0000000Z\","
            "  \"token\": \"argh\","
            "  \"recursive\": {}"
            "}")
    );
    message_set_len(msg,strlen(message_get_data(msg)));

    pretty_assert(h_jsonexport(ctx,msg) == true);
    Metadata *md = message_get_metadata(msg);
    MDatum m = metadata_find(md,"jpointer");
    pretty_assert(m->type == MTYPE_STRING);
    pretty_assert(strncmp(m->value.string,"argh",4) == 0);

    char *data = message_get_data(msg);
    // we should insert three rows here
    uint16_t rows = ntohs( *(uint16_t *) data);
    pretty_assert(rows == 3);
    data += 2;

    // first field has 4 bytes
    uint32_t length1 = ntohl( *(uint32_t *) data);
    pretty_assert(length1  == 4);
    data += 4;

    // first string is hurz
    pretty_assert(strncmp(data,"hurz",4) == 0);
    data += 4;

    // timestamps are longs
    uint32_t length2 = ntohl( *(uint32_t *) data);
    pretty_assert(length2 == 8);
    data +=4;

    // compare to precalculated timestamp
    pretty_assert(
        memcmp(data,"\000\002>\021\225\256\020\000",8) == 0
    );
    data += 8;

    // third field has 4 bytes
    uint32_t length3 = ntohl( *(uint32_t *) data);
    pretty_assert(length3 == 4);
    data += 4;

    // third string is hurz
    pretty_assert(strncmp(data,"argh",4) == 0);

    free(message_get_data(msg));
    metadata_free(md);

    // test if message is discarded correctly
    *md = NULL;
    message_set_metadata(msg,*md);
    message_set_data(msg,
        strdup(
            "{ \"text\": \"hurz\","
            "  \"timestamp\": \"2020-01-01T12:00:00.0000000Z\","
            "  \"token\": \"argh\","
            "  \"recursive\": {"
            "    \"discard\": true"
            "    }"
            "}")
    );
    message_set_len(msg,strlen(message_get_data(msg)));

    pretty_assert(h_jsonexport(ctx,msg) == false);
    free(message_get_data(msg));
    metadata_free(md);
    message_free(&msg);
    h_jsonexport_free(ctx);
    config_destroy(&root);
    return 0;
}
