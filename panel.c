

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <locale.h>
#include <string.h>
#include <signal.h>

#include "plugin.h"
#include "panel.h"
#include "misc.h"
#include "bg.h"

/* do not change this line - Makefile's 'tar' target depends on it */
#define VERSION "1.0"

static gchar *cfgfile = NULL;
static gchar version[] = VERSION;
static gchar *cprofile = "default";
int distance=0;
int expand=1 , padding=0;


//#define DEBUG
#include "dbg.h"


static panel *p;
static gchar *transparent_rc = "style 'transparent-style'\n"
"{\n"
"bg_pixmap[NORMAL] = \"<parent>\"\n"
"bg_pixmap[INSENSITIVE] = \"<parent>\"\n"
"bg_pixmap[PRELIGHT] = \"<parent>\"\n"
"bg_pixmap[SELECTED] = \"<parent>\"\n"
"bg_pixmap[ACTIVE] = \"<parent>\"\n"
"}\n"
"class \"GtkEventBox\" style \"transparent-style\"\n"
"class \"GtkSocket\" style \"transparent-style\"\n"
"class \"GtkBar\" style \"transparent-style\"\n"
"class \"GtkBox\" style \"transparent-style\"\n"
"\n";


/*
"class \"GtkBox\" style \"transparent-style\"\n"
"class \"GtkContainer\" style \"transparent-style\"\n"
"class \"GtkBin\" style \"transparent-style\"\n"
"class \"GtkSeparator\" style \"transparent-style\"\n"
*/

static void set_bg(GtkWidget *widget, panel *p);

/****************************************************
 *         panel's handlers for WM events           *
 ****************************************************/
/*
static void
panel_del_wm_strut(panel *p)
{
    XDeleteProperty(GDK_DISPLAY(), p->topxwin, a_NET_WM_STRUT);
    XDeleteProperty(GDK_DISPLAY(), p->topxwin, a_NET_WM_STRUT_PARTIAL);
}
*/

static void
panel_set_wm_strut(panel *p)
{
    unsigned long data[12] = { 0 };
    int i = 4;

    ENTER;
    if (!GTK_WIDGET_MAPPED (p->topgwin))
        return;
    switch (p->edge) {
    case EDGE_LEFT:
        i = 0;
        data[i] = p->aw;
        data[4 + i*2] = p->ay;
        data[5 + i*2] = p->ay + p->ah;
        break;
    case EDGE_RIGHT:
        i = 1;
        data[i] = p->aw;
        data[4 + i*2] = p->ay;
        data[5 + i*2] = p->ay + p->ah;
        break;
    case EDGE_TOP:
        i = 2;
        data[i] = p->ah;
        data[4 + i*2] = p->ax;
        data[5 + i*2] = p->ax + p->aw;
        break;
    case EDGE_BOTTOM:
        i = 3;
        data[i] = p->ah;
        data[4 + i*2] = p->ax;
        data[5 + i*2] = p->ax + p->aw;
        break;
    default:
        ERR("wrong edge %d. strut won't be set\n", p->edge);
        RET();
    }		
    DBG("type %d. width %d. from %d to %d\n", i, data[i], data[4 + i*2], data[5 + i*2]);

    /* if wm supports STRUT_PARTIAL it will ignore STRUT */
    XChangeProperty(GDK_DISPLAY(), p->topxwin, a_NET_WM_STRUT_PARTIAL,
          XA_CARDINAL, 32, PropModeReplace,  (unsigned char *) data, 12);
    /* old spec, for wms that do not support STRUT_PARTIAL */
    XChangeProperty(GDK_DISPLAY(), p->topxwin, a_NET_WM_STRUT,
          XA_CARDINAL, 32, PropModeReplace,  (unsigned char *) data, 4);

    RET();
}

static void
print_wmdata(panel *p)
{
    int i;

    ENTER;
    DBG("desktop %d/%d\n", p->curdesk, p->desknum);
    DBG("workarea\n");
    for (i = 0; i < p->wa_len/4; i++)
        DBG("(%d, %d) x (%d, %d)\n",
              p->workarea[4*i + 0],
              p->workarea[4*i + 1],
              p->workarea[4*i + 2],
              p->workarea[4*i + 3]);
    RET();
}


