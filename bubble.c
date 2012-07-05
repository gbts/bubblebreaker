#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gprintf.h>

#define DEBUG

#ifdef DEBUG
#define DPRINTF(...) ({char __msg[100]; sprintf(__msg,__VA_ARGS__); puts(__msg);})
#else
#define DPRINTF(...)
#endif

#define NUMBER_OF_COLORS (5)
typedef enum {
  RED,
  YELLOW,
  GREEN,
  BLUE,
  EMPTY,
  BULLET,
} Bubblecolor;

typedef enum {
  RIGHT,
  LEFT,
  UP,
  DOWN,
  NONE,
} Popdirection;


const int board_width = 5;
const int board_height = 5;
Bubblecolor **board = NULL;

GtkWidget *window;

gboolean
draw_callback (GtkWidget *widget, cairo_t *cr, gpointer data)
{
  GdkRGBA color;

  guint width = gtk_widget_get_allocated_width (widget);
  guint height = gtk_widget_get_allocated_height (widget);

  double bubble_width = width / (double) board_width;
  double bubble_height = height / (double) board_height;

  for (int i=0; i<board_width; i++){

    double bubble_center_x = bubble_width*i + bubble_width/2.0;

    for (int j=0; j<board_height; j++){

      double bubble_center_y = bubble_height*j + bubble_height/2.0;

      cairo_save(cr);

      switch (board[i][j]) {
        case BLUE:
          cairo_set_source_rgb (cr, 0, 0, 1);
          break;
        case GREEN:
          cairo_set_source_rgb (cr, 0, 1, 0);
          break;

        case BULLET:
          cairo_set_source_rgb (cr, 0, 0, 0);
          break;

        case RED:
          cairo_set_source_rgb (cr, 1, 0, 0);
          break;

        case YELLOW:
          cairo_set_source_rgb (cr, 1, 1, 0);
          break;
        case EMPTY:
        default:
          goto draw_nothing;
          break;
      }

      cairo_translate (cr, bubble_center_x, bubble_center_y);
      cairo_scale (cr, 1, height/(double)width);

      if (board[i][j] == BULLET)
        cairo_arc (cr, 0, 0, bubble_width/5.0, 0, 2 * G_PI);
      else
        cairo_arc (cr, 0, 0, bubble_width/2.5, 0, 2 * G_PI);

      cairo_fill (cr);

draw_nothing:
      cairo_restore(cr);
    }

  }

  return FALSE;
}

gboolean POP_THREAD_RUNNING = FALSE;
void *pop_thread(void *posData)
{
  POP_THREAD_RUNNING = TRUE;
  DPRINTF("Entering POP thread");
  pop (((int*)posData)[0], ((int*)posData)[1], NONE);
  g_free(posData);
  POP_THREAD_RUNNING = FALSE;
  DPRINTF("Exiting POP thread");
}

gboolean
mouse_callback (GtkWidget *widget, GdkEventButton* event, gpointer data)
{
  int mouse_x = event->x;
  int mouse_y = event->y;

  guint width = gtk_widget_get_allocated_width (widget);
  guint height = gtk_widget_get_allocated_height (widget);

  double bubble_width = width / (double) board_width;
  double bubble_height = height / (double) board_height;

  int x = (int) mouse_x / bubble_width;
  int y = (int) mouse_y / bubble_height;

  GError* err = NULL;
  void* posData = malloc(sizeof(int)*2);
  ((int*)posData)[0] = x;
  ((int*)posData)[1] = y;

  g_thread_create (pop_thread, posData, FALSE, &err);

  return TRUE;
}

void fill_with_random() {
  for (int i=0; i<board_width; i++)
    for (int j=0; j<board_height; j++)
      board[i][j] = rand() % NUMBER_OF_COLORS;
}

void bubble_init()
{
  board = (Bubblecolor**) g_malloc(board_width * sizeof(Bubblecolor*));
  Bubblecolor* blob = (Bubblecolor*) g_malloc(board_height * board_width * sizeof(Bubblecolor));

  /* point to first element of each column */
  for (int col=0; col<board_width; col++) {
    board[col] = (Bubblecolor*) (blob + col*board_height);
  }
  fill_with_random();
}

