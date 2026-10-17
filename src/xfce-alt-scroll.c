/*
 * xfce-alt-scroll - suppress two-finger smooth scrolling while Alt is held,
 * sending step-wise XTest Button4/5 events for xfwm4 desktop zoom instead.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * This tool is designed for Xorg/Xfce touchpads, not for wheel-only mice.
 */
#define _POSIX_C_SOURCE 200809L
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* libXtst public ABI. Compile also on minimal systems without libxtst-dev. */
extern Bool XTestQueryExtension(Display *display, int *event_base, int *error_base, int *major_version, int *minor_version);
extern int XTestFakeButtonEvent(Display *display, unsigned int button,
                                Bool is_press, unsigned long delay);
#define SLOT_COUNT 64
#define ARRAYLEN(x) (sizeof(x) / sizeof((x)[0]))
#define BIT_TEST(a, x) (((a)[(x) / 8] >> ((x) % 8)) & 1)

static volatile sig_atomic_t stop_requested = 0;
static double step_mm = 2.0;
static double interval_ms = 5.0;
static bool verbose = false;
static bool invert = false;

typedef struct {
    bool active;
    bool has_y;
    int y;
} Touch;

typedef struct {
    int fd;
    int xid;
    char path[128];
    char name[256];
    int original[3];
    bool disabled;
    int slot;
    Touch touch[SLOT_COUNT];
    bool has_previous;
    double previous_y;
    double accumulator;
    double threshold;
    double last_zoom_ms;
} Touchpad;

static Touchpad tp = {.fd = -1, .xid = -1};
static Display *display = NULL;
static KeyCode lalt, ralt;
static bool alt_mode = false;

static double monotonic_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

static void handle_signal(int sig)
{
    (void)sig;
    stop_requested = 1;
}

/* Capture stdout, execute argv without using a shell. */
static int execute_capture(char *const argv[], char *out, size_t size)
{
    int pipefd[2];
    if (pipe(pipefd) < 0)
        return -1;
    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]); close(pipefd[1]);
        return -1;
    }
    if (pid == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]); close(pipefd[1]);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        execvp(argv[0], argv);
        _exit(127);
    }
    close(pipefd[1]);
    size_t have = 0;
    while (have < size - 1) {
        ssize_t n = read(pipefd[0], out + have, size - 1 - have);
        if (n > 0) have += (size_t)n;
        else if (n == 0) break;
        else if (errno != EINTR) break;
    }
    out[have] = '\0';
    close(pipefd[0]);
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

static int set_scroll(int method[3])
{
    if (tp.xid < 0) return -1;
    char id[20], x[8], y[8], z[8];
    snprintf(id, sizeof(id), "%d", tp.xid);
    snprintf(x, sizeof(x), "%d", method[0]);
    snprintf(y, sizeof(y), "%d", method[1]);
    snprintf(z, sizeof(z), "%d", method[2]);
    char *argv[] = {"xinput", "set-prop", id,
                    "libinput Scroll Method Enabled", x, y, z, NULL};
    char output[64];
    return execute_capture(argv, output, sizeof(output));
}

static bool read_xinput_properties(const char *name, int *id, int method[3])
{
    char output[16384];
    char *list[] = {"xinput", "list", "--id-only", (char *)name, NULL};
    if (execute_capture(list, output, sizeof(output)) != 0) return false;
    char *end = NULL;
    long n = strtol(output, &end, 10);
    if (end == output || n <= 0 || n > 65535) return false;
    char idstr[20];
    snprintf(idstr, sizeof(idstr), "%ld", n);
    char *props[] = {"xinput", "list-props", idstr, NULL};
    if (execute_capture(props, output, sizeof(output)) != 0) return false;
    char *line = strstr(output, "libinput Scroll Method Enabled (");
    if (!line) return false;
    char *colon = strchr(line, ':');
    if (!colon || sscanf(colon + 1, " %d , %d , %d", &method[0], &method[1], &method[2]) != 3)
        return false;
    *id = (int)n;
    return true;
}

static bool alt_down(void)
{
    char keys[32];
    XQueryKeymap(display, keys);
    return (lalt && (keys[lalt / 8] & (1U << (lalt % 8)))) ||
           (ralt && (keys[ralt / 8] & (1U << (ralt % 8))));
}

static void clear_gesture(void)
{
    tp.has_previous = false;
    tp.accumulator = 0;
}