static GdkFilterReturn
panel_wm_events(GdkXEvent *xevent, GdkEvent *event, panel *p)
{
    Atom at;
    Window win;
    XEvent *ev = (XEvent *) xevent;

    ENTER;
    DBG("win = 0x%x\n", ev->xproperty.window);
    if (ev->type != PropertyNotify )
        RET(GDK_FILTER_CONTINUE);

    at = ev->xproperty.atom;
    win = ev->xproperty.window;
    if (win == GDK_ROOT_WINDOW()) {
	if (at == a_NET_CLIENT_LIST) {
            DBG("A_NET_CLIENT_LIST\n");
	} else if (at == a_NET_CURRENT_DESKTOP) {
            p->curdesk = get_net_current_desktop();
            DBG("A_NET_CURRENT_DESKTOP\n");
	} else if (at == a_NET_NUMBER_OF_DESKTOPS) {
            p->desknum = get_net_number_of_desktops();
            DBG("A_NET_NUMBER_OF_DESKTOPS\n");
	} else if (at == a_NET_ACTIVE_WINDOW) {
            DBG("A_NET_ACTIVE_WINDOW\n");
        } else if (at == a_XROOTPMAP_ID) {
            bg_rootbg_changed();
            set_bg(p->topgwin, p);
            gtk_widget_queue_draw(p->topgwin);
            DBG("a_XROOTPMAP_ID\n");
	} else if (at == a_NET_WORKAREA) {
            DBG("A_NET_WORKAREA\n");
            p->workarea = get_xaproperty (GDK_ROOT_WINDOW(), a_NET_WORKAREA, XA_CARDINAL, &p->wa_len);
            print_wmdata(p);
        }
    }
    RET(GDK_FILTER_CONTINUE);
}

/****************************************************
 *         panel's handlers for GTK events          *
 ****************************************************/


static gint
panel_delete_event(GtkWidget * widget, GdkEvent * event, gpointer data)
{
    ENTER;
    RET(FALSE);
}

static gint
panel_destroy_event(GtkWidget * widget, GdkEvent * event, gpointer data)
{
    //panel *p = (panel *) data;

    ENTER;
    //if (!p->self_destroy)
    gtk_main_quit();
    RET(FALSE);
}



static gint
panel_size_req(GtkWidget *widget, GtkRequisition *req, panel *p)
{
    ENTER;
    DBG("IN req=(%d, %d)\n", req->width, req->height);
    if (p->widthtype == WIDTH_REQUEST)
        p->width = (p->orientation == ORIENT_HORIZ) ? req->width : req->height;
    if (p->heighttype == HEIGHT_REQUEST)
        p->height = (p->orientation == ORIENT_HORIZ) ? req->height : req->width;
    calculate_position(p, distance);
    req->width  = p->aw;
    req->height = p->ah;
    DBG("OUT req=(%d, %d)\n", req->width, req->height);
    RET( TRUE );
}

static gint
panel_size_alloc(GtkWidget *widget, GtkAllocation *a, panel *p)
{
    ENTER;
    DBG("new alloc: size (%d, %d). pos (%d, %d)\n", a->width, a->height, a->x, a->y);
    DBG("old alloc: size (%d, %d). pos (%d, %d)\n", p->aw, p->ah, p->ax, p->ay);
    if (p->widthtype == WIDTH_REQUEST)
        p->width = (p->orientation == ORIENT_HORIZ) ? a->width : a->height;
    if (p->heighttype == HEIGHT_REQUEST)
        p->height = (p->orientation == ORIENT_HORIZ) ? a->height : a->width;
    calculate_position(p, distance);
    DBG("pref alloc: size (%d, %d). pos (%d, %d)\n", p->aw, p->ah, p->ax, p->ay);
    if (a->width == p->aw && a->height == p->ah && a->x == p->ax && a->y == p ->ay) {
        DBG("actual coords eq to preffered. just returning\n");
        RET(TRUE);
    }

    /*
    if (p->setstrut) {
        panel_del_wm_strut(p);
        gtk_window_move(GTK_WINDOW(p->topgwin), p->ax, p->ay);
        panel_set_wm_strut(p);
    } else {
        gtk_window_move(GTK_WINDOW(p->topgwin), p->ax, p->ay);
        }
    */
    gtk_window_move(GTK_WINDOW(p->topgwin), p->ax, p->ay);
    if (p->setstrut)
        panel_set_wm_strut(p);
    //gdk_window_clear(p->topgwin->window);
    RET(TRUE);
}




