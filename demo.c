#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ncurses.h>

// ---- CPU + MEM readers (same as before) ----
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

// ---- Graph buffers ----
#define WIDTH 50
float cpu_hist[WIDTH]={0};
float mem_hist[WIDTH]={0};

void push(float *arr,float val){
    memmove(arr,arr+1,sizeof(float)*(WIDTH-1));
    arr[WIDTH-1]=val;
}

// ---- Draw bar/graph ----
void draw_bar(int row, const char *label, float val, int color){
    attron(COLOR_PAIR(color));
    mvprintw(row, 0, "%-6s [", label);
    int bar = (int)(val/2); // scale 0-50
    for(int i=0;i<50;i++){
        if(i<bar) addch('#');
        else addch(' ');
    }
    printw("] %5.1f%%", val);
    attroff(COLOR_PAIR(color));
}

void draw_graph(int start_row, float *data, int color){
    attron(COLOR_PAIR(color));
    for(int x=0;x<WIDTH;x++){
        int height = (int)(data[x]/10); // scale 0–10 lines
        for(int y=0;y<height;y++){
            mvaddch(start_row - y, x, '|');
        }
    }
    attroff(COLOR_PAIR(color));
}

// ---- Main loop ----
int main(){
    initscr(); noecho(); curs_set(FALSE);
    nodelay(stdscr, TRUE);
    start_color();
    init_pair(1,COLOR_GREEN,COLOR_BLACK);
    init_pair(2,COLOR_CYAN,COLOR_BLACK);
    init_pair(3,COLOR_YELLOW,COLOR_BLACK);

    (void)get_cpu_usage(); sleep(1);
    while(1){
        float cpu=get_cpu_usage();
        float mem=get_mem_usage();
        push(cpu_hist,cpu);
        push(mem_hist,mem);

        clear();
        mvprintw(0,0,"=== ProcMon Graph (press q to quit) ===");

        draw_bar(2,"CPU",cpu,1);
        draw_bar(3,"MEM",mem,2);

        // Graph axes
        for(int i=0;i<WIDTH;i++)
            mvaddch(15,i,'_');

        mvprintw(5,0,"CPU Graph:");
        draw_graph(14,cpu_hist,1);

        mvprintw(17,0,"MEM Graph:");
        draw_graph(26,mem_hist,2);

        refresh();

        int ch=getch();
        if(ch=='q'||ch=='Q')break;
        usleep(200000); // 0.2s
    }

    endwin();
    return 0;
}