static void close_touchpad(void)
{
    if (tp.disabled) {
        if (set_scroll(tp.original) != 0)
            fprintf(stderr, "Warning: could not restore scrolling on %s; use xinput manually.\n", tp.name);
    }
    if (tp.fd >= 0) close(tp.fd);
    memset(&tp, 0, sizeof(tp));
    tp.fd = -1;
    tp.xid = -1;
}

static bool is_touchpad(int fd, char *name, size_t name_size, double *units_per_mm)
{
    unsigned char events[(EV_MAX + 8) / 8] = {0};
    unsigned char absbits[(ABS_MAX + 8) / 8] = {0};
    if (ioctl(fd, EVIOCGBIT(0, sizeof(events)), events) < 0 || !BIT_TEST(events, EV_ABS))
        return false;
    if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbits)), absbits) < 0 ||
        !BIT_TEST(absbits, ABS_MT_SLOT) || !BIT_TEST(absbits, ABS_MT_POSITION_Y))
        return false;
    if (ioctl(fd, EVIOCGNAME((int)name_size), name) < 0) return false;
    /* Do not accidentally intercept a touchscreen / tablet. */
    if (!strstr(name, "Touchpad") && !strstr(name, "touchpad")) return false;
    struct input_absinfo axis;
    if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &axis) < 0) return false;
    *units_per_mm = axis.resolution > 0 ? axis.resolution :
                    (double)(axis.maximum - axis.minimum) / 76.0;
    return *units_per_mm > 0;
}

static void discover_touchpad(void)
{
    if (tp.fd >= 0) return;
    DIR *dir = opendir("/dev/input");
    if (!dir) return;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "event", 5) != 0) continue;
        char path[128];
        snprintf(path, sizeof(path), "/dev/input/%.100s", ent->d_name);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        char name[256] = {0};
        double scale = 0;
        if (!is_touchpad(fd, name, sizeof(name), &scale)) {
            close(fd);
            continue;
        }
        int id = -1;
        int original[3];
        if (!read_xinput_properties(name, &id, original)) {
            if (verbose) fprintf(stderr, "No matching XInput libinput scroll property: %s\n", name);
            close(fd);
            continue;
        }
        tp.fd = fd;
        tp.xid = id;
        snprintf(tp.name, sizeof(tp.name), "%s", name);
        snprintf(tp.path, sizeof(tp.path), "%s", path);
        memcpy(tp.original, original, sizeof(original));
        tp.threshold = step_mm * scale;
        fprintf(stderr, "Using %s (%s, XInput ID %d), %.1f units per step\n",
                tp.name, tp.path, tp.xid, tp.threshold);
        if (alt_mode) {
            int off[] = {0, 0, 0};
            if (set_scroll(off) == 0) tp.disabled = true;
        }
        break;
    }
    closedir(dir);
}

static void update_alt(void)
{
    bool now = alt_down();
    if (now == alt_mode) return;
    alt_mode = now;
    clear_gesture();
    if (tp.fd < 0) return;
    if (now && !tp.disabled) {
        int off[] = {0, 0, 0};
        if (set_scroll(off) == 0) tp.disabled = true;
        else fprintf(stderr, "Could not turn off touchpad scrolling.\n");
    } else if (!now && tp.disabled) {
        if (set_scroll(tp.original) == 0) tp.disabled = false;
        else fprintf(stderr, "Could not restore touchpad scrolling.\n");
    }
    if (verbose) fprintf(stderr, "Alt mode: %s\n", now ? "ON" : "OFF");
}

static void inject_zoom(unsigned int button)
{
    if (invert) button = button == 4 ? 5 : 4;
    XTestFakeButtonEvent(display, button, True, CurrentTime);
    XTestFakeButtonEvent(display, button, False, CurrentTime);
    XFlush(display);
}

static void process_sync(void)
{
    if (!alt_mode || !tp.disabled) { clear_gesture(); return; }
    int count = 0;
    double y = 0;
    for (size_t i = 0; i < ARRAYLEN(tp.touch); ++i) {
        if (tp.touch[i].active) {
            if (!tp.touch[i].has_y) { clear_gesture(); return; }
            count++;
            y += tp.touch[i].y;
        }
    }
    if (count != 2) { clear_gesture(); return; }
    y /= 2;
    if (!tp.has_previous) {
        tp.previous_y = y;
        tp.has_previous = true;
        return;
    }
    const double delta = y - tp.previous_y;
    tp.previous_y = y;
    tp.accumulator += delta;
    if (tp.accumulator > tp.threshold) tp.accumulator = tp.threshold;
    if (tp.accumulator < -tp.threshold) tp.accumulator = -tp.threshold;
    double now = monotonic_ms();
    if (now - tp.last_zoom_ms < interval_ms) return;
    if (tp.accumulator >= tp.threshold || tp.accumulator <= -tp.threshold) {
        inject_zoom(tp.accumulator < 0 ? 4 : 5);
        tp.accumulator = 0;
        tp.last_zoom_ms = now;
    }
}