/****************************************************
 *         panel creation                           *
 ****************************************************/
static void
make_round_corners(panel *p)
{
    GtkWidget *b1, *b2, *img;
    GtkWidget *(*box_new) (gboolean, gint);
    void (*box_pack)(GtkBox *, GtkWidget *, gboolean, gboolean, guint);
    gchar *s1, *s2;
#define IMGPREFIX  PREFIX "/share/trayer/images/"

    ENTER;
    if (p->edge == EDGE_TOP) {
        s1 = IMGPREFIX "top-left.xpm";
        s2 = IMGPREFIX "top-right.xpm";
    } else if (p->edge == EDGE_BOTTOM) {
        s1 = IMGPREFIX "bottom-left.xpm";
        s2 = IMGPREFIX "bottom-right.xpm";
    } else if (p->edge == EDGE_LEFT) {
        s1 = IMGPREFIX "top-left.xpm";
        s2 = IMGPREFIX "bottom-left.xpm";
    } else if (p->edge == EDGE_RIGHT) {
        s1 = IMGPREFIX "top-right.xpm";
        s2 = IMGPREFIX "bottom-right.xpm";
    } else
        RET();

    box_new = (p->orientation == ORIENT_HORIZ) ? gtk_vbox_new : gtk_hbox_new;
    b1 = box_new(0, FALSE);
    gtk_widget_show(b1);
    b2 = box_new(0, FALSE);
    gtk_widget_show(b2);

    box_pack = (p->edge == EDGE_TOP || p->edge == EDGE_LEFT) ?
        gtk_box_pack_start : gtk_box_pack_end;

    img = gtk_image_new_from_file(s1);
    gtk_widget_show(img);
    box_pack(GTK_BOX(b1), img, FALSE, FALSE, 0);
    img = gtk_image_new_from_file(s2);
    gtk_widget_show(img);
    box_pack(GTK_BOX(b2), img, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(p->lbox), b1, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(p->lbox), b2, FALSE, FALSE, 0);
    RET();
}

static void
set_bg(GtkWidget *widget, panel *p)
{
    ENTER;
    /*
    if (p->xtopbg != None)
        XFreePixmap(GDK_DISPLAY(), p->xtopbg);
    p->xtopbg = bg_new_for_win(p->topxwin);
    if (p->gtopbg)
        g_object_unref(G_OBJECT(p->gtopbg));
    p->gtopbg = gdk_xid_table_lookup (p->xtopbg);
    if (p->gtopbg)
        g_object_ref (G_OBJECT (p->gtopbg));
    else
        p->gtopbg = gdk_pixmap_foreign_new (p->xtopbg);
    */
    if (p->gtopbg)
        g_object_unref(p->gtopbg);
    p->gtopbg = bg_new_for_win(p->topxwin);


    modify_drawable(p->gtopbg, p->topgwin->style->black_gc, p->tintcolor, p->alpha);

    gdk_window_set_back_pixmap(p->topgwin->window, p->gtopbg, FALSE);
    gdk_window_clear(p->topgwin->window);
    gtk_widget_queue_draw_area (p->topgwin, 0, 0, 2000, 2000);
    RET();
}

static void
panel_style_set(GtkWidget *widget, GtkStyle *s, panel *p)
{
    ENTER;

    gtk_rc_parse_string(transparent_rc);
    if (GTK_WIDGET_REALIZED(widget))
        set_bg(widget, p);
    RET();
}

static gboolean
panel_configure_event(GtkWidget *widget, GdkEventConfigure *event, panel *p)
{
    static gint x = 0, y = 0, width = 0, height = 0;

    ENTER;
    if (x == event->x && y == event->y
          && width == event->width && height == event->height)
        RET(FALSE);
    x = event->x;
    y = event->y;
    width = event->width;
    height = event->height;
    set_bg(widget, p);
    RET(FALSE);
}

