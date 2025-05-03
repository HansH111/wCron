// dynarray.h
#ifndef _DYNARRAY_H_
#define _DYNARRAY_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Generic dynamic array
typedef struct {
    void *items;        // Pointer to elements
    size_t item_size;   // Size of each element
    int size;           // Number of used elements
    int capacity;       // Number of allocated elements
} dynamic_array;

// Generic array function prototypes
void  array_init (dynamic_array *, size_t);
void *array_add  (dynamic_array *);
void  array_clear(dynamic_array *, void (*free_item)(void *));
void *array_get  (dynamic_array *, int);

#endif

