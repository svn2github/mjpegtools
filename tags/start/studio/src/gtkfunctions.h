/* functions to make life easier */

#ifndef __GTK_FUNCTIONS_H__
#define __GTK_FUNCTIONS_H__

/* types */
#define STUDIO_WARNING 0
#define STUDIO_ERROR 1
#define STUDIO_INFO 2

void gtk_show_text_window(int type, char *message, char *message2);
GtkWidget *gtk_widget_from_xpm_data(gchar **data);

#endif /* __GTK_FUNCTIONS_H__ */
