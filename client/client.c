#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cairo.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#define SCREEN_FRAC 1.0f        /* fraction of screen height used for FFT */
#define DYNAMIC_RANGE 70.f      /* -dBFS coreresponding to bottom of screen */

static float    scale;
static int      yzero = 0;
static int      text_margin = 0;

static gint     width, height;  /* screen width and height */
static gboolean freq_changed = TRUE;

static int      fft_size = 640;
static float   *log_pwr_fft;    /* dbFS relative to 1.0 */

static struct sockaddr_in addr;
static int fd, nbytes;
static unsigned int addrlen;

static void tcp_connect(const char* server)
{
    /* create socket */
    if ((fd=socket(AF_INET,SOCK_STREAM,0)) < 0) {
        perror("socket");
        exit(1);
    }

    /* set up destination address */
    memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_port=htons(8888);
    if(inet_pton(AF_INET, server, &addr.sin_addr) <= 0) 
    {
        perror("\nInvalid address/ Address not supported");
        printf("Note: Hostnames are not supported, only IP addresses\n");
        exit(1);
    }
    
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        perror("Connect failed");
        exit(1);
    }
    
}

static gboolean delete_event(GtkWidget * widget, GdkEvent * e, gpointer d)
{
    return FALSE;
}

static void destroy(GtkWidget * widget, gpointer data)
{
    gtk_main_quit();
}

gint keypress_cb(GtkWidget * widget, GdkEvent * event, gpointer data)
{
    guint           event_handled = TRUE;

    switch (event->key.keyval)
    {
    case GDK_KEY_Return:
        /* exit application */
        gtk_widget_destroy(widget);
        break;
    default:
        event_handled = FALSE;
        break;
    }

    return event_handled;
}

static int db_to_pixel(float dbfs)
{
    return yzero + (int)(-dbfs * scale);
}

static void draw_text(cairo_t * cr)
{
    cairo_text_extents_t cte;
    double          txt1_y, txt2_y;

    gchar          *freq_str =
        g_strdup_printf("?? MHz");
    gchar          *delta_str = g_strdup_printf("BW: ?? kHz   RBW: ?? kHz");

    /* clear area */
    cairo_set_source_rgb(cr, 0.02, 0.02, 0.09);
    cairo_set_line_width(cr, 1.0);
    cairo_rectangle(cr, 0, 0, width, yzero);
    cairo_stroke_preserve(cr);
    cairo_fill(cr);

    cairo_select_font_face(cr, "Sans",
                           CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 24);
    cairo_text_extents(cr, freq_str, &cte);
    txt1_y = text_margin + cte.height;
    cairo_set_source_rgba(cr, 0.97, 0.98, 0.02, 0.8);
    cairo_move_to(cr, text_margin, txt1_y);
    cairo_show_text(cr, freq_str);

    cairo_set_font_size(cr, 12);
    cairo_text_extents(cr, delta_str, &cte);
    txt2_y = txt1_y + cte.height + text_margin;
    cairo_set_source_rgba(cr, 0.97, 0.98, 0.02, 0.8);
    cairo_move_to(cr, text_margin, txt2_y);
    cairo_show_text(cr, delta_str);

    g_free(freq_str);
    g_free(delta_str);
}

static void draw_fft(cairo_t * cr)
{
    cairo_set_source_rgb(cr, 0.02, 0.02, 0.09);
    cairo_set_line_width(cr, 1.0);

    cairo_rectangle(cr, 0, yzero, width, height);
    cairo_stroke_preserve(cr);
    cairo_fill(cr);

    //cairo_set_source_rgba(cr, 0.30, 0.450, 0.60, 1.0);
    cairo_set_source_rgba(cr, 0.7, 0.7, 0.7, 0.9);

    int             x, y;

    for (x = 0; x < width; x++)
    {
        y = db_to_pixel(log_pwr_fft[x]);
        g_random_int_range(10, 100);
        cairo_move_to(cr, x, height);
        cairo_line_to(cr, x, y);
    }

    cairo_stroke(cr);
}

gint timeout_cb(gpointer darea)
{
    // populate fft data via socket
    if ((nbytes=read(fd, log_pwr_fft, fft_size*sizeof(log_pwr_fft[0])) < 0)) {
        perror("read");
        exit(1);
    }

    /* update plot */
    cairo_t        *cr;

    cr = gdk_cairo_create(gtk_widget_get_window(GTK_WIDGET(darea)));
    draw_fft(cr);
    if (freq_changed)
    {
        //draw_text(cr);
        //freq_changed = FALSE;
    }
    cairo_destroy(cr);

    return TRUE;
}

int main(int argc, char *argv[])
{
    /* GtkWidget is the storage type for widgets */
    GtkWidget      *window;
    guint           tid;
    int             opt;
    int             i;

    gtk_init(&argc, &argv);
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_add_events(window, GDK_KEY_PRESS_MASK);

    g_signal_connect(window, "delete-event", G_CALLBACK(delete_event), NULL);
    g_signal_connect(window, "destroy", G_CALLBACK(destroy), NULL);
    g_signal_connect(window, "key_press_event", G_CALLBACK(keypress_cb), NULL);

    /* parse cmd line */
    while ((opt = getopt(argc, argv, "h")) != -1)
    {
        switch (opt)
        {
        case 'h':
        case '?':
        default:
            printf("usage: rtlizer [WIDTHxHEIGHT+XOFF+YOFF] <HOST>\n");
            exit(EXIT_SUCCESS);
            break;
        }
    }
    
    if (optind >= argc)
    {
        printf("usage: rtlizer <HOST> [WIDTHxHEIGHT+XOFF+YOFF]\n");
        exit(EXIT_SUCCESS);
    }
    char* server = argv[optind];
    optind++;

    /* default window size if no geometry is specified */
    width = 640;                //gdk_screen_width();
    height = 480;               //gdk_screen_height();
    if (argc > optind)
    {
        if (!gtk_window_parse_geometry(GTK_WINDOW(window), argv[optind]))
            fprintf(stderr, "Failed to parse '%s'\n", argv[optind]);
        else
            gtk_window_get_default_size(GTK_WINDOW(window), &width, &height);
    }

    gtk_window_set_default_size(GTK_WINDOW(window), width, height);
    scale = (float)height / DYNAMIC_RANGE * SCREEN_FRAC;
    yzero = (int)(height * (1.0f - SCREEN_FRAC));
    text_margin = yzero / 10;

    g_print("window size: %dx%d pixels\n", width, height);
    g_print("SCALE: %.2f / Y0: %d / TXTMARG: %d\n", scale, yzero, text_margin);

    gtk_widget_show(window);
    gdk_window_set_cursor(gtk_widget_get_window(window),
                          gdk_cursor_new(GDK_BLANK_CURSOR));
                          
    fft_size = 2 * width / 2;
    log_pwr_fft = malloc(width * sizeof(float));
    for (i = 0; i < width; i++)
        log_pwr_fft[i] = -70.f;
        
    tcp_connect(server);

    tid = g_timeout_add(40, timeout_cb, window);

    gtk_main();

    g_source_remove(tid);

    return 0;
}
