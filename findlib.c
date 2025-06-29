// Outputs absolute paths to libraries. Build with:
// gcc findlib.c -o findlib -ldl

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <link.h>

int find_library_path(const char *libname) {
    void *handle = dlopen(libname, RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "Error: %s\n", dlerror());
        return 1;
    }

    struct link_map *map = NULL;
    if (dlinfo(handle, RTLD_DI_LINKMAP, &map) == 0) {
        if (map && map->l_name) {
            printf("%s\n", map->l_name);
            return 0;
        } else {
            return 1;
        }
    } else {
        perror("dlinfo");
        return 1;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <library_name>\n", argv[0]);
        return 1;
    }

    return find_library_path(argv[1]);
}