void
panel_start_gui(panel *p)
{
    Atom state[3];
    XWMHints wmhints;
    unsigned int val;


    ENTER;
    //gtk_rc_parse_string(transparent_rc);
    p->topgwin =  gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width(GTK_CONTAINER(p->topgwin), 0);
    gtk_window_set_resizable(GTK_WINDOW(p->topgwin), FALSE);
    gtk_window_set_wmclass(GTK_WINDOW(p->topgwin), "panel", "trayer");
    gtk_window_set_title(GTK_WINDOW(p->topgwin), "panel");
    g_signal_connect(G_OBJECT(p->topgwin), "delete-event",
          G_CALLBACK(panel_delete_event), p);
    g_signal_connect(G_OBJECT(p->topgwin), "destroy-event",
          G_CALLBACK(panel_destroy_event), p);
    g_signal_connect (G_OBJECT (p->topgwin), "size-request",
          (GCallback) panel_size_req, p);
    g_signal_connect (G_OBJECT (p->topgwin), "size-allocate",
          (GCallback) panel_size_alloc, p);

    if (p->transparent) {
        g_signal_connect (G_OBJECT (p->topgwin), "configure-event",
              (GCallback) panel_configure_event, p);

        g_signal_connect (G_OBJECT (p->topgwin), "style-set",
              (GCallback) panel_style_set, p);
    }
    gtk_widget_realize(p->topgwin);
    gdk_window_set_decorations(p->topgwin->window, 0);
    gtk_widget_set_app_paintable(p->topgwin, TRUE);

    p->lbox = p->my_box_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(p->lbox), 0);
    gtk_container_add(GTK_CONTAINER(p->topgwin), p->lbox);
    gtk_widget_show(p->lbox);
    if (p->round_corners)
        make_round_corners(p);

    p->box = p->my_box_new(FALSE, 1);
    gtk_container_set_border_width(GTK_CONTAINER(p->box), 1);
    gtk_box_pack_start(GTK_BOX(p->lbox), p->box, TRUE, TRUE, 0);
    gtk_widget_show(p->box);

    p->topxwin = GDK_WINDOW_XWINDOW(GTK_WIDGET(p->topgwin)->window);

    bg_init(GDK_DISPLAY());

    /* make our window unfocusable */
    wmhints.flags = InputHint;
    wmhints.input = 0;
    XSetWMHints (GDK_DISPLAY(), p->topxwin, &wmhints);
    if (p->setdocktype) {
        state[0] = a_NET_WM_WINDOW_TYPE_DOCK;
        XChangeProperty(GDK_DISPLAY(), p->topxwin, a_NET_WM_WINDOW_TYPE, XA_ATOM,
              32, PropModeReplace, (unsigned char *) state, 1);
    }



#define WIN_HINTS_SKIP_FOCUS      (1<<0)	/* "alt-tab" skips this win */
    val = WIN_HINTS_SKIP_FOCUS;
    XChangeProperty(GDK_DISPLAY(), p->topxwin,
          XInternAtom(GDK_DISPLAY(), "_WIN_HINTS", False), XA_CARDINAL, 32,
          PropModeReplace, (unsigned char *) &val, 1);

    Xclimsg(p->topxwin, a_NET_WM_DESKTOP, 0xFFFFFFFF, 0, 0, 0, 0);

    /************************/
    /* Window Mapping Point */
    gtk_widget_show_all(p->topgwin);
    Xclimsg(p->topxwin, a_NET_WM_DESKTOP, 0xFFFFFFFF, 0, 0, 0, 0);

    state[0] = a_NET_WM_STATE_SKIP_PAGER;
    state[1] = a_NET_WM_STATE_SKIP_TASKBAR;
    state[2] = a_NET_WM_STATE_STICKY;
    XChangeProperty(GDK_DISPLAY(), p->topxwin, a_NET_WM_STATE, XA_ATOM,
          32, PropModeReplace, (unsigned char *) state, 3);



    XSelectInput (GDK_DISPLAY(), GDK_ROOT_WINDOW(), PropertyChangeMask);
    /*
      XSelectInput (GDK_DISPLAY(), topxwin, PropertyChangeMask | FocusChangeMask |
      StructureNotifyMask);
    */
    gdk_window_add_filter(gdk_get_default_root_window (), (GdkFilterFunc)panel_wm_events, p);

    calculate_position(p, distance);
    gdk_window_move_resize(p->topgwin->window, p->ax, p->ay, p->aw, p->ah);
    if (p->setstrut)
        panel_set_wm_strut(p);


    RET();
}

