typedef struct HTable HTable;
typedef uint32_t (*HashFunc) (const char *);

typedef enum HTAction
{
    HT_FIND,
    HT_INSERT,
    HT_REMOVE
} HTAction;


HTable *ht_create(uint16_t nslots, size_t data_size, HashFunc hfunc);
void *ht_search(HTable *ht, const char *key, HTAction action);
