/*
 * mm-implicit.c - The best malloc package EVAR!
 *
 * TODO (bug): mm_realloc and mm_calloc don't seem to be working...
 * TODO (bug): The allocator doesn't re-use space very well...
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memlib.h"
#include "mm.h"

/** The required alignment of heap payloads */
const size_t ALIGNMENT = 2 * sizeof(size_t);

/** The layout of each block allocated on the heap */
typedef struct {
    /** The size of the block and whether it is allocated (stored in the low bit) */
    size_t header;
    /**
     * We don't know what the size of the payload will be, so we will
     * declare it as a zero-length array.  This allow us to obtain a
     * pointer to the start of the payload.
     */
    uint8_t payload[];
} block_t;

typedef struct {
    size_t size;
} footer_t;

typedef struct free_node_t {
    size_t header;
    struct free_node_t *left;
    struct free_node_t *right;
} free_node_t;

/** The first and last blocks on the heap */
static block_t *mm_heap_first = NULL;
static block_t *mm_heap_last = NULL;

static free_node_t *free_list_first = NULL;

void add_free_node(free_node_t *free_node) {
    if (free_list_first == NULL) {
        free_list_first = free_node;
        free_list_first->left = NULL;
        free_list_first->right = NULL;
    }
    else {
        free_node->right = free_list_first;
        free_list_first->left = free_node;
        free_list_first = free_node;
        free_node->left = NULL;
    }
}

void remove_free_node(free_node_t *free_node) {
    if (free_node == free_list_first) {
        free_list_first = free_list_first->right;
        if (free_list_first != NULL) {
            free_list_first->left = NULL;
        }
    }
    else {
        if (free_node->left != NULL) {
            free_node->left->right = free_node->right;
        }
        if (free_node->right != NULL) {
            free_node->right->left = free_node->left;
        }
    }
}

/** Rounds up `size` to the nearest multiple of `n` */
static size_t round_up(size_t size, size_t n) {
    return (size + (n - 1)) / n * n;
}

/** Set's a block's header with the given size and allocation state */
static void set_header(block_t *block, size_t size, bool is_allocated) {
    block->header = size | is_allocated;
    footer_t *footer = (void *) block + size - sizeof(footer_t);
    footer->size = size | is_allocated;
}

/** Extracts a block's size from its header */
static size_t get_size(block_t *block) {
    return block->header & ~1;
}

/** Extracts a block's allocation state from its header */
static bool is_allocated(block_t *block) {
    return block->header & 1;
}

/**
 * Finds the first free block in the heap with at least the given size.
 * If no block is large enough, returns NULL.
 */
static block_t *find_fit(size_t size) {
    // Traverse the blocks in the heap using the explicit list
    free_node_t *curr = free_list_first;
    while (curr != NULL) {
        block_t *block = (block_t *) curr;
        if (get_size(block) >= size) {
            // check can split
            size_t min_block_size =
                round_up(sizeof(block_t) + sizeof(footer_t) + ALIGNMENT, ALIGNMENT);
            size_t original_size = get_size(block);
            if (original_size - size >= min_block_size) {
                set_header(block, size, false);
                block_t *nxt = (void *) block + size;
                set_header(nxt, original_size - size, false);
                if (nxt > mm_heap_last) {
                    mm_heap_last = nxt;
                }

                remove_free_node(curr);
                free_node_t *new_node = (free_node_t *) nxt;
                add_free_node(new_node);
                return block;
            }

            // cant split
            if (curr == free_list_first) {
                free_list_first = curr->right;
                return block;
            }

            curr->left->right = curr->right;
            if (curr->right != NULL) {
                curr->right->left = curr->left;
            }
            return block;
        }
        curr = curr->right;
    }
    return NULL;
}

/** Gets the header corresponding to a given payload pointer */
static block_t *block_from_payload(void *ptr) {
    return ptr - offsetof(block_t, payload);
}

static void coalesce(block_t *block) {
    bool flag = false;
    if (block != mm_heap_first) {
        footer_t *left_footer = (void *) block - sizeof(footer_t);
        block_t *left_block = (void *) block - (left_footer->size & ~1);

        if (!is_allocated(left_block)) {
            set_header(left_block, get_size(left_block) + get_size(block), false);
            if (block == mm_heap_last) {
                mm_heap_last = left_block;
            }
            block = left_block;
            flag = true;
        }
    }

    if (block < mm_heap_last) {
        block_t *right_block = (void *) block + get_size(block);

        if (!is_allocated(right_block)) {
            remove_free_node((free_node_t *) right_block);
            set_header(block, get_size(block) + get_size(right_block), false);
            if (right_block == mm_heap_last) {
                mm_heap_last = block;
            }
        }
    }

    if (!flag) {
        add_free_node((free_node_t *) block);
    }
}

