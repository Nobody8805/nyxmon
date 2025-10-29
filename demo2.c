#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <ncurses.h>

// ---------- CPU + MEM SAMPLING ----------
float get_cpu_usage() {
    static unsigned long long prev_total=0, prev_idle=0;
    unsigned long long user,nice,system,idle,iowait,irq,softirq,steal;
    FILE *f=fopen("/proc/stat","r");
    if(!f)return 0;
    fscanf(f,"cpu %llu %llu %llu %llu %llu %llu %llu %llu",
        &user,&nice,&system,&idle,&iowait,&irq,&softirq,&steal);
    fclose(f);
    unsigned long long idle_time=idle+iowait;
    unsigned long long non_idle=user+nice+system+irq+softirq+steal;
    unsigned long long total=idle_time+non_idle;
    unsigned long long totald=total-prev_total;
    unsigned long long idled=idle_time-prev_idle;
    prev_total=total; prev_idle=idle_time;
    if(totald==0)return 0;
    return (float)(totald-idled)/totald*100.0f;
}

float get_mem_usage(){
    long total=0,avail=0; char k[64]; long v;
    FILE*f=fopen("/proc/meminfo","r");
    if(!f)return 0;
    while(fscanf(f,"%63s %ld",k,&v)==2){
        if(strcmp(k,"MemTotal:")==0)total=v;
        else if(strcmp(k,"MemAvailable:")==0)avail=v;
    }
    fclose(f);
    if(total==0)return 0;
    return (float)(total-avail)/total*100.0f;
}

// ---------- PROCESS STRUCTURE ----------
typedef struct {
    int pid;
    char name[32];
    float cpu_time;
    float mem_percent;
} Proc;

// ---------- GET PROCESS LIST ----------
int get_processes(Proc *list, int max) {
    DIR *d = opendir("/proc");
    if(!d) return 0;
    struct dirent *e;
    int count=0;
    long mem_total=0;
    FILE *fm=fopen("/proc/meminfo","r");
    if(fm){
        char k[64]; long v;
        while(fscanf(fm,"%63s %ld",k,&v)==2)
            if(strcmp(k,"MemTotal:")==0){ mem_total=v; break; }
        fclose(fm);
    }
    while((e=readdir(d)) && count<max){
        if(!isdigit(e->d_name[0])) continue;
        char statpath[256];
        snprintf(statpath,sizeof(statpath),"/proc/%s/stat",e->d_name);
        FILE *sf=fopen(statpath,"r");
        if(!sf) continue;
        char comm[256]; long utime=0,stime=0; long rss=0;
        int res=fscanf(sf,"%*d (%255[^)]) %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %ld %ld %*d %*d %*d %*d %*d %*d %ld",
                        comm,&utime,&stime,&rss);
        fclose(sf);
        if(res<3) continue;
        long ticks=sysconf(_SC_CLK_TCK);
        float cpu_time=(utime+stime)/(float)ticks;
        float memp = (rss*4.0f)/mem_total; // rough %
        list[count].pid=atoi(e->d_name);
        strncpy(list[count].name,comm,31);
        list[count].cpu_time=cpu_time;
        list[count].mem_percent=memp;
        count++;
    }
    closedir(d);
    return count;
}

// ---------- DRAW UTILITIES ----------
void draw_bar(int row, const char *label, float val, int color){
    attron(COLOR_PAIR(color));
    mvprintw(row, 0, "%-6s [", label);
    int bar=(int)(val/2);
    for(int i=0;i<50;i++)
        addch(i<bar?'#':' ');
    printw("] %5.1f%%",val);
    attroff(COLOR_PAIR(color));
}

// ---------- MAIN ----------
int main(){
    initscr(); noecho(); curs_set(FALSE);
    nodelay(stdscr,FALSE); // wait for input
    keypad(stdscr,TRUE);
    start_color();
    init_pair(1,COLOR_GREEN,COLOR_BLACK);
    init_pair(2,COLOR_CYAN,COLOR_BLACK);
    init_pair(3,COLOR_YELLOW,COLOR_BLACK);

    (void)get_cpu_usage(); sleep(1);
    int offset=0;
    while(1){
        float cpu=get_cpu_usage();
        float mem=get_mem_usage();
        Proc list[1024];
        int total=get_processes(list,1024);

        clear();
        attron(COLOR_PAIR(3));
        mvprintw(0,0,"ProcMon (mini-htop)  [UP/DOWN to scroll, q to quit]");
        attroff(COLOR_PAIR(3));
        draw_bar(1,"CPU",cpu,1);
        draw_bar(2,"MEM",mem,2);

        mvprintw(4,0,"%-6s %-25s %-10s %-8s","PID","NAME","CPU(s)","MEM%");
        mvhline(5,0,'-',70);

        for(int i=0;i<20 && (i+offset)<total;i++){
            Proc *p=&list[i+offset];
            mvprintw(6+i,0,"%-6d %-25.25s %-10.1f %-8.2f",
                     p->pid,p->name,p->cpu_time,p->mem_percent);
        }

        int ch=getch();
        if(ch=='q')break;
        if(ch==KEY_DOWN && offset<total-20)offset++;
        if(ch==KEY_UP && offset>0)offset--;

        refresh();
        usleep(300000);
    }

    endwin();
    return 0;
}

