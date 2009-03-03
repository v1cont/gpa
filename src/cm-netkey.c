/* cm-netkey.c  -  Widget to show information about a Netkey card.
 * Copyright (C) 2009 g10 Code GmbH
 *
 * This file is part of GPA.
 *
 * GPA is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * GPA is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>. 
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "gpa.h"   
#include "convert.h"

#include "cm-object.h"
#include "cm-netkey.h"




/* Identifiers for the entry fields.  */
enum
  {
    ENTRY_SERIALNO,

    ENTRY_LAST
  }; 



/* Object's class definition.  */
struct _GpaCMNetkeyClass 
{
  GpaCMObjectClass parent_class;
};


/* Object definition.  */
struct _GpaCMNetkey
{
  GpaCMObject  parent_instance;

  GtkWidget *general_frame;

  GtkWidget *entries[ENTRY_LAST];
};

/* The parent class.  */
static GObjectClass *parent_class;



/* Local prototypes */
static void gpa_cm_netkey_finalize (GObject *object);



/************************************************************ 
 *******************   Implementation   *********************
 ************************************************************/

/* Clears the info contained in the card widget. */
static void
clear_card_data (GpaCMNetkey *card)
{
  int idx;

  for (idx=0; idx < ENTRY_LAST; idx++)
    gtk_entry_set_text (GTK_ENTRY (card->entries[idx]), "");
}


struct scd_getattr_parm
{
  GpaCMNetkey *card;  /* The object.  */
  const char *name;     /* Name of expected attribute.  */
  int entry_id;        /* The identifier for the entry.  */
  void (*updfnc) (GpaCMNetkey *card, int entry_id, const char *string);
};


static gpg_error_t
scd_getattr_cb (void *opaque, const char *status, const char *args)
{
  struct scd_getattr_parm *parm = opaque;
  int entry_id;

/*   g_debug ("STATUS_CB: status=`%s'  args=`%s'", status, args); */

  if (!strcmp (status, parm->name) )
    {
      entry_id = parm->entry_id;

      if (entry_id < ENTRY_LAST)
        {
          if (parm->updfnc)
            parm->updfnc (parm->card, entry_id, args);
          else
            gtk_entry_set_text 
              (GTK_ENTRY (parm->card->entries[entry_id]), args);
        }
    }

  return 0;
}     


/* Data callback used by check_nullpin. */
static gpg_error_t
check_nullpin_data_cb (void *opaque, const void *data_arg, size_t datalen)
{
  const unsigned char *data = data_arg;

  if (datalen >= 2)
    {
      unsigned int sw = ((data[datalen-2] << 8) | data[datalen-1]);

      if (sw == 0x6985)
        g_debug ("NullPIN activ for PIN0");
      else if (sw == 0x6983)
        g_debug ("PIN0 is blocked");
      else if ((sw & 0xfff0) == 0x63C0)
        g_debug ("PIN0 has %d tries left", (sw & 0x000f));
      else
        g_debug ("status for global PIN0 is %04x", sw);
    }
  return 0;
}     


/* Check whether the NullPIN is still active.  */
static void
check_nullpin (GpaCMNetkey *card)
{
  gpg_error_t err;
  gpgme_ctx_t gpgagent;

  gpgagent = GPA_CM_OBJECT (card)->agent_ctx;
  g_return_if_fail (gpgagent);

  /* A TCOS card responds to a verify with empty data (i.e. without
     the Lc byte) with the status of the PIN.  The PIN is given as
     usual as P2. */
  err = gpgme_op_assuan_transact (gpgagent,
                                  "SCD APDU 00:20:00:00",
                                  check_nullpin_data_cb, card,
                                  NULL, NULL,
                                  NULL, NULL);
  if (!err)
    err = gpgme_op_assuan_result (gpgagent)->err;
  if (err)
    g_debug ("assuan dummy verify command failed: %s <%s>\n", 
             gpg_strerror (err), gpg_strsource (err));
}



/* Use the assuan machinery to load the bulk of the OpenPGP card data.  */
static void
reload_data (GpaCMNetkey *card)
{
  static struct {
    const char *name;
    int entry_id;
    void (*updfnc) (GpaCMNetkey *card, int entry_id,  const char *string);
  } attrtbl[] = {
    { "SERIALNO",    ENTRY_SERIALNO },
    { NULL }
  };
  int attridx;
  gpg_error_t err;
  char command[100];
  struct scd_getattr_parm parm;
  gpgme_ctx_t gpgagent;

  gpgagent = GPA_CM_OBJECT (card)->agent_ctx;
  g_return_if_fail (gpgagent);

  check_nullpin (card);

  parm.card = card;
  for (attridx=0; attrtbl[attridx].name; attridx++)
    {
      parm.name     = attrtbl[attridx].name;
      parm.entry_id = attrtbl[attridx].entry_id;
      parm.updfnc   = attrtbl[attridx].updfnc;
      snprintf (command, sizeof command, "SCD GETATTR %s", parm.name);
      err = gpgme_op_assuan_transact (gpgagent,
                                      command,
                                      NULL, NULL,
                                      NULL, NULL,
                                      scd_getattr_cb, &parm);
      if (!err)
        err = gpgme_op_assuan_result (gpgagent)->err;

      if (err)
        {
          if (gpg_err_code (err) == GPG_ERR_CARD_NOT_PRESENT)
            ; /* Lost the card.  */
          else
            {
              g_debug ("assuan command `%s' failed: %s <%s>\n", 
                       command, gpg_strerror (err), gpg_strsource (err));
            }
          clear_card_data (card);
          break;
        }
    }
}



