#include <X11/X.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>
#include <panel.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <sys/wait.h>
#include <math.h>

#define X_OFFSET 1
#define Y_OFFSET 2
#define BAR "h(left/exit), j(down), k(up), l(right/select/enter) move | x delete | q quit | s sync monitors | r reload"

typedef struct {
    int width;
    int height;
    double refresh_rates[64];
    int refresh_count;
} ModeGroup;

typedef struct {
    char name[64];
    char is_primary;
    char active;
    char updated;

    int mm_width;
    int mm_height;
    int x, y;
    int res_width;
    int res_height;
    double refresh;

    ModeGroup modes[128];
    int mode_count;
} OutputInfo;

typedef struct {
    OutputInfo outputs[100];
    int count;
    int selected_row;
    int selected_column;
} DisplayInfo;

static double mode_refresh(const XRRModeInfo *mode) {
    if (mode->hTotal && mode->vTotal)
        return (double)mode->dotClock /
               ((double)mode->hTotal * (double)mode->vTotal);
    return 0.0;
}


int get_randr_outputs(DisplayInfo *info) {
    if (!info) return -1;

    memset(info, 0, sizeof(*info));

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) return -1;

    Window root = DefaultRootWindow(dpy);
    XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);
    if (!res) {
        XCloseDisplay(dpy);
        return -1;
    }

    RROutput primary = XRRGetOutputPrimary(dpy, root);
    for (int i = 0; i < res->noutput; i++) {
        XRROutputInfo *out = XRRGetOutputInfo(dpy, res, res->outputs[i]);
        
        if (!out || out->connection != RR_Connected) {
            if (out) XRRFreeOutputInfo(out);
            continue;
        }

        OutputInfo *dst = &info->outputs[info->count++];
        snprintf(dst->name, sizeof(dst->name), "%.*s",
                 out->nameLen, out->name);

        dst->is_primary = (res->outputs[i] == primary);
        dst->mm_width = out->mm_width;
        dst->mm_height = out->mm_height;

        if (out->crtc) {
            XRRCrtcInfo *crtc = XRRGetCrtcInfo(dpy, res, out->crtc);
            dst->x = crtc->x;
            dst->y = crtc->y;

            for (int m = 0; m < res->nmode; m++) {
                if (res->modes[m].id == crtc->mode) {
                    dst->res_width = res->modes[m].width;
                    dst->res_height = res->modes[m].height;
                    dst->refresh =
                        (double)res->modes[m].dotClock /
                        (res->modes[m].hTotal * res->modes[m].vTotal);
                    break;
                }
            }
            dst->active = True;
            XRRFreeCrtcInfo(crtc);
        }
        else{
            dst->active = False;
        }
        for (int j = 0; j < out->nmode; j++) {
            for (int m = 0; m < res->nmode; m++) {
                if (res->modes[m].id == out->modes[j]) {
                    for (int k = 0; k < dst->mode_count + 1; k++){
                        if (dst->modes[k].height == res->modes[m].height && dst->modes[k].width == res->modes[m].width){
                            ModeGroup *mi = &dst->modes[k];
                            mi->refresh_rates[mi->refresh_count++] =
                              (double)res->modes[m].dotClock /
                              (res->modes[m].hTotal * res->modes[m].vTotal);
                            break;
                        }
                        if (dst->modes[k].height == 0){
                            ModeGroup *mi = &dst->modes[dst->mode_count++];
                            mi->width = res->modes[m].width;
                            mi->height = res->modes[m].height;
                            //mi->mode_id[mi->refresh_count] = res->modes[m].id;
                            mi->refresh_rates[mi->refresh_count++] =
                              (double)res->modes[m].dotClock /
                              (res->modes[m].hTotal * res->modes[m].vTotal);
                            //printf("hell");
                            break;
                        }
                        
                    }
                }
            }
        }
        XRRFreeOutputInfo(out);
    }
    XRRFreeScreenResources(res);
    XCloseDisplay(dpy);
    return 0;
}



