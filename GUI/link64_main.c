#include "forms.h"
#include "link64_gui.h"

int main(int argc, char *argv[])
{
   FD_Link64 *fd_Link64;

   fl_initialize(&argc, argv, 0, 0, 0);
   fd_Link64 = create_form_Link64();

   /* initialization code */

   /* show the first form */
   fl_show_form(fd_Link64->Link64,FL_PLACE_CENTER,FL_FULLBORDER,"Link64 BETA - (p) Jimmy Larsson 1997");
   fl_do_forms();
   return 0;
}