static int
panel_parse_global(panel *p)
{
    ENTER;
    p->orientation = (p->edge == EDGE_TOP || p->edge == EDGE_BOTTOM)
        ? ORIENT_HORIZ : ORIENT_VERT;
    if (p->orientation == ORIENT_HORIZ) {
        p->my_box_new = gtk_hbox_new;
        p->my_separator_new = gtk_vseparator_new;
    } else {
        p->my_box_new = gtk_vbox_new;
        p->my_separator_new = gtk_hseparator_new;
    }
    if (p->width < 0)
        p->width = 100;
    if (p->widthtype == WIDTH_PERCENT && p->width > 100)
        p->width = 100;
    p->heighttype = HEIGHT_PIXEL;
    if (p->heighttype == HEIGHT_PIXEL) {
        if (p->height < PANEL_HEIGHT_MIN)
            p->height = PANEL_HEIGHT_MIN;
        else if (p->height > PANEL_HEIGHT_MAX)
            p->height = PANEL_HEIGHT_MAX;
    }
    p->curdesk = get_net_current_desktop();
    p->desknum = get_net_number_of_desktops();
    p->workarea = get_xaproperty (GDK_ROOT_WINDOW(), a_NET_WORKAREA, XA_CARDINAL, &p->wa_len);
    print_wmdata(p);
    panel_start_gui(p);
    RET(1);
}

static int
panel_parse_plugin(panel *p)
{
    plugin *plug = NULL;
    FILE *tmpfp;

    ENTER;
    if (!(tmpfp = tmpfile())) {
        ERR( "can't open temporary file with tmpfile()\n");
        RET(0);
    }

    if (!(plug = plugin_load("tray"))) {
        ERR( "trayer: can't load systray\n" );
        goto error;
    }
    plug->panel = p;
    plug->fp = tmpfp;
    plug->expand = expand;
    plug->padding = padding;
    fprintf(tmpfp, "}\n");
    fseek(tmpfp, 0, SEEK_SET);
    if (!plugin_start(plug)) {
        ERR( "trayer: can't start systray\n" );
        goto error;
    }
    DBG("systray\n");
    p->plugins = g_list_append(p->plugins, plug);
    RET(1);

 error:
    fclose(tmpfp);
    if (plug)
          plugin_put(plug);
    RET(0);

}


int
panel_start(panel *p)
{
    line s;

    /* parse global section */
    ENTER;
    s.len = 256;

    if (!panel_parse_global(p))
        RET(0);

    if (!panel_parse_plugin(p))
        RET(0);

    gtk_widget_show_all(p->topgwin);
    print_wmdata(p);
    RET(1);
}

static void
delete_plugin(gpointer data, gpointer udata)
{
    ENTER;
    plugin_stop((plugin *)data);
    plugin_put((plugin *)data);
    RET();

}

void panel_stop(panel *p)
{
    ENTER;

    g_list_foreach(p->plugins, delete_plugin, NULL);
    g_list_free(p->plugins);
    p->plugins = NULL;
    XSelectInput (GDK_DISPLAY(), GDK_ROOT_WINDOW(), NoEventMask);
    gdk_window_remove_filter(gdk_get_default_root_window (), (GdkFilterFunc)panel_wm_events, p);
    gtk_widget_destroy(p->topgwin);
    g_free(p->workarea);
    RET();
}