static void read_touchpad(void)
{
    struct input_event events[64];
    ssize_t n;
    while ((n = read(tp.fd, events, sizeof(events))) > 0) {
        for (size_t i = 0; i < (size_t)n / sizeof(events[0]); ++i) {
            struct input_event *e = &events[i];
            if (e->type == EV_ABS) {
                if (e->code == ABS_MT_SLOT) {
                    if (e->value >= 0 && e->value < SLOT_COUNT) tp.slot = e->value;
                    else tp.slot = -1;
                } else if (tp.slot >= 0) {
                    Touch *t = &tp.touch[tp.slot];
                    if (e->code == ABS_MT_TRACKING_ID) {
                        t->active = e->value >= 0;
                        t->has_y = false;
                    } else if (e->code == ABS_MT_POSITION_Y && t->active) {
                        t->y = e->value;
                        t->has_y = true;
                    }
                }
            } else if (e->type == EV_SYN) {
                if (e->code == SYN_DROPPED) {
                    memset(tp.touch, 0, sizeof(tp.touch));
                    clear_gesture();
                } else if (e->code == SYN_REPORT) process_sync();
            }
        }
    }
    if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
        fprintf(stderr, "Touchpad disconnected; rediscovering.\n");
        close_touchpad();
    }
}

static void print_usage(const char *program)
{
    printf("Usage: %s [--step-mm 2.0] [--interval-ms 5] [--invert] [--verbose]\n", program);
    puts("X11/XFCE Alt + two-finger-scroll zoom without app scrolling.\n"
         "Requires xinput and read permission for /dev/input/event* touchpads.\n"
         "Touchpad-only in version 0.1; physical mouse wheel is NOT filtered.");
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            print_usage(argv[0]); return 0;
        } else if (!strcmp(argv[i], "--step-mm") && i+1 < argc) {
            step_mm = strtod(argv[++i], NULL);
        } else if (!strcmp(argv[i], "--interval-ms") && i+1 < argc) {
            interval_ms = strtod(argv[++i], NULL);
        } else if (!strcmp(argv[i], "--verbose")) verbose = true;
        else if (!strcmp(argv[i], "--invert")) invert = true;
        else { print_usage(argv[0]); return 2; }
    }
    if (!(step_mm > 0 && step_mm < 100 && interval_ms >= 0 && interval_ms <= 1000)) {
        fprintf(stderr, "Invalid step or interval.\n"); return 2;
    }
    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Cannot connect to X11. Run inside an Xorg session.\n");
        return 1;
    }
    int event_base, error_base, major, minor;
    if (!XTestQueryExtension(display, &event_base, &error_base, &major, &minor)) {
        fprintf(stderr, "XTest extension unavailable.\n");
        XCloseDisplay(display); return 1;
    }
    lalt = XKeysymToKeycode(display, XK_Alt_L);
    ralt = XKeysymToKeycode(display, XK_Alt_R);
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);
    signal(SIGHUP, handle_signal);
    fprintf(stderr, "xfce-alt-scroll 0.1: step=%.2fmm, interval=%.1fms (Ctrl+C to stop)\n",
            step_mm, interval_ms);
    double last_discovery = -10000;
    while (!stop_requested) {
        update_alt();
        double now = monotonic_ms();
        if (tp.fd < 0 && now - last_discovery > 2500) {
            discover_touchpad();
            last_discovery = now;
        }
        struct pollfd pfd = {.fd = tp.fd, .events = POLLIN};
        if (tp.fd >= 0) {
            int ready = poll(&pfd, 1, 8);
            if (ready > 0) {
                if (pfd.revents & POLLIN) read_touchpad();
                if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) && tp.fd >= 0)
                    close_touchpad();
            }
        } else poll(NULL, 0, 15);
    }
    close_touchpad();
    XCloseDisplay(display);
    fprintf(stderr, "Stopped; original scroll method restored.\n");
    return 0;
}
