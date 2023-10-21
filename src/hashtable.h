#ifndef HASHTABLE_H_
#define HASHTABLE_H_

struct ht_item;
struct htable;

void create_ht_item(struct ht_item **item, char *key, char *value);
void create_htable(struct htable **table);
void free_ht_item(struct ht_item *item);
void free_htable(struct htable *table);
void htable_insert(struct htable *table, char *key, char *value);
int htable_remove(struct htable *table, char *key);
char *htable_search(struct htable *table, char *key);

#endif

