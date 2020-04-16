typedef struct HTable HTable;
typedef struct HTIter HTIter;

typedef uint32_t (*HashFunc) (const char *);

typedef enum HTAction
{
    HT_FIND,
    HT_INSERT,
    HT_UPSERT, // insert if doesn't exist or return an existing entry for update
    HT_REMOVE
} HTAction;


HTable *ht_create(uint16_t nslots, size_t data_size, HashFunc hfunc);
void *ht_search(HTable *ht, const char *key, HTAction action);
HTIter *ht_iter_create(HTable *ht);
const char *ht_iter_next(HTIter *iter, void **data);
