/* gpawidgets.h  -  The GNU Privacy Assistant
 *	Copyright (C) 2000, 2001 G-N-U GmbH.
 *
 * This file is part of GPA
 *
 * GPA is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GPA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#ifndef GPAWIDGETS_H
#define GPAWIDGETS_H

#include <gtk/gtk.h>

extern GtkWidget *gpa_key_info_new (GpapaKey *key, GtkWidget *window);

extern GtkWidget *gpa_secret_key_list_new (GtkWidget *window);
extern GtkWidget *gpa_public_key_list_new (GtkWidget *window);
extern GtkWidget *gpa_key_list_new_from_glist (GtkWidget *window,
                                                      GList *list);
extern gint gpa_key_list_selection_length (GtkWidget *clist);
extern GList *gpa_key_list_selected_ids (GtkWidget *clist);
extern gchar *gpa_key_list_selected_id (GtkWidget *clist);

extern GtkWidget *gpa_expiry_frame_new (GtkAccelGroup *accelGroup,
				        GDate *expiryDate);
extern gchar *gpa_expiry_frame_validate (GtkWidget *expiry_frame);
extern gboolean gpa_expiry_frame_get_expiration(GtkWidget *expiry_frame,
					        GDate **date,
					        int *interval, gchar *unit);

#endif /* GPAWIDGETS_H */