gboolean refresh_thread(void* pData) {
  //DPRINTF("Redrawing");
    gtk_widget_queue_draw(window);
  return TRUE;
}

gint pop(int x, int y, Popdirection direction)
{
  int popped = 0;

  if ( x<0 || y<0 || y>(board_width-1) || x>(board_width-1) )
    return 0;

  Bubblecolor val = board[x][y];

  if (val == EMPTY) {
    board[x][y] = BULLET;
    usleep(1e5);
    board[x][y] = EMPTY;

    switch (direction) {
      case LEFT:
        popped += pop (x-1, y, direction);
        break;
      case RIGHT:
        popped += pop (x+1, y, direction);
        break;
      case UP:
        popped += pop (x, y-1, direction);
        break;
      case DOWN:
        popped += pop (x, y+1, direction);
        break;
      default:
        break;
      /* user attempted to pop an empty square */
    }
    return popped;
  }

  switch (val) {
    case RED:
      DPRINTF("Popping %d %d", x, y);
      popped++;
      board[x][y] = EMPTY;
      popped += pop (x-1, y, LEFT);
      popped += pop (x+1, y, RIGHT);
      popped += pop (x, y-1, UP);
      popped += pop (x, y+1, DOWN);
      break;
    case YELLOW:
    case GREEN:
    case BLUE:
      if (direction !=NONE)
        board[x][y]--;
      break;
  }

  return popped;
}

gboolean input_callback (GIOChannel *iochannel, GIOCondition condition, gpointer data) { 
  GError* err = NULL;
  gchar *msg, *piece;
  gsize len;

  if (condition & G_IO_HUP)
    g_error ("IO_HUP!\n");

  int ret = g_io_channel_read_line (iochannel, &msg, &len, NULL, &err);

  piece = strtok(msg, " ,");
  if (piece == NULL) {
    DPRINTF("Illegal input; screw you\n");
    goto input_fail;
  }

  int x = atoi(piece);

  piece = strtok(NULL, " ,");
  if (piece == NULL) {
    DPRINTF("Illegal input; screw you\n");
    goto input_fail;
  }

  int y = atoi(piece);

  if (x<0 || x>board_width-1 || y<0 || y>board_height-1) {
    DPRINTF("Illegal input; screw you\n");
    goto input_fail;
  }

  pop(x, y, NONE);

  print_board();

  gtk_widget_queue_draw(window);

input_fail:

  g_free(msg);

  return TRUE;
}

void print_board()
{
  for (int i=0; i<board_width; i++) {
    for (int j=0; j<board_height; j++) {
      putchar('"');
      switch(board[j][i]) {
        case BLUE:
          putchar('B');
          break;
        case YELLOW:
          putchar('Y');
          break;
        case RED:
          putchar('R');
          break;
        case GREEN:
          putchar('G');
          break;
        case EMPTY:
          putchar('_');
          break;
      }
      putchar('"'); putchar(',');
    }
    putchar('\n');
  }
  return;
}

gboolean REFRESH_THREAD_ON = TRUE;

int main (int argc, char *argv[])
{

  gdk_threads_init ();
  gdk_threads_enter ();

  gtk_init(&argc, &argv);
  bubble_init();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  if (! (argc>1 && strcmp(argv[1], "--no-gui")==0) )
    gtk_widget_show (window);

  gtk_widget_set_size_request (window, 400, 400);

  GtkWidget *drawing_area = gtk_drawing_area_new ();

  g_signal_connect (G_OBJECT (drawing_area), "draw",
      G_CALLBACK (draw_callback), NULL);

  gtk_widget_add_events (drawing_area, GDK_BUTTON_PRESS_MASK);
  g_signal_connect (G_OBJECT (drawing_area), "button-press-event",
      G_CALLBACK (mouse_callback), NULL);

  gtk_container_add(GTK_CONTAINER (window), drawing_area);
  gtk_widget_show(drawing_area);

  GIOChannel* stdin_channel = g_io_channel_unix_new(fileno(stdin));


    gdk_threads_add_timeout(100, refresh_thread, NULL);

  g_io_add_watch(stdin_channel, (G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_PRI), input_callback, NULL);

  GError *error = NULL;


  gtk_main();
  gdk_threads_leave ();

  g_free(board);
  REFRESH_THREAD_ON = FALSE;

  return 0;
}