int apply_xrandr_lib(DisplayInfo *info)
{
    if (!info)
        return -1;

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy)
        return -1;

    Window root = DefaultRootWindow(dpy);
    XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);
    if (!res) {
        XCloseDisplay(dpy);
        return -1;
    }

    for (int i = 0; i < info->count; i++) {
        OutputInfo *o = &info->outputs[i];
        if (!o->updated)
            continue;

        o->updated = FALSE;

        /* find output by name */
        RROutput output = None;
        XRROutputInfo *out_info = NULL;

        for (int j = 0; j < res->noutput; j++) {
            out_info = XRRGetOutputInfo(dpy, res, res->outputs[j]);
            if (out_info && strcmp(out_info->name, o->name) == 0) {
                output = res->outputs[j];
                break;
            }
            XRRFreeOutputInfo(out_info);
            out_info = NULL;
        }

        if (!out_info || output == None)
            continue;

        if (!o->active) {
            if (out_info->crtc) {
                XRRSetCrtcConfig(
                    dpy, res, out_info->crtc,
                    CurrentTime,
                    0, 0,
                    None,
                    RR_Rotate_0,
                    NULL, 0
                );
            }
            XRRFreeOutputInfo(out_info);
            continue;
        }

        /* find matching mode */
        RRMode mode = None;
        for (int m = 0; m < out_info->nmode; m++) {
            for (int k = 0; k < res->nmode; k++) {
                XRRModeInfo *mi = &res->modes[k];
                if (mi->id == out_info->modes[m] &&
                    mi->width == o->res_width &&
                    mi->height == o->res_height) {

                    if (o->refresh > 0.0) {
                        double rate =
                            (double)mi->dotClock /
                            (mi->hTotal * mi->vTotal);
                        if (fabs(rate - o->refresh) > 0.5)
                            continue;
                    }
                    mode = mi->id;
                    break;
                }
            }
        }

        if (mode == None) {
            XRRFreeOutputInfo(out_info);
            continue;
        }

        RRCrtc crtc = out_info->crtc ?
                      out_info->crtc :
                      out_info->crtcs[0];

        XRRSetCrtcConfig(
            dpy, res, crtc,
            CurrentTime,
            o->x, o->y,
            mode,
            RR_Rotate_0,
            &output, 1
        );

        if (o->is_primary)
            XRRSetOutputPrimary(dpy, root, output);

        XRRFreeOutputInfo(out_info);
    }

    XRRFreeScreenResources(res);
    XFlush(dpy);
    XCloseDisplay(dpy);
    return 0;
}



void render_table(const DisplayInfo *info){
    int x = 2;
    mvprintw(1, x, "Output");
    x += 12;
    mvprintw(1, x, "Active");
    x += 12;
    mvprintw(1, x, "Primary");
    x += 12;
    mvprintw(1, x, "Resolution");
    x += 12;
    mvprintw(1, x, "Refresh");
    x += 12;
    mvprintw(1, x, "X Position");
    x += 12;
    mvprintw(1, x, "Y Position");
    x += 12;
    
    int y = 3;
    for (int i = 0; i < info->count; i++){
        x = 2;
        if (i == info->selected_row) {
            attron(A_REVERSE);
            mvhline(y, x , ' ', info->selected_column*12);
            attroff(A_REVERSE);
            mvhline(y, x + info->selected_column*12 , ' ', 12);
            attron(A_REVERSE);
            mvhline(y, x + (info->selected_column + 1)*12, ' ', 12*7 - (info->selected_column + 1)*12);  // fill entire line
        }
        else{
            attroff(A_REVERSE);
            mvhline(y, x, ' ', 12*7);  // fill entire line
        }
        if (info->selected_column == 0 && i == info->selected_row) {attroff(A_REVERSE);}
        if (info->outputs[i].updated) mvprintw(y, x, "*%s", info->outputs[i].name);
        else mvprintw(y, x, "%s", info->outputs[i].name);
        if (info->selected_column == 0 && i == info->selected_row) {attron(A_REVERSE);}
        x += 12;
        if (info->selected_column == 1 && i == info->selected_row) {attroff(A_REVERSE);}
        mvprintw(y, x, "%s", info->outputs[i].active ?  "True" : "False");
        if (info->selected_column == 1 && i == info->selected_row) {attron(A_REVERSE);}
        x += 12;
        if (info->selected_column == 2 && i == info->selected_row) {attroff(A_REVERSE);}
        mvprintw(y, x, "%s", info->outputs[i].is_primary ?  "True" : "False");
        if (info->selected_column == 2 && i == info->selected_row) {attron(A_REVERSE);}
        x += 12;
        if (info->selected_column == 3 && i == info->selected_row) {attroff(A_REVERSE);}
        mvprintw(y, x, "%dx%d", info->outputs[i].res_width, info->outputs[i].res_height);
        if (info->selected_column == 3 && i == info->selected_row) {attron(A_REVERSE);}
        x += 12;
        if (info->selected_column == 4 && i == info->selected_row) {attroff(A_REVERSE);}
        mvprintw(y, x, "%.2f", info->outputs[i].refresh);
        if (info->selected_column == 4 && i == info->selected_row) {attron(A_REVERSE);}
        x += 12;
        if (info->selected_column == 5 && i == info->selected_row) {attroff(A_REVERSE);}
        mvprintw(y, x, "%d", info->outputs[i].x);
        if (info->selected_column == 5 && i == info->selected_row) {attron(A_REVERSE);}
        x += 12;
        if (info->selected_column == 6 && i == info->selected_row) {attroff(A_REVERSE);}
        mvprintw(y, x, "%d", info->outputs[i].y);
        if (info->selected_column == 6 && i == info->selected_row) {attron(A_REVERSE);}
        y++;
        if (i == info->selected_row) attroff(A_REVERSE);
    }
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    mvprintw(rows - 1, 0, BAR);
}