/* Helper for construct_data_widget.  */
static GtkWidget *
add_table_row (GtkWidget *table, int *rowidx, const char *labelstr)
{
  GtkWidget *widget;
  GtkWidget *label;

  widget = gtk_entry_new ();

  label = gtk_label_new (labelstr);
  gtk_label_set_width_chars  (GTK_LABEL (label), 22);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1,	       
                    *rowidx, *rowidx + 1, GTK_FILL, GTK_SHRINK, 0, 0); 

  gtk_editable_set_editable (GTK_EDITABLE (widget), FALSE);
  gtk_entry_set_has_frame (GTK_ENTRY (widget), FALSE);

  gtk_table_attach (GTK_TABLE (table), widget, 1, 2,
                    *rowidx, *rowidx + 1, GTK_FILL, GTK_SHRINK, 0, 0);

  ++*rowidx;

  return widget;
}


/* This function constructs the container holding all widgets making
   up this data widget.  It is called during instance creation.  */
static void
construct_data_widget (GpaCMNetkey *card)
{
  GtkWidget *general_frame;
  GtkWidget *general_table;
  GtkWidget *label;
  int rowidx;

  /* Create frame and tables. */

  general_frame = card->general_frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (general_frame), GTK_SHADOW_NONE);
  label = gtk_label_new (_("<b>General</b>"));
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_frame_set_label_widget (GTK_FRAME (general_frame), label);

  general_table = gtk_table_new (1, 3, FALSE);

  gtk_container_set_border_width (GTK_CONTAINER (general_table), 10);
  
  /* General frame.  */
  rowidx = 0;

  card->entries[ENTRY_SERIALNO] = add_table_row 
    (general_table, &rowidx, _("Serial Number: "));



  gtk_container_add (GTK_CONTAINER (general_frame), general_table);

  /* Put all frames together.  */
  gtk_box_pack_start (GTK_BOX (card), general_frame, FALSE, TRUE, 0);
}



/************************************************************ 
 ******************   Object Management  ********************
 ************************************************************/

static void
gpa_cm_netkey_class_init (void *class_ptr, void *class_data)
{
  GpaCMNetkeyClass *klass = class_ptr;

  parent_class = g_type_class_peek_parent (klass);
  
  G_OBJECT_CLASS (klass)->finalize = gpa_cm_netkey_finalize;
}


static void
gpa_cm_netkey_init (GTypeInstance *instance, void *class_ptr)
{
  GpaCMNetkey *card = GPA_CM_NETKEY (instance);

  construct_data_widget (card);

}


static void
gpa_cm_netkey_finalize (GObject *object)
{  
/*   GpaCMNetkey *card = GPA_CM_NETKEY (object); */

  parent_class->finalize (object);
}


/* Construct the class.  */
GType
gpa_cm_netkey_get_type (void)
{
  static GType this_type = 0;
  
  if (!this_type)
    {
      static const GTypeInfo this_info =
	{
	  sizeof (GpaCMNetkeyClass),
	  (GBaseInitFunc) NULL,
	  (GBaseFinalizeFunc) NULL,
	  gpa_cm_netkey_class_init,
	  (GClassFinalizeFunc) NULL,
	  NULL, /* class_data */
	  sizeof (GpaCMNetkey),
	  0,    /* n_preallocs */
	  gpa_cm_netkey_init
	};
      
      this_type = g_type_register_static (GPA_CM_OBJECT_TYPE,
                                          "GpaCMNetkey",
                                          &this_info, 0);
    }
  
  return this_type;
}


/************************************************************ 
 **********************  Public API  ************************
 ************************************************************/
GtkWidget *
gpa_cm_netkey_new ()
{
  return GTK_WIDGET (g_object_new (GPA_CM_NETKEY_TYPE, NULL));  
}


/* If WIDGET is of Type GpaCMNetkey do a data reload through the
   assuan connection.  */
void
gpa_cm_netkey_reload (GtkWidget *widget, gpgme_ctx_t gpgagent)
{
  if (GPA_IS_CM_NETKEY (widget))
    {
      GPA_CM_OBJECT (widget)->agent_ctx = gpgagent;
      if (gpgagent)
        reload_data (GPA_CM_NETKEY (widget));
    }
}
