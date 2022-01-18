#include "schaufel.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "test/test.h"
#include "utils/metadata.h"

int main()
{
    Metadata meta = NULL;
    MDatum res = NULL;
    uint32_t *num = calloc(1,sizeof(*num));
    *num = 0xffff;

    MDatum md1 = mdatum_init(MTYPE_STRING,strdup("hurz"),strlen("hurz"));
    res = metadata_insert(&meta,"1",md1);
    pretty_assert(res != NULL);
    pretty_assert(strncmp(res->value,"hurz",4) == 0);
    MDatum md2 = mdatum_init(MTYPE_STRING,strdup("huch"),strlen("huch"));
    metadata_insert(&meta,"2",md2);
    MDatum md3 = mdatum_init(MTYPE_STRING,strdup("moep"),strlen("moep"));
    metadata_insert(&meta,"3",md3);
    MDatum md4 = mdatum_init(MTYPE_STRING,strdup("argh"),strlen("argh"));
    metadata_insert(&meta,"4",md4);
    MDatum md5 = mdatum_init(MTYPE_STRING,strdup("blah"),strlen("blah"));
    metadata_insert(&meta,"5",md5);
    MDatum md6 = mdatum_init(MTYPE_STRING,strdup("blub"),strlen("blub"));
    metadata_insert(&meta,"6",md6);
    MDatum md7 = mdatum_init(MTYPE_INT,num,sizeof(*num));
    metadata_insert(&meta,"7",md7);

    // Test max elements (8)
    MDatum md8 = mdatum_init(MTYPE_STRING,strdup("qoui"),strlen("quoi"));
    res = metadata_insert(&meta,"8",md8);
    pretty_assert(res != NULL);
    res = metadata_insert(&meta,"9",md8);
    pretty_assert(res == NULL);

    // Find integer
    res = metadata_find(&meta,"7");
    pretty_assert(res != NULL);
    pretty_assert(res->type == MTYPE_INT);
    pretty_assert((*(uint32_t *)(res->value)) == 0xffff);

    // Find string
    res = metadata_find(&meta,"4");
    pretty_assert(res != NULL);
    pretty_assert(res->type == MTYPE_STRING);
    pretty_assert(strncmp(res->value,"argh",4) == 0);

    metadata_free(&meta);
    return 0;
}