void box_maker(int start_y, int start_x, int height, int width){
    for (int x = start_x; x < start_x + width; x++){
        mvaddch(start_y, x, ACS_HLINE);
        mvaddch(start_y + height - 1, x, ACS_HLINE);
    }
    for (int y = start_y; y < start_y + height; y++){
        mvaddch(y, start_x, ACS_VLINE);
        mvaddch(y, start_x + width - 1, ACS_VLINE);
    }
    mvaddch(start_y, start_x, ACS_ULCORNER);
    mvaddch(start_y, start_x + width - 1, ACS_URCORNER);
    mvaddch(start_y + height - 1, start_x, ACS_LLCORNER);
    mvaddch(start_y + height - 1, start_x + width - 1, ACS_LRCORNER);
}


///////////////////////////////////////////////////////////////////////////////
typedef struct {
    OutputInfo* row;
    char rownumber;
    char selected_row;
    char buffer[12];
    int buffer_size;
    char first;
} Widget;

typedef void (*ItemRenderer)(void *item, char *buf, size_t bufsize);
typedef int  (*ItemEquals)(void *item, void *userdata);


void draw_select_list(
    Widget *widg,
    void   *items,
    int     item_size,
    int     count,
    int     x, int y,
    int     width,
    ItemRenderer render,
    ItemEquals equals,
    void   *userdata
){
    box_maker(y, x, count + 2, width);
    y++; x++;

    for (int i = 0; i < count; i++) {
        void *item = (char*)items + i * item_size;
        char line[64];
        render(item, line, sizeof(line));
        int equiv = equals(item, userdata);
        if (equiv && widg->first) {
            widg->selected_row = i;
            widg->first = 0;
        }
        if ((!widg->first && i == widg->selected_row) || (widg->first && equiv)) {
            attron(A_REVERSE);
        }
        mvhline(y, x, ' ', width - 2);
        mvprintw(y, x, equiv ? "*%s" : "%s", line);
        attroff(A_REVERSE);
        y++;
    }
    if (widg->first)
        widg->first = 0;
}

void resolution_renderer(void *item, char *buf, size_t n){
    ModeGroup *m = item;
    snprintf(buf, n, "%dx%d", m->width, m->height);
}

int resolution_equals(void *item, void *userdata){
    ModeGroup *m = item;
    OutputInfo *info = userdata;
    return m->width == info->res_width &&
           m->height == info->res_height;
}

void selectwidget_resolution(Widget *widg){
    OutputInfo *info = widg->row;
    draw_select_list(
        widg,
        info->modes,
        sizeof(ModeGroup),
        info->mode_count,
        3 * 12,
        2 + widg->rownumber,
        14,
        resolution_renderer,
        resolution_equals,
        info
    );
}
void refresh_renderer(void *item, char *buf, size_t n){
    double *m = item;
    snprintf(buf, n, "%.2f", *m);
}


int refresh_equals(void *item, void *userdata){
    double *m = item;
    OutputInfo *info = userdata;
    return *m == info->refresh;
}


void selectwidget_refresh(Widget *widg){
    OutputInfo *info = widg->row;
    ModeGroup *mode;
    for (int i = 0; i < info->mode_count; i++) 
        if (resolution_equals(&info->modes[i], info)) 
            mode = &info->modes[i];
    draw_select_list(
        widg,
        mode->refresh_rates,
        sizeof(double),
        mode->refresh_count,
        4 * 12,
        2 + widg->rownumber,
        14,
        refresh_renderer,
        refresh_equals,
        info
    );
}

void active_renderer(void *item, char *buf, size_t n){
    int *m = item;
    snprintf(buf, n, *m ? "True" : "False");
}


