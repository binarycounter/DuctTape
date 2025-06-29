// Traps SDL2's attempts to register signal handlers.

//  Build with:
//  gcc -shared -fPIC -o nosignals.so nosignals.c

#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>



typedef struct _range
{
    void* start;
    void* end;
} range;

static int no_signal_range_count=0;
static range* no_signal_ranges[16]={0};


void no_signal_register_backend(const char* name)
{
    FILE* maps = fopen("/proc/self/maps", "r");
    if (!maps) {
        perror("fopen");
        return;
    }
    char line[256];
    static char library_name[256];  
    while (fgets(line, sizeof(line), maps)) {
        void* start;
        void* end;
        char perms[5];
        unsigned long offset;
        int dev_major, dev_minor;
        unsigned long inode;
        char pathname[256];

        if (sscanf(line, "%p-%p %4s %lx %x:%x %lu %s",
                   &start, &end, perms, &offset,
                   &dev_major, &dev_minor, &inode, pathname) == 8) {
            if (strstr(pathname,name)!=NULL) {
                range* newRange=malloc(sizeof(range));
                newRange->start=start;
                newRange->end=end;
                no_signal_ranges[no_signal_range_count]=newRange;
                printf("Trapping sigaction requests from %s at %p - %p\n",pathname,start,end);
                no_signal_range_count+=1;
            }
        }
    }

    fclose(maps);
}


bool no_signal_call_from_backend(void* address)
{
    for(int i=0; i<no_signal_range_count; i++)
    {
        if(address >= no_signal_ranges[i]->start && address < no_signal_ranges[i]->end)
            return true;
    }
    return false;
}

static int(*real_sigaction)(int signum,
                     const struct sigaction * restrict act,
                     struct sigaction * restrict oldact) = NULL;

int sigaction(int signum,
                     const struct sigaction * restrict act,
                     struct sigaction * restrict oldact)
{
    void* call_address= __builtin_return_address(0); 
    if(no_signal_range_count==0)
        no_signal_register_backend("libSDL2");
    if(no_signal_call_from_backend(call_address)) 
    {
        printf("Trapped sigaction call from backend.\n");
        return 0;
    }           
    if (real_sigaction == NULL)
        real_sigaction=dlsym(RTLD_NEXT, "sigaction");

    return real_sigaction(signum, act, oldact);
}
