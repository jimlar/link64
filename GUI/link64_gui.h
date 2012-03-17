#ifndef FD_Link64_h_
#define FD_Link64_h_
/* Header file generated with fdesign. */

/**** Callback routines ****/

extern void set_rootdir(FL_OBJECT *, long);
extern void startstop_button(FL_OBJECT *, long);
extern void cable_select(FL_OBJECT *, long);


/**** Forms and Objects ****/

typedef struct {
	FL_FORM *Link64;
	void *vdata;
	long ldata;
	FL_OBJECT *root_dir;
	FL_OBJECT *stop_button;
	FL_OBJECT *cable_group;
	FL_OBJECT *x1541;
	FL_OBJECT *trans64;
} FD_Link64;

extern FD_Link64 * create_form_Link64(void);

#endif /* FD_Link64_h_ */