int active_equals(void *item, void *userdata){
    int *m = item;
    OutputInfo *info = userdata;
    return *m == info->active;
}


void selectwidget_active(Widget *widg){
    OutputInfo *info = widg->row;
    int _sel[2] = {0, 1};
    draw_select_list(
        widg,
        _sel,
        sizeof(int),
        2,
        1 * 12,
        2 + widg->rownumber,
        14,
        active_renderer,
        active_equals,
        info
    );
}
///////////////////////////////////////////////////////////////////////////////
void bufferwiget(Widget *widg, int col){
    int x = col * 12;
    int y = 2 + widg->rownumber;
    box_maker(y, x, 3, 14);
    x++;
    y++;
    mvhline(y, x, ' ', 12);
    mvprintw(y, x, "%s", widg->buffer);
    attron(A_REVERSE);
    mvprintw(y, x + widg->buffer_size, " ");
    attroff(A_REVERSE);
    //todo add cool cursor
}


///////////////////////////////////////////////////////////////////////////////

typedef enum {
    MODE_TABLE,
    MODE_SELECT,
    MODE_WBUFFER,
} UiMode;

#define KEY_ESC        27
#define KEY_ENTER_1    10   /* '\n' */
#define KEY_ENTER_2    KEY_ENTER
#define KEY_BACKSPACE_1 127
#define KEY_BACKSPACE_2 KEY_BACKSPACE

#define RIGHCASE case 'l': case KEY_RIGHT:
#define LEFTCASE case 'h': case KEY_LEFT:
#define UPCASE case 'k': case KEY_UP:
#define DOWNCASE case 'j': case KEY_DOWN:


int main(void)
{
    DisplayInfo info = {0};
    Widget widg = {0};
    UiMode mode = MODE_TABLE;

    int rows, cols;
    int key;

    get_randr_outputs(&info);

    /* ncurses init */
    initscr();
    set_escdelay(0);
    getmaxyx(stdscr, rows, cols);
    keypad(stdscr, TRUE);
    
    noecho();
    curs_set(0);
    if (has_colors())
        init_color(0, 0, 0, 0);


    scrollok(stdscr, FALSE);
    info.selected_row = 0;
    render_table(&info);
    refresh();

    while ((key = getch()) != 'q') {

        int is_enter = key == KEY_ENTER || key == '\n';

        int is_esc = key == 27;

        int is_backspace = (8 == key || 127 == key || KEY_BACKSPACE == key);

        if (key == 'r') {
            get_randr_outputs(&info);
            continue;
        }

        if (key == 's') {
            apply_xrandr_lib(&info);
            render_table(&info);
            refresh();
            continue;
        }

        if (mode == MODE_TABLE && is_esc) break;

        switch (mode) {
        case MODE_TABLE:
            switch (key) {
            LEFTCASE
                if (info.selected_column > 0)
                    info.selected_column--;
                break;

            RIGHCASE
                if (info.selected_column < 6)
                    info.selected_column++;
                break;

            UPCASE
                if (info.selected_row > 0)
                    info.selected_row--;
                break;

            DOWNCASE
                if (info.selected_row < info.count - 1)
                    info.selected_row++;
                break;

            case 'i': case KEY_ENTER_1: case KEY_ENTER_2:
                if (info.selected_column == 0)
                    break;

                widg.row = &info.outputs[info.selected_row];
                widg.rownumber = info.selected_row;

                if (info.selected_column == 1 ||
                    info.selected_column == 3 ||
                    info.selected_column == 4) {

                    mode = MODE_SELECT;
                    widg.first = 1;

                } else if (info.selected_column == 5 ||
                           info.selected_column == 6) {

                    mode = MODE_WBUFFER;
                    widg.buffer_size =
                        snprintf(widg.buffer, sizeof(widg.buffer),
                                 "%d", widg.row->x);
                }
                break;
            }
            break;

        /* ================= SELECT MODE ================= */
        case MODE_SELECT:
            switch (key) {
            LEFTCASE case KEY_ESC:
                mode = MODE_TABLE;
                clear();
                break;
            UPCASE
                if (widg.selected_row > 0)
                    widg.selected_row--;
                break;

            DOWNCASE
                if (info.selected_column == 1 ||
                    info.selected_column == 2) {

                    if (widg.selected_row < 1)
                        widg.selected_row++;

                } else if (info.selected_column == 3) {

                    if (widg.selected_row <
                        widg.row->mode_count - 1)
                        widg.selected_row++;

                } else if (info.selected_column == 4) {

                    ModeGroup *m = NULL;
                    for (int i = 0; i < widg.row->mode_count; i++)
                        if (resolution_equals(&widg.row->modes[i], widg.row))
                            m = &widg.row->modes[i];

                    if (m && widg.selected_row < m->refresh_count - 1)
                        widg.selected_row++;
                }
                break;

            RIGHCASE case KEY_ENTER_1: case KEY_ENTER_2: 
                if (info.selected_column == 1) {
                    widg.row->active = widg.selected_row;
                    widg.row->updated = TRUE;

                } else if (info.selected_column == 3) {
                    ModeGroup *m = &widg.row->modes[widg.selected_row];
                    widg.row->res_width  = m->width;
                    widg.row->res_height = m->height;
                    widg.row->refresh    = m->refresh_rates[0];
                    widg.row->updated    = TRUE;

                } else if (info.selected_column == 4) {
                    ModeGroup *m = NULL;
                    for (int i = 0; i < widg.row->mode_count; i++)
                        if (resolution_equals(&widg.row->modes[i], widg.row))
                            m = &widg.row->modes[i];

                    if (m) {
                        widg.row->refresh =
                            m->refresh_rates[widg.selected_row];
                        widg.row->updated = TRUE;
                    }
                }
                mode = MODE_TABLE;
                clear();
                break;
            }
            break;

        case MODE_WBUFFER:
            if (key == 'h' || is_esc) {
                mode = MODE_TABLE;
                clear();
                break;
            }
            if (key == 'l' || is_enter) {
                if (info.selected_column == 5)
                    widg.row->x = atoi(widg.buffer);
                else if (info.selected_column == 6)
                    widg.row->y = atoi(widg.buffer);

                widg.row->updated = TRUE;
                mode = MODE_TABLE;
                clear();
                break;
            }

            if ((key == 'x' || is_backspace) && widg.buffer_size > 0) {
                widg.buffer[--widg.buffer_size] = '\0';
                break;
            }

            if (key >= '0' && key <= '9' &&
                widg.buffer_size < (int)sizeof(widg.buffer) - 1) {

                widg.buffer[widg.buffer_size++] = key;
                widg.buffer[widg.buffer_size] = '\0';
            }
            break;
        }
        switch (mode) {
        case MODE_TABLE:
            render_table(&info);
            break;

        case MODE_SELECT:
            if (info.selected_column == 1)
                selectwidget_active(&widg);
            else if (info.selected_column == 3)
                selectwidget_resolution(&widg);
            else if (info.selected_column == 4)
                selectwidget_refresh(&widg);
            break;

        case MODE_WBUFFER:
            bufferwiget(&widg, info.selected_column);
            break;
        }

        refresh();
    }

    endwin();
    return 0;
}

// int main(){
//     DisplayInfo info;
//     Widget widg;
//     UiMode mode =  MODE_TABLE;
//     //printf("Size: %d\n", sizeof(DisplayInfo));
//     int rows, cols;
//     int table_mode = 1;
   
//     get_randr_outputs(&info);
//     //if (get_randr_outputs(&info) == 0) {
//     //    for (int i = 0; i < info.count; i++) {
//     //        printf("Output: %s (%dx%d @ %.2fHz)\n",
//     //            info.outputs[i].name,
//     //            info.outputs[i].res_width,
//     //            info.outputs[i].res_height,
//     //            info.outputs[i].refresh);
//     //    }
//     //}
//     initscr();
//     getmaxyx(stdscr, rows, cols);
//     keypad(stdscr, TRUE);
//     if (has_colors()) init_color(0, 0, 0, 0);
//     curs_set(0);
//     noecho();
//     info.selected_row = 0;
//     render_table(&info);
//     mvprintw(rows-1, 0, BAR);
//     refresh(); 
//     //S
//     int key;
//     while(true) {
//         key = getchar();
//         if(key == 'q') break;
//         if(key == 'r') get_randr_outputs(&info);
//         if(key == 's') apply_xrandr_cli(&info);
//         switch (mode) {
//             case MODE_TABLE:
//                 switch (key) {
//                     case 'h':
//                         if (info.selected_column > 0) info.selected_column--;
//                         break;
//                     case 'l':
//                         if (info.selected_column < 7 - 1) info.selected_column++;
//                         break;

//                     case 'k':
//                         if (info.selected_row > 0) info.selected_row--;
//                         break;

