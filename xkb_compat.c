// Build with: gcc -shared -fPIC -o libxkb_compat.so xkb_compat.c

#include <stdio.h>
#include  <X11/XKBlib.h>
#include <stdlib.h>

XkbDescPtr XkbGetKeyboard (Display *display, unsigned int which, unsigned int device_spec){
    XkbDescPtr fake = calloc(1, sizeof(XkbDescRec));
    if (!fake) return NULL;

    fake->min_key_code = 2;
    fake->max_key_code = 1;

    return fake;
}
