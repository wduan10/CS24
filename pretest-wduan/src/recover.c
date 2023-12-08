#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "directory_tree.h"
#include "fat16.h"

const size_t MASTER_BOOT_RECORD_SIZE = 0x20B;

void follow(FILE *disk, directory_node_t *node, bios_parameter_block_t bpb) {
    while (1) {
        directory_entry_t entry;
        size_t written = fread(&entry, sizeof(directory_entry_t), 1, disk);
        assert(written == 1);

        if (is_hidden(entry)) {
            continue;
        }

        // exit loop if filename starts with \0
        char *filename = get_file_name(entry);
        if (filename[0] == '\0') {
            free(filename);
            break;
        }

        size_t location = ftell(disk); // return to this later
        size_t offset = get_offset_from_cluster(entry.first_cluster, bpb);
        fseek(disk, offset, SEEK_SET);
        if (is_directory(entry)) {
            directory_node_t *dnode = init_directory_node(filename);
            add_child_directory_tree(node, (node_t *) dnode);
            follow(disk, dnode, bpb);
        }
        else {
            uint8_t *contents = malloc(sizeof(uint8_t) * entry.file_size);
            written = fread(contents, sizeof(uint8_t) * entry.file_size, 1, disk);
            assert(written == 1);

            file_node_t *file_node = init_file_node(filename, entry.file_size, contents);
            add_child_directory_tree(node, (node_t *) file_node);
        }

        fseek(disk, location, SEEK_SET);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "USAGE: %s <image filename>\n", argv[0]);
        return 1;
    }

    FILE *disk = fopen(argv[1], "r");
    if (disk == NULL) {
        fprintf(stderr, "No such image file: %s\n", argv[1]);
        return 1;
    }

    bios_parameter_block_t bpb;

    // my code
    fseek(disk, MASTER_BOOT_RECORD_SIZE, SEEK_SET);
    size_t written = fread(&bpb, sizeof(bios_parameter_block_t), 1, disk);
    assert(written == 1);
    size_t location = get_root_directory_location(bpb);
    fseek(disk, location, SEEK_SET);

    directory_node_t *root = init_directory_node(NULL);
    follow(disk, root, bpb);
    print_directory_tree((node_t *) root);
    create_directory_tree((node_t *) root);
    free_directory_tree((node_t *) root);

    int result = fclose(disk);
    assert(result == 0);
}