//                     case 'j':
//                         if (info.selected_row < info.count - 1) info.selected_row++;
//                         break;

//                     case 'i':
//                         if (info.selected_column == 0)
//                             break;
//                         widg.row = &info.outputs[info.selected_row];
//                         widg.rownumber = info.selected_row;
//                         switch (info.selected_column) {
//                             case 1: case 3: case 4:
//                                 mode = MODE_SELECT;
//                                 widg.first = 1;
//                                 break;
//                             case 5: case 6:
//                                 mode = MODE_WBUFFER;
//                                 widg.buffer_size = snprintf(widg.buffer, 12, "%d", widg.row->x);
//                                 break;
//                         }
//                         break;
//                 }
//                 break;
//             case MODE_SELECT:
//                 switch (key) {
//                     case 'h':
//                         mode = MODE_TABLE;
//                         clear();
//                         mvprintw(
//                             rows - 1, 0, BAR
//                         );
//                         break;

//                     case 'k':
//                         if (widg.selected_row > 0)
//                             widg.selected_row--;
//                         break;

//                     case 'j':
//                         switch (info.selected_column) {
//                              case 1: case 2:
//                                 if (widg.selected_row < 1)
//                                     widg.selected_row++;
//                                 break;
//                             case 3:
//                                 if (widg.selected_row < widg.row->mode_count - 1)
//                                     widg.selected_row++;
//                                 break;
//                             case 4:
//                                 OutputInfo *_info = widg.row;
//                                 ModeGroup *_mode;
//                                 for (int i = 0; i < _info->mode_count; i++) 
//                                     if (resolution_equals(&_info->modes[i], _info)) 
//                                         _mode = &_info->modes[i];
//                                 if (widg.selected_row < _mode->refresh_count - 1)
//                                     widg.selected_row++;
//                                 break;
//                         }
//                         break;
//                     case 'l':
//                         switch (info.selected_column) {
//                             case 1:
//                                 widg.row->active = widg.selected_row;
//                                 widg.row->updated = TRUE;
//                                 break;
//                             case 3:
//                                 widg.row->res_height =
//                                     widg.row->modes[widg.selected_row].height;
//                                 widg.row->res_width =
//                                     widg.row->modes[widg.selected_row].width;
//                                 widg.row->refresh = widg.row->modes[widg.selected_row].refresh_rates[0];
//                                 widg.row->updated = TRUE;
//                                 break;
//                             case 4:
//                                 OutputInfo *info = widg.row;
//                                 ModeGroup *_mode;
//                                 for (int i = 0; i < info->mode_count; i++) 
//                                     if (resolution_equals(&info->modes[i], info)) 
//                                         _mode = &info->modes[i];
//                                 widg.row->refresh = _mode->refresh_rates[widg.selected_row];
//                                 widg.row->updated = TRUE;
//                                 break;
//                         }
//                         break;
//                 }
//                 break;
//             case MODE_WBUFFER:
//                 if (key == 'h'){
//                     mode = MODE_TABLE;
//                     clear();
//                     mvprintw(
//                         rows - 1, 0,
//                         "h(left), j(down), k(up), l(right) move | "
//                         "i edit | h exit edit | q quit | "
//                         "s sync monitors | r reload"
//                     );
//                 }
//                 if (key == 'l'){
//                     if (info.selected_column == 5) widg.row->x = atoi(widg.buffer);
//                     if (info.selected_column == 6) widg.row->y = atoi(widg.buffer);
//                     widg.row->updated = TRUE;
//                 }
//                 if (key == 'x' && widg.buffer_size > 0){
//                     widg.buffer[--widg.buffer_size] = '\0';
//                 }
//                 if ('0' <= key && key <= '9' && widg.buffer_size < 11){
//                     widg.buffer[widg.buffer_size++] = key;
//                     widg.buffer[widg.buffer_size] = '\0';
//                 }
//                 break;

//         }
            
        
//         switch (mode) {
//             case MODE_TABLE:
//                 render_table(&info);
//                 break;
//             case MODE_SELECT:
//                 switch (info.selected_column) {
//                     case 1:
//                         selectwidget_active(&widg);
//                         break;
//                     case 3:
//                         selectwidget_resolution(&widg);
//                         break;
//                     case 4:
//                         selectwidget_refresh(&widg);
//                         break;
                    
//                 }
//                 break;
//             case MODE_WBUFFER:
//                 bufferwiget(&widg, info.selected_column);
//                 break;
//         } 
//         refresh(); 
//     }
//     endwin();
//     return 0;
// }