void
usage()
{
    ENTER;
    printf("trayer %s - lightweight GTK2+ systray for UNIX desktops\n", version);
    printf("Command line options:\n");
    printf(" -h  -- print this help and exit:\n");
    printf(" -v  -- print version and exit:\n");
    printf(" --edge <left|right|top|bottom|none>\n");
    printf(" --align <left|right|center>\n");
    printf(" --margin <number>\n");
    printf(" --widthtype <request|pixel|percent>\n");
    printf(" --width <number>\n");
    printf(" --heighttype <pixel>\n");
    printf(" --height <number>\n");
    printf(" --SetDockType <true|false>\n");
    printf(" --SetPartialStrut <true|false>\n");
    printf(" --RoundCorners <true|false>\n");
    printf(" --transparent <true|false>\n");
    printf(" --alpha <number>\n");
    printf(" --tint <int>\n");
    printf(" --distance <number>\n");
    printf(" --expand <false|true>\n");
    printf(" --padding <number>\n");
    /*printf(" -p <name> -- use named profile. File ~/.trayer/<name> must exist\n");
    printf("\nVisit http://fbpanel.sourceforge.net/ for detailed documentation,\n");
    printf("sample profiles and other stuff.\n\n");*/
}

FILE *
open_profile(gchar *profile)
{
    gchar *fname;
    FILE *fp;

    ENTER;
    fname = g_strdup_printf("%s/.trayer/%s", getenv("HOME"), profile);
    if ((fp = fopen(fname, "r"))) {
        cfgfile = fname;
        ERR("Using %s\n", fname);
        RET(fp);
    }
    ERR("Can't load %s\n", fname);
    g_free(fname);

    /* check private configuration directory */
    fname = g_strdup_printf("%s/share/trayer/%s", PREFIX, profile);
    if ((fp = fopen(fname, "r"))) {
        cfgfile = fname;
        ERR("Using %s\n", fname);
        RET(fp);
    }
    ERR("Can't load %s\n", fname);
    g_free(fname);
    ERR("Can't open '%s' profile\n", profile);
    RET(NULL);
}

void
handle_error(Display * d, XErrorEvent * ev)
{
    char buf[256];

    ENTER;
    XGetErrorText(GDK_DISPLAY(), ev->error_code, buf, 256);
    ERR( "trayer : X error: %s\n", buf);
    RET();
}

/*
static void
sig_usr(int signum)
{
    if (signum != SIGUSR1)
        return;
    gtk_main_quit();
}
*/

int
main(int argc, char *argv[], char *env[])
{
    int i;

    ENTER;
    setlocale(LC_CTYPE, "");
    gtk_set_locale();
    gtk_init(&argc, &argv);
    XSetLocaleModifiers("");
    XSetErrorHandler((XErrorHandler) handle_error);
    resolve_atoms();

    p = g_new0(panel, 1);
    memset(p, 0, sizeof(panel));
    p->allign = ALLIGN_CENTER;
    p->edge = EDGE_BOTTOM;
    p->widthtype = WIDTH_PERCENT;
    p->width = 100;
    p->heighttype = HEIGHT_PIXEL;
    p->height = PANEL_HEIGHT_DEFAULT;
    p->setdocktype = 1;
    p->setstrut = 0;
    p->round_corners = 0;
    p->transparent = 0;
    p->alpha = 127;
    p->tintcolor = 0xFFFFFFFF;
    p->xtopbg = None;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage();
            exit(0);
        } else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--version")) {
            printf("trayer %s\n", version);
            exit(0);
