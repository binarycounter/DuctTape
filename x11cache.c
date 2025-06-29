// Build with: gcc -fPIC -shared -o libx11_cache.so x11cache.c -ldl -lX11 -pthread `pkg-config --cflags glib-2.0` `pkg-config --libs glib-2.0`



#define _GNU_SOURCE
#include <X11/Xlib.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <pthread.h>

typedef Status (*XGetWindowAttributesFunc)(Display *, Window, XWindowAttributes *);

static XGetWindowAttributesFunc real_XGetWindowAttributes = NULL;
static GHashTable *cache = NULL;
static pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;

// Simple key struct to uniquely identify (Display*, Window)
typedef struct {
    Display *display;
    Window window;
} CacheKey;

static guint cache_key_hash(gconstpointer key) {
    const CacheKey *k = (const CacheKey *)key;
    return g_direct_hash(k->display) ^ g_direct_hash(GINT_TO_POINTER((gulong)k->window));
}

static gboolean cache_key_equal(gconstpointer a, gconstpointer b) {
    const CacheKey *ka = (const CacheKey *)a;
    const CacheKey *kb = (const CacheKey *)b;
    return ka->display == kb->display && ka->window == kb->window;
}

__attribute__((constructor))
static void init() {
    cache = g_hash_table_new_full(cache_key_hash, cache_key_equal, free, free);
}

__attribute__((destructor))
static void cleanup() {
    g_hash_table_destroy(cache);
}

Status XGetWindowAttributes(Display *display, Window w, XWindowAttributes *attr_return) {
    if (!real_XGetWindowAttributes) {
        real_XGetWindowAttributes = (XGetWindowAttributesFunc)dlsym(RTLD_NEXT, "XGetWindowAttributes");
        if (!real_XGetWindowAttributes) {
            fprintf(stderr, "Failed to load real XGetWindowAttributes\n");
            exit(1);
        }
    }

    CacheKey lookup_key = { display, w };

    pthread_mutex_lock(&cache_mutex);
    XWindowAttributes *cached_attrs = g_hash_table_lookup(cache, &lookup_key);
    if (cached_attrs) {
        memcpy(attr_return, cached_attrs, sizeof(XWindowAttributes));
        pthread_mutex_unlock(&cache_mutex);
        return 1;  // Success
    }
    pthread_mutex_unlock(&cache_mutex);

    // Not cached, call the real function
    Status status = real_XGetWindowAttributes(display, w, attr_return);

    if (status) {
        CacheKey *new_key = malloc(sizeof(CacheKey));
        *new_key = lookup_key;

        XWindowAttributes *new_attrs = malloc(sizeof(XWindowAttributes));
        memcpy(new_attrs, attr_return, sizeof(XWindowAttributes));

        pthread_mutex_lock(&cache_mutex);
        g_hash_table_insert(cache, new_key, new_attrs);
        pthread_mutex_unlock(&cache_mutex);
    }

    return status;
}

