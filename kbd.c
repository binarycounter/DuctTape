

#include <fcntl.h>
#include <linux/uinput.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <termios.h>
#include <ctype.h>
#include <stdlib.h>

// ASCII -> keycode + shift state
struct keymap_entry {
    char ascii;
    int keycode;
    int shift;
};

struct keymap_entry keymap[] = {
    // Lowercase letters
    {'a', KEY_A, 0}, {'b', KEY_B, 0}, {'c', KEY_C, 0}, {'d', KEY_D, 0},
    {'e', KEY_E, 0}, {'f', KEY_F, 0}, {'g', KEY_G, 0}, {'h', KEY_H, 0},
    {'i', KEY_I, 0}, {'j', KEY_J, 0}, {'k', KEY_K, 0}, {'l', KEY_L, 0},
    {'m', KEY_M, 0}, {'n', KEY_N, 0}, {'o', KEY_O, 0}, {'p', KEY_P, 0},
    {'q', KEY_Q, 0}, {'r', KEY_R, 0}, {'s', KEY_S, 0}, {'t', KEY_T, 0},
    {'u', KEY_U, 0}, {'v', KEY_V, 0}, {'w', KEY_W, 0}, {'x', KEY_X, 0},
    {'y', KEY_Y, 0}, {'z', KEY_Z, 0},

    // Uppercase (with shift)
    {'A', KEY_A, 1}, {'B', KEY_B, 1}, {'C', KEY_C, 1}, {'D', KEY_D, 1},
    {'E', KEY_E, 1}, {'F', KEY_F, 1}, {'G', KEY_G, 1}, {'H', KEY_H, 1},
    {'I', KEY_I, 1}, {'J', KEY_J, 1}, {'K', KEY_K, 1}, {'L', KEY_L, 1},
    {'M', KEY_M, 1}, {'N', KEY_N, 1}, {'O', KEY_O, 1}, {'P', KEY_P, 1},
    {'Q', KEY_Q, 1}, {'R', KEY_R, 1}, {'S', KEY_S, 1}, {'T', KEY_T, 1},
    {'U', KEY_U, 1}, {'V', KEY_V, 1}, {'W', KEY_W, 1}, {'X', KEY_X, 1},
    {'Y', KEY_Y, 1}, {'Z', KEY_Z, 1},

    // Digits
    {'0', KEY_0, 0}, {'1', KEY_1, 0}, {'2', KEY_2, 0}, {'3', KEY_3, 0},
    {'4', KEY_4, 0}, {'5', KEY_5, 0}, {'6', KEY_6, 0}, {'7', KEY_7, 0},
    {'8', KEY_8, 0}, {'9', KEY_9, 0},

    // Symbols with shift
    {'!', KEY_1, 1}, {'@', KEY_2, 1}, {'#', KEY_3, 1}, {'$', KEY_4, 1},
    {'%', KEY_5, 1}, {'^', KEY_6, 1}, {'&', KEY_7, 1}, {'*', KEY_8, 1},
    {'(', KEY_9, 1}, {')', KEY_0, 1},

    // Other symbols
    {'-', KEY_MINUS, 0}, {'_', KEY_MINUS, 1},
    {'=', KEY_EQUAL, 0}, {'+', KEY_EQUAL, 1},
    {'[', KEY_LEFTBRACE, 0}, {'{', KEY_LEFTBRACE, 1},
    {']', KEY_RIGHTBRACE, 0}, {'}', KEY_RIGHTBRACE, 1},
    {'\\', KEY_BACKSLASH, 0}, {'|', KEY_BACKSLASH, 1},
    {';', KEY_SEMICOLON, 0}, {':', KEY_SEMICOLON, 1},
    {'\'', KEY_APOSTROPHE, 0}, {'"', KEY_APOSTROPHE, 1},
    {',', KEY_COMMA, 0}, {'<', KEY_COMMA, 1},
    {'.', KEY_DOT, 0}, {'>', KEY_DOT, 1},
    {'/', KEY_SLASH, 0}, {'?', KEY_SLASH, 1},
    {'`', KEY_GRAVE, 0}, {'~', KEY_GRAVE, 1},

    // Whitespace and control
    {' ', KEY_SPACE, 0},
    {'\n', KEY_ENTER, 0},
    {'\t', KEY_TAB, 0},
    {'\x7F', KEY_BACKSPACE, 0},
    {'\x1B', KEY_ESC, 0},
};

int emit(int fd, int type, int code, int val) {
    struct input_event ie = {0};
    ie.type = type;
    ie.code = code;
    ie.value = val;
    return write(fd, &ie, sizeof(ie));
}

void emit_key(int fd, int keycode, int shift) {
    if (shift) emit(fd, EV_KEY, KEY_LEFTSHIFT, 1);
    emit(fd, EV_KEY, keycode, 1);
    emit(fd, EV_SYN, SYN_REPORT, 0);
    emit(fd, EV_KEY, keycode, 0);
    emit(fd, EV_SYN, SYN_REPORT, 0);
    if (shift) emit(fd, EV_KEY, KEY_LEFTSHIFT, 0);
    emit(fd, EV_SYN, SYN_REPORT, 0);
}

int main(void) {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("open /dev/uinput");
        return 1;
    }

    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_EVBIT, EV_SYN);
    for (int i = 0; i < KEY_MAX; i++) {
        ioctl(fd, UI_SET_KEYBIT, i);
    }

    struct uinput_setup usetup = {0};
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor  = 0x1;
    usetup.id.product = 0x1;
    strcpy(usetup.name, "ssh-fullkbd");

    ioctl(fd, UI_DEV_SETUP, &usetup);
    ioctl(fd, UI_DEV_CREATE);
    sleep(1);

    // Set terminal to raw mode
    struct termios orig, raw;
    tcgetattr(STDIN_FILENO, &orig);
    raw = orig;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    printf("Type to inject. Ctrl+C to exit.\n");

    int ch;
    while ((ch = getchar()) != EOF) {
        int found = 0;
        for (unsigned i = 0; i < sizeof(keymap)/sizeof(keymap[0]); i++) {
            if (keymap[i].ascii == ch) {
                emit_key(fd, keymap[i].keycode, keymap[i].shift);
                found = 1;
                break;
            }
        }
        if (!found) {
            printf("\n[unsupported: 0x%x '%c']\n", ch, isprint(ch) ? ch : '?');
        }
    }

    // Restore terminal
    tcsetattr(STDIN_FILENO, TCSANOW, &orig);
    printf("\nExiting.\n");

    ioctl(fd, UI_DEV_DESTROY);
    close(fd);
    return 0;
}