/**
 * mm_init - Initializes the allocator state
 */
bool mm_init(void) {
    // We want the first payload to start at ALIGNMENT bytes from the start of the heap
    void *padding = mem_sbrk(ALIGNMENT - sizeof(block_t));
    if (padding == (void *) -1) {
        return false;
    }

    // Initialize the heap with no blocks
    mm_heap_first = NULL;
    mm_heap_last = NULL;
    free_list_first = NULL;
    return true;
}

/**
 * mm_malloc - Allocates a block with the given size
 */
void *mm_malloc(size_t size) {
    // The block must have enough space for a header and be 16-byte aligned
    size = round_up(sizeof(block_t) + sizeof(footer_t) + size, ALIGNMENT);

    // If there is a large enough free block, use it
    block_t *block = find_fit(size);
    if (block != NULL) {
        set_header(block, get_size(block), true);
        return block->payload;
    }

    // Otherwise, a new block needs to be allocated at the end of the heap
    block = mem_sbrk(size);
    if (block == (void *) -1) {
        return NULL;
    }

    // Update mm_heap_first and mm_heap_last since we extended the heap
    if (mm_heap_first == NULL) {
        mm_heap_first = block;
    }
    mm_heap_last = block;

    // Initialize the block with the allocated size
    set_header(block, size, true);
    return block->payload;
}

/**
 * mm_free - Releases a block to be reused for future allocations
 */
void mm_free(void *ptr) {
    // mm_free(NULL) does nothing
    if (ptr == NULL) {
        return;
    }

    // Mark the block as unallocated
    block_t *block = block_from_payload(ptr);
    set_header(block, get_size(block), false);

    coalesce(block);
}

/**
 * mm_realloc - Change the size of the block by mm_mallocing a new block,
 *      copying its data, and mm_freeing the old block.
 */
void *mm_realloc(void *old_ptr, size_t size) {
    // if ptr is NULL, run malloc
    if (old_ptr == NULL) {
        return mm_malloc(size);
    }

    // if size is 0, free and return NULL
    if (size == 0) {
        mm_free(old_ptr);
        return NULL;
    }

    block_t *block = block_from_payload(old_ptr);
    // if new size is less than old size, no need to do anything
    if (get_size(block) >=
        round_up(sizeof(block_t) + sizeof(footer_t) + size, ALIGNMENT)) {
        return old_ptr;
    }

    void *new_payload = mm_malloc(size);
    if (new_payload == NULL) {
        return NULL;
    }

    block_t *new_block = block_from_payload(new_payload);
    memcpy(new_payload, old_ptr, get_size(block) - sizeof(block_t) - sizeof(footer_t));
    mm_free(old_ptr);
    return new_block->payload;
}

/**
 * mm_calloc - Allocate the block and set it to zero.
 */
void *mm_calloc(size_t nmemb, size_t size) {
    size_t total_size = nmemb * size;
    if (total_size == 0) return NULL;
    void *payload = mm_malloc(total_size);
    memset(payload, 0, total_size);
    return payload;
}

/**
 * mm_checkheap - So simple, it doesn't need a checker!
 */
void mm_checkheap(void) {
    // make sure headers and footers are identical
    size_t index = 0;
    size_t free_blocks = 0;
    for (block_t *curr = mm_heap_first; mm_heap_last != NULL && curr <= mm_heap_last;
         curr = (void *) curr + get_size(curr)) {
        footer_t *footer = (void *) curr + get_size(curr) - sizeof(footer_t);
        if (curr->header != footer->size) {
            printf("Header and footer not identical at block %d\n", (int) index);
            printf("%zd, %zd, %zd\n", curr->header, footer->size, get_size(curr));
            // printf("Addresses: %d\n", (int) curr);
            exit(0);
        }
        index++;

        if (!is_allocated(curr)) {
            free_blocks++;
        }
    }

    size_t free_list_length = 0;
    for (free_node_t *free_node = free_list_first; free_node != NULL;
         free_node = free_node->right) {
        free_list_length++;
    }

    if (free_list_length != free_blocks) {
        printf("Free list length not valid: %d vs %d\n", (int) free_list_length,
               (int) free_blocks);
        // for (block_t *curr = mm_heap_first; mm_heap_last != NULL && curr <=
        // mm_heap_last;
        //      curr = (void *) curr + get_size(curr)) {
        //     if (!is_allocated(curr)) {
        //         printf("Address: %d\n", (int) curr);
        //     }
        // }
        // printf("\n");
        // for (free_node_t *free_node = free_list_first; free_node != NULL;
        //      free_node = free_node->right) {
        //     printf("Address: %d\n", (int) free_node);
        // }
        exit(0);
    }
}