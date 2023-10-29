#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashtable.h"

#define CAPACITY 50000

struct ht_item {
    char *key, *value;
    size_t key_size, value_size;
};

struct htable {
    struct ht_item **items;
    int size, count;
};

static void hash_func(unsigned long *hash, char *str)
{
    unsigned long i = 0;

    for (size_t j = 0; str[j]; ++j)
        i += str[j];
    *hash = i % CAPACITY;
}

void create_ht_item(struct ht_item **item, char *key, char *value)
{
    *item = malloc(sizeof(struct ht_item));
    (*item)->key = malloc(sizeof(char) * (strlen(key) + 1));
    (*item)->value = malloc(sizeof(char) * (strlen(value) + 1));
    strncpy((*item)->key, key, strlen(key));
    strncpy((*item)->value, value, strlen(value));

    (*item)->key_size = sizeof(char) * (strlen(key) + 1);
    (*item)->value_size = sizeof(char) * (strlen(value) + 1);

    fprintf(stdout, "[ht_item %p]: [Key (%ld bytes, at %p): '%s'] [Value (%ld bytes, at %p): '%s']\n", *item, (*item)->key_size, (*item)->key, (*item)->key, (*item)->value_size, (*item)->value, (*item)->value);
}

void create_htable(struct htable **table) 
{
    *table = malloc(sizeof(struct htable));
    (*table)->size = CAPACITY;
    (*table)->count = 0;
    (*table)->items = calloc((*table)->size, sizeof(struct ht_item));

    for (size_t i = 0; i < (*table)->size; ++i)
        (*table)->items[i] = NULL;
}

void free_ht_item(struct ht_item *item)
{
    free(item->key);
    free(item->value);
    free(item);
}

void free_htable(struct htable *table)
{
    struct ht_item *item;
    for (size_t i = 0; i < table->size; ++i) {
        item = table->items[i];
        if (item != NULL)
            free_ht_item(item);
    }
    free(table->items);
    free(table);
}

static void handle_ht_collision(struct htable *table, struct ht_item *item)
{
    fprintf(stdout, "Collision!\n");
}

void htable_insert(struct htable *table, char *key, char *value)
{
    fprintf(stdout, "(htable_insert) Inserting value '%s' in key '%s'\n", value, key); 
    struct ht_item *item, *current;
    unsigned long idx;
    create_ht_item(&item, key, value);
    hash_func(&idx, key);
    current = table->items[idx];
    if (current == NULL) {
        if (table->count == table->size) {
            fprintf(stdout, "Table is full!\n");
            free_ht_item(item);
            return;
        }
        table->items[idx] = item;
        table->count++;

    } else if (strncmp(current->key, key, strlen(key)) == 0) {
        strncpy(table->items[idx]->value, value, strlen(value));
    } else {
        handle_ht_collision(table, item);
    }
}

int htable_remove(struct htable *table, char *key)
{
    unsigned long idx;
    struct ht_item *current;
    hash_func(&idx, key);
    current = table->items[idx];

    fprintf(stdout, "Deleting key '%s' at %p\n", key, current);

    if ((current != NULL) && (strncmp(current->key, key, strlen(key)) == 0)) {
        table->items[idx] = NULL;
        table->count--;
        free_ht_item(current);
        return 0;
    } else 
        return 1;
}

char *htable_search(struct htable *table, char *key) 
{
    unsigned long idx;
    struct ht_item *item;

    hash_func(&idx, key);
    item = table->items[idx];

    if (item == NULL)
        return NULL;

    fprintf(stdout, "(htable_search) Has key with %ld bytes\n", item->key_size);
    fprintf(stdout, "(htable_search) Has value with %ld bytes\n", item->value_size);

    if ((item->key_size > 0) && (item->value_size > 0)) {
        fprintf(stdout, "(htable_search) Searching key at %p\n", item->key);

        if (strncmp(item->key, key, strlen(key)) == 0) {
            fprintf(stdout, "(htable_search) Found value '%s' in key '%s'\n", item->value, item->key);

            return item->value;
        } 
    }

    return NULL;
}

