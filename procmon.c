// procmon_ncurses_linux.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <ncurses.h>

// ===== CPU & MEM READERS =====
float get_cpu_usage() {
    static unsigned long long prev_total = 0, prev_idle = 0;
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return 0.0f;
    fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
           &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
    fclose(f);

    unsigned long long idle_time = idle + iowait;
    unsigned long long non_idle = user + nice + system + irq + softirq + steal;
    unsigned long long total = idle_time + non_idle;

    unsigned long long totald = total - prev_total;
    unsigned long long idled = idle_time - prev_idle;

    prev_total = total;
    prev_idle = idle_time;

    if (totald == 0) return 0.0f;
    return (float)(totald - idled) * 100.0f / (float)totald;
}

float get_mem_usage() {
    long total = 0, available = 0;
    char key[64];
    long value;
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return 0.0f;
    while (fscanf(f, "%63s %ld", key, &value) == 2) {
        if (strcmp(key, "MemTotal:") == 0) total = value;
        else if (strcmp(key, "MemAvailable:") == 0) available = value;
    }
    fclose(f);
    if (total == 0) return 0.0f;
    return (float)(total - available) * 100.0f / (float)total;
}

// ===== PROCESS ENUMERATION =====
void list_processes(int limit, int start_row) {
    DIR *d = opendir("/proc");
    if (!d) return;
    struct dirent *e;
    int shown = 0;

    mvprintw(start_row, 0, "%-8s %-24s %-10s", "PID", "NAME", "CPU TIME");

    while ((e = readdir(d)) && shown < limit) {
        if (!isdigit(e->d_name[0])) continue;

        char statpath[256];
        snprintf(statpath, sizeof(statpath), "/proc/%s/stat", e->d_name);

        FILE *sf = fopen(statpath, "r");
        if (!sf) continue;

        char comm[256];
        long utime = 0, stime = 0;
        int read = fscanf(sf, "%*d (%255[^)]) %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %ld %ld",
                          comm, &utime, &stime);
        fclose(sf);
        if (read < 3 && read != EOF) continue;

        long ticks = sysconf(_SC_CLK_TCK);
        float cpu_time = (utime + stime) / (float)ticks;

        mvprintw(start_row + shown + 1, 0, "%-8s %-24s %-10.1f", e->d_name, comm, cpu_time);
        shown++;
    }
    closedir(d);
}

// ===== MAIN LOOP =====
int main() {
    initscr();
    noecho();
    curs_set(FALSE);
    nodelay(stdscr, TRUE);
    start_color();
    init_pair(1, COLOR_GREEN, COLOR_BLACK);
    init_pair(2, COLOR_CYAN, COLOR_BLACK);

    (void)get_cpu_usage();
    sleep(1);

    while (1) {
        float cpu = get_cpu_usage();
        float mem = get_mem_usage();

        clear();
        attron(COLOR_PAIR(1));
        mvprintw(0, 0, "=== ProcMon Linux (ncurses) ===   [q] Quit");
        attroff(COLOR_PAIR(1));
        attron(COLOR_PAIR(2));
        mvprintw(2, 0, "CPU Usage: %.2f%%", cpu);
        mvprintw(3, 0, "Memory Usage: %.2f%%", mem);
        attroff(COLOR_PAIR(2));

        list_processes(10, 5);
        refresh();

        int ch = getch();
        if (ch == 'q' || ch == 'Q')
            break;

        usleep(400000); // refresh every 0.4s
    }

    endwin();
    return 0;
}

