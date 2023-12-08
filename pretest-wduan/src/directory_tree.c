#include "directory_tree.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

const size_t MODE = 0777;

void init_node(node_t *node, char *name, node_type_t type) {
    if (name == NULL) {
        name = strdup("ROOT");
        assert(name != NULL);
    }
    node->name = name;
    node->type = type;
}

file_node_t *init_file_node(char *name, size_t size, uint8_t *contents) {
    file_node_t *node = malloc(sizeof(file_node_t));
    assert(node != NULL);
    init_node((node_t *) node, name, FILE_TYPE);
    node->size = size;
    node->contents = contents;
    return node;
}

directory_node_t *init_directory_node(char *name) {
    directory_node_t *node = malloc(sizeof(directory_node_t));
    assert(node != NULL);
    init_node((node_t *) node, name, DIRECTORY_TYPE);
    node->num_children = 0;
    node->children = NULL;
    return node;
}

void add_child_directory_tree(directory_node_t *dnode, node_t *child) {
    dnode->num_children++;
    dnode->children = realloc(dnode->children, sizeof(node_t *) * dnode->num_children);
    dnode->children[dnode->num_children - 1] = child;

    size_t index = dnode->num_children - 1;
    while (1) {
        if (index == 0) {
            break;
        }

        node_t *cur = dnode->children[index];
        node_t *previous = dnode->children[index - 1];
        if (strcmp(cur->name, previous->name) < 0) {
            dnode->children[index - 1] = cur;
            dnode->children[index] = previous;
        }

        index--;
    }
}

void print_subtree(node_t *node, size_t depth) {
    printf("%*s", (int) (4 * depth), "");
    printf("%s\n", node->name);
    if (node->type == DIRECTORY_TYPE) {
        directory_node_t *dnode = (directory_node_t *) node;
        for (size_t i = 0; i < dnode->num_children; i++) {
            print_subtree(dnode->children[i], depth + 1);
        }
    }
}

void print_directory_tree(node_t *node) {
    print_subtree(node, 0);
}

void create_tree(node_t *node, char *path) {
    size_t add_slash = 0;
    if (strlen(path) != 0) {
        add_slash += 1;
    }
    char *cur_path =
        malloc(sizeof(char) * (strlen(path) + strlen(node->name) + 1 + add_slash));
    if (strlen(path) != 0) {
        strcpy(cur_path, path);
        strcat(cur_path, "/");
        strcat(cur_path, node->name);
    }
    else {
        strcpy(cur_path, node->name);
    }

    if (node->type == DIRECTORY_TYPE) {
        mkdir(cur_path, MODE);
        directory_node_t *dnode = (directory_node_t *) node;
        for (size_t i = 0; i < dnode->num_children; i++) {
            create_tree(dnode->children[i], cur_path);
        }
    }
    else {
        FILE *file = fopen(cur_path, "w");
        assert(file != NULL);

        file_node_t *fnode = (file_node_t *) node;
        size_t written = fwrite(fnode->contents, sizeof(uint8_t), fnode->size, file);
        assert(written == fnode->size);

        int close_result = fclose(file);
        assert(close_result == 0);
    }
    free(cur_path);
}

void create_directory_tree(node_t *node) {
    char *empty = "";
    create_tree(node, empty);
}

void free_directory_tree(node_t *node) {
    if (node->type == FILE_TYPE) {
        file_node_t *fnode = (file_node_t *) node;
        free(fnode->contents);
    }
    else {
        assert(node->type == DIRECTORY_TYPE);
        directory_node_t *dnode = (directory_node_t *) node;
        for (size_t i = 0; i < dnode->num_children; i++) {
            free_directory_tree(dnode->children[i]);
        }
        free(dnode->children);
    }
    free(node->name);
    free(node);
}
