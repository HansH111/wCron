#include "dynarray.h"

// Generic Dynamic Array Functions
void array_init(dynamic_array *arr, size_t item_size)
{
    arr->items = NULL;
    arr->item_size = item_size;
    arr->size = 0;
    arr->capacity = 0;
}

void *array_add(dynamic_array *arr)
{
	void *new_item;
    if (arr->size >= arr->capacity) {
        int new_capacity = arr->capacity ? arr->capacity * 2 : 8;
        void *new_items = realloc(arr->items, new_capacity * arr->item_size);
        if (!new_items) return NULL;
        arr->items = new_items;
        arr->capacity = new_capacity;
    }
    
    new_item = (char *)arr->items + arr->size * arr->item_size;
    arr->size++;
    return new_item;
}

void array_clear(dynamic_array *arr, void (*free_item)(void *))
{
	int i;
	if (free_item) {
        for (i = 0; i < arr->size; i++) {
            void *item = (char *)arr->items + i * arr->item_size;
            free_item(item);
        }
    }
    if (arr->items) free(arr->items);
    arr->items = NULL;
    arr->size = 0;
    arr->capacity = 0;
}

void *array_get(dynamic_array *arr, int index)
{
    if (index < 0 || index >= arr->size) return NULL;
    return (char *)arr->items + index * arr->item_size;
}

