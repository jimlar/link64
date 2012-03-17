/*
 * link64_gui_cb.c - here are all the code executed by ui events
 *
 */

#include <stdio.h>
#include <string.h>
#include "forms.h"
#include "link64_gui.h"

/* callbacks for form Link64 */

/* 
 * The root directory of the server has been altered
 * set the dir to the new one                        
 */

void set_rootdir(FL_OBJECT *ob, long data)
{

}

/*
 * The Start/Stop has been pushed
 *
 */

void startstop_button(FL_OBJECT *ob, long data)
{
    if (strcmp ("Stop Server", ob->label) == 0)
    {
	ob->label = "Start Server";
	fl_redraw_object (ob);
	/* insert my code to stop the sever here */
    }
    else
    {
	ob->label = "Stop Server";
	fl_redraw_object (ob);
	/* insert my code to start the server here */
    }
}

/*
 * A cable select has happened, set to new
 *
 */

void cable_select(FL_OBJECT *ob, long data)
{
  /* fill-in code for callback */
}



