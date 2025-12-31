#ifndef BGAME_DRAW_LIST_H
#define BGAME_DRAW_LIST_H

#include "ecs.h"
#include "allocator/frame.h"
#include <stdlib.h>

typedef struct {
    int sort_key;
    ecs_id_t entity_id;
} bgame_draw_item_t;

typedef struct {
    int len;
    bgame_draw_item_t items[];
} bgame_draw_list_t;

static inline bgame_draw_list_t*
bgame_alloc_draw_list(int len) {
    bgame_draw_list_t* draw_list = bgame_alloc_for_frame(
        sizeof(bgame_draw_list_t) + sizeof(bgame_draw_item_t) * len,
        _Alignof(bgame_draw_list_t)
    );
    draw_list->len = len;
    return draw_list;
}

static inline int
bgame_cmp_draw_items(const void* lhs, const void* rhs) {
    return ((const bgame_draw_item_t*)lhs)->sort_key - ((const bgame_draw_item_t*)rhs)->sort_key;
}

static inline void
bgame_sort_draw_list(bgame_draw_list_t* list) {
    qsort(
        list->items,
        (size_t)list->len,
        sizeof(list->items[0]),
        bgame_cmp_draw_items
    );
}

static inline bgame_draw_item_t*
bgame_next_draw_item(
    bgame_draw_list_t* list,
    int* index_ptr,
    int max_key
) {
    int index = *index_ptr;
    if (index >= list->len) { return NULL; }

    bgame_draw_item_t* item = &list->items[index];
    if (item->sort_key <= max_key) {
        *index_ptr = index + 1;
        return item;
    } else {
        return NULL;
    }
}

#endif
