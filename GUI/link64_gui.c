/* Form definition file generated with fdesign. */

#include "forms.h"
#include <stdlib.h>
#include "link64_gui.h"

FD_Link64 *create_form_Link64(void)
{
  FL_OBJECT *obj;
  FD_Link64 *fdui = (FD_Link64 *) fl_calloc(1, sizeof(*fdui));

  fdui->Link64 = fl_bgn_form(FL_NO_BOX, 340, 140);
  obj = fl_add_box(FL_UP_BOX,0,0,340,140,"");
  obj = fl_add_text(FL_NORMAL_TEXT,10,10,180,20,"Link64 0.1.0 Beta");
    fl_set_object_lcolor(obj,FL_RED);
    fl_set_object_lsize(obj,FL_MEDIUM_SIZE);
    fl_set_object_lalign(obj,FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
    fl_set_object_lstyle(obj,FL_BOLD_STYLE+FL_SHADOW_STYLE);
  fdui->root_dir = obj = fl_add_input(FL_NORMAL_INPUT,90,90,230,20,"Server root:");
    fl_set_object_callback(obj,set_rootdir,0);
  fdui->stop_button = obj = fl_add_button(FL_NORMAL_BUTTON,240,20,80,30,"Start Server");
    fl_set_object_callback(obj,startstop_button,0);

  fdui->cable_group = fl_bgn_group();
  fdui->x1541 = obj = fl_add_checkbutton(FL_RADIO_BUTTON,20,40,100,20,"X1541 Cable");
    fl_set_object_color(obj,FL_RED,FL_GREEN);
    fl_set_object_callback(obj,cable_select,0);
    fl_set_button(obj, 1);
  fdui->trans64 = obj = fl_add_checkbutton(FL_RADIO_BUTTON,20,60,100,20,"Trans64 Cable");
    fl_set_object_color(obj,FL_RED,FL_GREEN);
    fl_set_object_callback(obj,cable_select,1);
  fl_end_group();

  fl_end_form();

  fdui->Link64->fdui = fdui;

  return fdui;
}
/*---------------------------------------*/

