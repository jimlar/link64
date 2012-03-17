#
# Makefile for the Link64 project
#
# Initiated:
#        1997-05-16 by Jimmy Larsson
#

CC = gcc
CFLAGS = -g -O -I/usr/local/xforms/FORMS
OBJS = link64_gui.o link64_gui_cb.o link64_main.o


all:	link64

link64:	$(OBJS)
	$(CC) -lforms -L/usr/local/xforms/FORMS -lX11 -L/usr/X11R6/lib -lm -o link64 $(OBJS)

clean:	
	rm -f link64 *.o *~ \#*

# dependencies
link64_gui.o:		link64_gui.h
link64_gui_cb.o:	link64_gui.h
link64_main.o:		link64_gui.h		

