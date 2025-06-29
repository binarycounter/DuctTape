# DuctTape
A collection of random pieces of code that fix weird specific issues i had while working on ports.

| File              | Description                                                                                                                                                                                                                                                           |
|-------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| kbd.c             | CLI Utility that creates a virtual keyboard via uinput, and forwards keys typed into the terminal to that keyboard. Useful if you need to control a program via a remote console (e.g through SSH)                                                                    |
| findlib.c         | CLI Utility that returns the absolute path to any library findable by the systems dynamic linker. Returns exit code 0/1 depending on success. Can e.g. be used in bash scripts to set up replacement libraries in LD_LIBRARY_PATH, if they aren't installed natively. |
| stub_drm_driver.c | A CLI deamon that (poorly) emulates a DRM/KMS device for programs that depend on them having "something" responding to commands. Uses CUSE/FUSE to create a character device.                                                                                         |
| x11cache.c        | LD_PRELOAD library that caches XGetWindowAttributes, only calling the actual X11 function every 10 seconds. This function is very CPU heavy and Binding Of Isaac Rebirth calls it every frame for some reason.                                                        |
| xkb_compat.c      | LD_PRELOAD library. On XWayland systems, XkbGetKeyboard can return NULL, which Binding Of Isaac Rebirth doesn't check. This fakes that function and returns just enough data for Rebirth's keymap function to fail gracefully.                                        |
| nosignals.c       | LD_PRELOAD library that stops SDL2 from registering SIGSEGV signal handlers. SEGV signals are used by Java and Mono, and SDL2's handlers interfere with that.                                                                                                         |

