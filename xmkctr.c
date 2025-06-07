#define _DEFAULT_SOURCE
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <glob.h>
#include <signal.h>
#include <limits.h>
#include <libgen.h>
#include <errno.h>
#include <sys/stat.h>

// Globals
static int xi_opcode;

typedef struct {
    int mouse_move_distance;
    int left_click_count;
    int right_click_count;
    int key_press_count;
} InputCounts;

InputCounts counts = {0, 0, 0, 0};

static int last_x = -1, last_y = -1;

static char current_datetime[20] = ""; // "YYYYMMDD_HH_MM" + NULL

static char previous_lock_file[100] = "";

// Flag for signal handler
volatile sig_atomic_t stop = 0;

// Output dir
char output_dir[PATH_MAX] = "";

// SIgnal handler
void handle_sigint(int sig) {
    stop = 1;
}

void delete_all_lock_files(const char *pattern) {
    glob_t glob_result;
    int ret = glob(pattern, 0, NULL, &glob_result);
    if (ret == 0) {
        for (size_t i = 0; i < glob_result.gl_pathc; i++) {
            remove(glob_result.gl_pathv[i]);
        }
        globfree(&glob_result);
    }
}

void reset_counts() {
    counts.mouse_move_distance = 0;
    counts.left_click_count = 0;
    counts.right_click_count = 0;
    counts.key_press_count = 0;
}

// CSV flasher
void log_counts_to_csv(FILE *file, struct tm *local_time) {
    if (file == NULL) {
        fprintf(stderr, "File pointer is NULL. Unable to log counts.\n");
        return;
    }

    char iso_time[26];
    strftime(iso_time, sizeof(iso_time), "%Y-%m-%dT%H:%M:%S%z", local_time);
    if (iso_time[20] != 'Z') {
        // 01234567890123456789012345
        // YYYY-MM-DDTHH:mm:ss+zzzz00
        memmove(&iso_time[23], &iso_time[22], 3);
        iso_time[22] = ':';
        iso_time[25] = '\0';
    }

    fprintf(file, "%s,%d,%d,%d,%d\n",
            iso_time,
            counts.mouse_move_distance,
            counts.left_click_count,
            counts.right_click_count,
            counts.key_press_count);
    fflush(file);
}

void create_lock_file(const char *filename) {
    FILE *file = fopen(filename, "w");
    if (file != NULL) {
        fclose(file);
    }
}

void delete_lock_file(const char *filename) {
    remove(filename);
}

// Count event handler
void handle_event(XEvent *event) {
    if (event->type == GenericEvent && event->xcookie.extension == xi_opcode) {
        XGetEventData(event->xany.display, &event->xcookie);
        if (event->xcookie.evtype == XI_KeyPress) {
            counts.key_press_count++;
        } else if (event->xcookie.evtype == XI_ButtonPress) {
            XIDeviceEvent *device_event = (XIDeviceEvent *)event->xcookie.data;
            if (device_event->detail == 1) {
                counts.left_click_count++;
            } else if (device_event->detail == 3) {
                counts.right_click_count++;
            }
        } else if (event->xcookie.evtype == XI_Motion) {
            XIDeviceEvent *device_event = (XIDeviceEvent *)event->xcookie.data;
            if (last_x != -1 && last_y != -1) {
                int dx = device_event->event_x - last_x;
                int dy = device_event->event_y - last_y;
                counts.mouse_move_distance += abs(dx) + abs(dy);
            }
            last_x = device_event->event_x;
            last_y = device_event->event_y;
        }
        XFreeEventData(event->xany.display, &event->xcookie);
    }
}

int get_executable_path(char *buffer, size_t size) {
    ssize_t len = readlink("/proc/self/exe", buffer, size - 1);
    if (len == -1) {
        perror("readlink");
        return -1;
    }
    buffer[len] = '\0';
    return 0;
}

