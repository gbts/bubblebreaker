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

typedef enum {
  BLUE,
  GREEN,
  RED,
  YELLOW,
  MAGENTA,
  EMPTY,
  POPPED
} Bubblecolor;

#define NUMBER_OF_COLORS (5)

const int board_width = 20;
const int board_height = 20;
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
        case RED:
          cairo_set_source_rgb (cr, 1, 0, 0);
          break;
        case MAGENTA:
          cairo_set_source_rgb (cr, 1, 0, 1);
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
      cairo_arc (cr, 0, 0,
          bubble_width/2.5,
          0, 2 * G_PI);

      cairo_fill (cr);

draw_nothing:
      cairo_restore(cr);
    }

  }

  return FALSE;
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

  pop(x,y, TRUE);

  pack_board();

  gtk_widget_queue_draw(window);

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

gint pop(int x, int y, gboolean check_lonely)
{
  if (board[x][y] == EMPTY)
    return 0;

  Bubblecolor val = board[x][y];

  gboolean left_is_equal = ( x>0 && val==board[x-1][y] );
  gboolean right_is_equal = ( x<(board_width-1) && val==board[x+1][y] );
  gboolean down_is_equal = ( y<(board_height-1) && val==board[x][y+1] );
  gboolean up_is_equal = ( y>0 && val==board[x][y-1] );

  /* lonely bubble */
  if ( check_lonely && ! (left_is_equal||right_is_equal||down_is_equal||up_is_equal) )
    return 0;

  DPRINTF("Popping %d %d", x, y);

  int popped = 0;
  /* temporary status to avoid back recursion */
  board[x][y] = POPPED;

  if (left_is_equal)
    popped += pop(x-1, y, FALSE);
  if (right_is_equal)
    popped += pop(x+1, y, FALSE);
  if (down_is_equal)
    popped += pop(x, y+1, FALSE);
  if (up_is_equal)
    popped += pop(x, y-1, FALSE);

  board[x][y] = EMPTY;
  return popped+1;
}

gint pack_rows(int y)
{
  gint floated = 0;
  for (int r=0; r<board_height; r++)
  {
    if (y>0 && board[r][y]==EMPTY && board[r][y-1]!=EMPTY)
      floated += float_empty_to_top(r, y);
  }

  return floated;
}

gint pack_columns()
{
  for (int x=0; x<board_width; x++) {
    if (is_empty_column(x))
      float_empty_col_to_left(x);
  }
}

gboolean is_empty_column(gint x)
{
  for (int y=0; y<board_height; y++) {
    if (board[x][y]!=EMPTY)
      return FALSE;
  }

  return TRUE;
}

gint float_empty_col_to_left(gint x)
{
  if (x>0 && is_empty_column(x)) {
    for (int y=0; y<board_height; y++) {
      board[x][y] = board[x-1][y];
      board[x-1][y] = EMPTY;
    }
    float_empty_col_to_left(x-1);
  }
}

gint float_empty_to_top(gint x, gint y)
{
  if (y>0 && board[x][y-1]!=EMPTY) {
    board[x][y] = board[x][y-1];
    board[x][y-1] = EMPTY;
    DPRINTF("Swapped %d,%d with %d,%d", x, y, x, y-1);
    return 1 + float_empty_to_top(x, y-1);
  }

  return 0;
}

gint pack_board()
{
  gint floated = 0;
  for (int c=1; c<board_width; c++)
    floated += pack_rows(c);
  pack_columns();
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

  pop(x,y, TRUE);

  pack_board();

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
        case MAGENTA:
          putchar('M');
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
}

int main (int argc, char *argv[])
{
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
  GError *error = NULL;

  g_io_add_watch(stdin_channel, (G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_PRI), input_callback, NULL);


  gtk_main();

  g_free(board);

  return 0;
}