/*
        } else if (!strcmp(argv[i], "--verbose")) {
            verbose = 1;
        } else if (!strcmp(argv[i], "--profile") || !strcmp(argv[i], "-p")) {
            i++;
            if (i == argc) {
                ERR( "trayer: missing profile name\n");
                usage();
                exit(1);
            } else {
                cprofile = g_strdup(argv[i]);
            }
*/
        } else if (!strcmp(argv[i], "--edge")) {
            i++;
            if (i == argc) {
                ERR( "trayer: missing edge parameter value\n");
                usage();
                exit(1);
            } else {
                p->edge = str2num(edge_pair, argv[i], EDGE_NONE);
            }
        } else if (!strcmp(argv[i], "--align")) {
            i++;
            if (i == argc) {
                ERR( "trayer: missing align parameter value\n");
                usage();
                exit(1);
            } else {
                p->allign = str2num(allign_pair, argv[i], ALLIGN_NONE);
            }
        } else if (!strcmp(argv[i], "--margin")) {
            i++;
            if (i == argc) {
                ERR( "trayer: missing margin parameter value\n");
                usage();
                exit(1);
            } else {
                p->margin = atoi(argv[i]);
            }
        } else if (!strcmp(argv[i], "--widthtype")) {
            i++;
            if (i == argc) {
                ERR( "trayer: missing widthtype parameter value\n");
                usage();
                exit(1);
            } else {
                p->widthtype = str2num(width_pair, argv[i], WIDTH_NONE);
            }
        } else if (!strcmp(argv[i], "--width")) {
            i++;
            if (i == argc) {
                ERR( "trayer: missing width parameter value\n");
                usage();
                exit(1);
            } else {
                p->width = atoi(argv[i]);
            }
        } else if (!strcmp(argv[i], "--heighttype")) {
            i++;
            if (i == argc) {
                ERR( "trayer: missing heighttype parameter value\n");
                usage();
                exit(1);
            } else {
                p->heighttype = str2num(height_pair, argv[i], HEIGHT_NONE);
            }
        } else if (!strcmp(argv[i], "--height")) {
            i++;
            if (i == argc) {
                ERR( "trayer: missing height parameter value\n");
                usage();
                exit(1);
            } else {
                p->height = atoi(argv[i]);
            }
        } else if (!strcmp(argv[i], "--SetDockType")) {
            i++;
            if (i == argc) {
                ERR( "trayer: missing SetDockType parameter value\n");
                usage();
                exit(1);
            } else {
                p->setdocktype = str2num(bool_pair, argv[i], 0);
            }
        } else if (!strcmp(argv[i], "--SetPartialStrut")) {
            i++;
            if (i == argc) {
                ERR( "trayer: missing SetPartialStrut parameter value\n");
                usage();
                exit(1);
            } else {
                p->setstrut = str2num(bool_pair, argv[i], 0);
            }
        } else if (!strcmp(argv[i], "--RoundCorners")) {
            i++;
            if (i == argc) {
                ERR( "trayer: missing RoundCorners parameter value\n");
                usage();
                exit(1);
            } else {
                p->round_corners = str2num(bool_pair, argv[i], 0);
            }
        } else if (!strcmp(argv[i], "--transparent")) {
            i++;
            if (i == argc) {
                ERR( "trayer: missing transparent parameter value\n");
                usage();
                exit(1);
            } else {
                p->transparent = str2num(bool_pair, argv[i], 1);
            }
        } else if (!strcmp(argv[i], "--alpha")) {
            i++;
            if (i == argc) {
                ERR( "trayer: missing alpha parameter value\n");
                usage();
                exit(1);
            } else {
                p->alpha = atoi(argv[i]);
            }
        } else if (!strcmp(argv[i], "--tint")) {
            i++;
            if (i == argc) {
                ERR( "trayer: missing tint parameter value\n");
                usage();
                exit(1);
            } else {
                p->tintcolor = strtoul(argv[i], NULL, 0);
            }
        } else if (!strcmp(argv[i], "--distance")) {
            i++;
            if (i == argc) {
                ERR( "trayer: missing distance parameter value\n");
                usage();
                exit(1);
            } else {
                distance = atoi(argv[i]);
            }
        } else if (!strcmp(argv[i], "--expand")) {
            i++;
            if (i == argc) {
                ERR( "trayer: missing expand parameter value\n");
                usage();
                exit(1);
            } else {
                expand = str2num(bool_pair, argv[i], 1);
            }
        } else if (!strcmp(argv[i], "--padding")) {
            i++;
            if (i == argc) {
                ERR( "trayer: missing padding parameter value\n");
                usage();
                exit(1);
            } else {
                padding = atoi(argv[i]);
            }
        } else {
            printf("trayer: unknown option - %s\n", argv[i]);
            usage();
            exit(1);
        }
    }
    g_return_val_if_fail (p != NULL, 1);
    if (!panel_start(p)) {
        ERR( "trayer: can't start panel\n");
        exit(1);
    }
    gtk_main();
    panel_stop(p);
    g_free(p);

    exit(0);
}