int main() {
    // Ser signal handler
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // Get progran dir
    char exe_path[PATH_MAX];
    if (get_executable_path(exe_path, sizeof(exe_path)) == -1) {
        exit(EXIT_FAILURE);
    }

    // Set xmkctr.txt path
    char config_path[PATH_MAX];
    snprintf(config_path, sizeof(config_path), "%s/xmkctr.txt", dirname(exe_path));

    // Get out dir from xmkctr.txt
    FILE *config_file = fopen(config_path, "r");
    if (config_file == NULL) {
        fprintf(stderr, "Unable to open config file: %s\n", config_path);
        exit(EXIT_FAILURE);
    }

    if (fgets(output_dir, sizeof(output_dir), config_file) == NULL) {
        fprintf(stderr, "Failed to read output directory from config file.\n");
        fclose(config_file);
        exit(EXIT_FAILURE);
    }
    fclose(config_file);

    size_t len = strlen(output_dir);
    if (len > 0 && output_dir[len - 1] == '\n') {
        output_dir[len - 1] = '\0';
    }

    // Check out dir exists
    struct stat st = {0};
    if (stat(output_dir, &st) == -1) {
        fprintf(stderr, "Output directory does not exist: %s\n", output_dir);
        exit(EXIT_FAILURE);
    }

    // Connect to X display
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Unable to open display\n");
        return -1;
    }

    // Check XInput ext
    int devent, derror;
    if (!XQueryExtension(display, "XInputExtension", &xi_opcode, &devent, &derror)) {
        fprintf(stderr, "X Input extension not available\n");
        XCloseDisplay(display);
        return -1;
    }

    // Get root window
    Window root = DefaultRootWindow(display);

    // Set event mask
    XIEventMask event_mask;
    unsigned char mask[XIMaskLen(XI_LASTEVENT)] = { 0 };
    event_mask.deviceid = XIAllDevices;
    event_mask.mask_len = sizeof(mask);
    event_mask.mask = mask;
    XISetMask(mask, XI_KeyPress);
    XISetMask(mask, XI_ButtonPress);
    XISetMask(mask, XI_Motion);
    XISelectEvents(display, root, &event_mask, 1);
    // Send to the server
    XFlush(display);

    // Delete all *.lck
    char lock_pattern[PATH_MAX];
    snprintf(lock_pattern, sizeof(lock_pattern), "%s/*.lck", output_dir);
    delete_all_lock_files(lock_pattern);
    // Decide file name
    time_t current_time = time(NULL);
    struct tm *local_time = localtime(&current_time);
    strftime(current_datetime, sizeof(current_datetime), "%Y%m%d_%H", local_time);
    // Save current date time
    int last_logged_hour = local_time->tm_hour;
    int last_logged_minute = local_time->tm_min;
    // CSV open
    char filename[PATH_MAX];
    snprintf(filename, sizeof(filename), "%s/%s.csv", output_dir, current_datetime);
    FILE *csv_file = fopen(filename, "a");
    if (csv_file == NULL) {
        fprintf(stderr, "Unable to open initial CSV file: %s\n", filename);
        XCloseDisplay(display);
        return -1;
    }
    // Make lck
    char lock_filename[PATH_MAX];
    snprintf(lock_filename, sizeof(lock_filename), "%s/%s.lck", output_dir, current_datetime);
    create_lock_file(lock_filename);
    strcpy(previous_lock_file, lock_filename);

    int xfd = ConnectionNumber(display);
    // Main loop
    while (!stop) {
        current_time = time(NULL);
        local_time = localtime(&current_time);
        // Event timeout
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 10000; // 10ms
        // Init file descriptor
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(xfd, &fds);
        // Open event
        int ret = select(xfd + 1, &fds, NULL, NULL, &timeout);
        if (ret == -1) {
            perror("Cannot select event");
            break;
        }
        if (ret > 0 && FD_ISSET(xfd, &fds)) {
            // Event occured
            while (XPending(display)) {
                XEvent event;
                XNextEvent(display, &event);
                handle_event(&event);
            }
        }
        // Hour or day changed
        if (local_time->tm_hour != last_logged_hour) {
            // Delete last lck
            if (strlen(previous_lock_file) > 0) {
                delete_lock_file(previous_lock_file);
            }
            // Set new file name
            strftime(current_datetime, sizeof(current_datetime), "%Y%m%d_%H", local_time);
            snprintf(filename, sizeof(filename), "%s/%s.csv", output_dir, current_datetime);
            // Open new CSV
            csv_file = fopen(filename, "a");
            if (csv_file == NULL) {
                fprintf(stderr, "Unable to open new CSV file: %s\n", filename);
                break;
            }
            // Open new lck
            snprintf(lock_filename, sizeof(lock_filename), "%s/%s.lck", output_dir, current_datetime);
            create_lock_file(lock_filename);
            strcpy(previous_lock_file, lock_filename);
            // Renew last save time
            last_logged_hour = local_time->tm_hour;
        }
        // Min changed
        if (local_time->tm_min != last_logged_minute) {
            log_counts_to_csv(csv_file, local_time);
            reset_counts();
            last_logged_minute = local_time->tm_min;
        }

        // Wait 10ms
        usleep(10000);
    }

    // SIGINT
    printf("\nSIGINT received. Exiting gracefully...\n");
    // Delete current lck
    if (strlen(previous_lock_file) > 0) {
        delete_lock_file(previous_lock_file);
    }
    if (csv_file != NULL) {
        fclose(csv_file);
    }
    XCloseDisplay(display);
    return 0;
}
