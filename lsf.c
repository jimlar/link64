/*
 * Link64 IEC-bus lowlevel driver for Linux
 *
 * Contains timing and basic communication 
 * for the Link64 IEC bus emulator
 *
 * Compile with: "gcc -O -DMODULE -D__KERNEL__ -c lsf.c"
 *           or: "gcc -O -DMODULE -D__KERNEL__ -DDEBUG -c lsf.c
 *
 * (C) 1997 Jimmy Larsson
 *
 *
 */

/* major of the device */
#define LSF_MAJOR 127

/* The paralell port address and irq */
#define LSF_BASEADDR 0x3BC
#define LSF_IRQ      0x7

/* Cable bit settings (because of the interrupt, we only support one type) */
/* All signals are active low (inverted) */
#define LSF_CABLECLK   2
#define LSF_CABLEDATA  8
#define LSF_CABLEATN   64

/* Timings, all in microseconds, taken from */
/* Commodores programming manual            */
#define LSF_TIME_TF  20
#define LSF_TIME_TH  50
#define LSF_TIME_TRY 30
#define LSF_TIME_TNE 40
#define LSF_TIME_TNE_MIN 40
#define LSF_TIME_TNE_MAX 200
#define LSF_TIME_TEI 80
#define LSF_TIME_TBB 100
#define LSF_TIME_TTK 20
#define LSF_TIME_TR  20
#define LSF_TIME_TDA 80
#define LSF_TIME_TS 70
#define LSF_TIME_TV 70

/* Misc, non protocol, timings */
#define LSF_RESET_TIME 4000

/* std timeout (in jiffies) */
#define LSF_STD_TIMEOUT 10

/* error codes */
#define LSF_WAITCLKERR 1
#define LSF_WAITTLKERR 2
#define LSF_WAITLSTERR 3
#define LSF_WAITEOIRESPERR 4

/* Flag values for the byte-io routines */
#define LSF_FLAG_EOI       1
#define LSF_FLAG_SUPPORTED 2

/* Protocol flags */
#define LSF_PROT_LISTEN    0x20
#define LSF_PROT_UNLISTEN  0x3f
#define LSF_PROT_TALK      0x40
#define LSF_PROT_UNTALK    0x5f
#define LSF_PROT_OPENFILE  0xf0
#define LSF_PROT_CLOSEFILE 0xe0
#define LSF_PROT_SENDDATA  0x60

/* ioctl's, these should be in a header file later... */
#define LSFEN     _IO('z', 0)  /* enable Link64 */
#define LSFDIS    _IO('z', 1)  /* disable Link64 */
#define LSFDEVEN  _IO('z', 2)  /* enable monitoring of a device (8-11)  */
#define LSFDEVDIS _IO('z', 3)  /* disable monitoring of a device (8-11) */


#include <linux/config.h>
#include <linux/ioctl.h>

#ifdef MODULE
#include <linux/module.h>
#include <linux/version.h>
#else
#define MOD_INC_USE_COUNT
#define MOD_DEC_USE_COUNT
#endif

#include <linux/sched.h>
#include <linux/major.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/malloc.h>

#include <asm/segment.h>
#include <asm/io.h>


/*
 * Global variables (sorry...)
 *
 */

/* error indicator */
static int lsf_errno = 0;

/* timer_list pointer */
static struct timer_list *lsf_timer_list = NULL;

/* store current state of bus-bits here to save reads */
static int lsf_busstate;

/* lock variable */
static int lsf_lock = 0;

/* confiuration */
static struct lsf_config_struct 
{
    /* devices 0-15, supported flag */
    int   devs[16];
    int   cur_dev;
    int   cur_channel;
    int   request_waiting;
    int   debug;
} lsf_config;
	

/* 
 * Bit setting functions, saves state of the bus-bits
 *
 */

static void lsf_databit (int bit)
{
    if (!bit)
	lsf_busstate |= LSF_CABLEDATA;
    else
	lsf_busstate &= ~LSF_CABLEDATA;

    outb (lsf_busstate, LSF_BASEADDR + 2);
}

static void lsf_atnbit (int bit)
{
/*    if (!bit)
 *	lsf_busstate |= LSF_CABLEATN;
 *   else
 *	lsf_busstate &= ~LSF_CABLEATN;
 * 
 *   outb (lsf_busstate, LSF_BASEADDR + 1);
 */
}	

static void lsf_clkbit (int bit)
{
    if (!bit)
	lsf_busstate |= LSF_CABLECLK;
    else
	lsf_busstate &= ~LSF_CABLECLK;

    outb (lsf_busstate, LSF_BASEADDR + 2);
}	

/*
 * lsf_reset_bus
 * Resets serial bus after some error
 */
static void lsf_reset_bus (void)
{
    int i;

    lsf_databit (0);
    lsf_atnbit (1);
    lsf_clkbit (1);

    for (i = 1; i <= 18; i++)
    {
        udelay (LSF_RESET_TIME);
	lsf_clkbit (0);
	udelay (LSF_RESET_TIME);
	lsf_databit (1);
	udelay (LSF_RESET_TIME);
	lsf_clkbit (1);
    }

    lsf_databit (0);
    lsf_atnbit (1);
    lsf_clkbit (0);
}


/*
 * Timeout handling code 
 *
 */

/* remove an error timer */
static void lsf_kill_errortimer (void)
{
    del_timer (lsf_timer_list);
    kfree (lsf_timer_list);
}

/* timeout handling code */
static void lsf_errorhandler (unsigned long error_no)
{
    lsf_kill_errortimer ();
/*  lsf_reset_bus ();      */

    lsf_errno = error_no;
}

/* setup an error timer */
static void lsf_set_errortimer (unsigned long jiffies, int error_no)
{
    /* allocate memory atomic, since this will be called from interrupts */ 
    lsf_timer_list = kmalloc (sizeof(struct timer_list), GFP_ATOMIC);
    init_timer (lsf_timer_list);

    lsf_timer_list->expires = jiffies;
    lsf_timer_list->data = error_no;
    lsf_timer_list->function = lsf_errorhandler;

    add_timer (lsf_timer_list);
    lsf_errno = 0;
}


/*
 * lsf_getbits
 * Get Databits from bus.
 * This is the same for all types of input bytes.
 *
 */

static int lsf_getbits (int ATN_byte)
{
    int             i;
    int             data_byte = 0;
    int             temp_byte = 0;
    unsigned char   clk_or_data = LSF_CABLECLK | LSF_CABLEDATA;

    for (i = 0; i <= 7; i++)
    {    
        
        /* setup timeout */
        lsf_set_errortimer (LSF_STD_TIMEOUT, LSF_WAITCLKERR);
	do
	{
	    /* wait for clock to go high */ 
	    temp_byte = inb (LSF_BASEADDR + 2) & clk_or_data;
	} while ((temp_byte & LSF_CABLECLK) != 0 && lsf_errno == 0);

	/* remove timeout */
	lsf_kill_errortimer ();
       
	/* TAKE CARE OF ERRORS HERE */

	/* Read the bit */
	if ((temp_byte & LSF_CABLEDATA) == 0)
	{
       	    data_byte = data_byte | (1 << i);
	}

        
        /* setup timeout */
        lsf_set_errortimer (LSF_STD_TIMEOUT, LSF_WAITCLKERR);
	do
	{
	    /* Wait for clock to go low */
	} while (((inb (LSF_BASEADDR + 2) & LSF_CABLECLK) == 0) && lsf_errno == 0);
	lsf_kill_errortimer ();

	/* TAKE CARE OF ERRORS HERE */
    }
	
    if (!ATN_byte)
    {
	udelay (LSF_TIME_TF);
	lsf_databit (0);
    }

    return data_byte;
}


    
/*
 * lsf_getbyte
 * Receives byte from the c64.
 * If it's an ATN byte, respond to it if we support the device.
 *
 */

static int lsf_getbyte (int *flags, int ATN_flag)
{
    int   data_byte = 0;
    int   temp_byte = 0;
    int   retry_cnt = 0;

    /* set default values */
    *flags = 0;
    
    /* this wait is really strange... =)  */
    udelay (50);
    lsf_clkbit (1);
    lsf_databit (0);

    /* setup timeout */
    lsf_set_errortimer (LSF_STD_TIMEOUT, LSF_WAITCLKERR);
    do
    {
	/* Wait for clock to rise */	
	temp_byte = inb (LSF_BASEADDR + 2) & LSF_CABLECLK;
    } while (temp_byte != 0 && lsf_errno == 0);
    lsf_kill_errortimer ();

    /* TAKE CARE OF ERRORS HERE */

    udelay (LSF_TIME_TH);
    lsf_databit (1);
    
    udelay (LSF_TIME_TNE_MIN);

    do
    {
	/* Wait for clock to go low, if longer than the TNE time    */
	/* we have an EOI byte coming (last byte)                   */
	/* I really should use timers here, but what the hell... =) */
	
	udelay (1);
	retry_cnt++;
	temp_byte = inb (LSF_BASEADDR + 2) & LSF_CABLECLK;
    } while ((temp_byte == 0) && (retry_cnt <= LSF_TIME_TNE_MAX));

    if (!temp_byte)
	data_byte = lsf_getbits (ATN_flag);
    else
    {
	/* We have an EOI-byte coming */
	*flags = LSF_FLAG_EOI;  /* set the EOI-flag */
	lsf_databit (0);
	lsf_clkbit (1);
	udelay (LSF_TIME_TEI);
	
	lsf_databit (1);

	/* setup timeout */
        lsf_set_errortimer (LSF_STD_TIMEOUT, LSF_WAITTLKERR);
	do
	{
	    /* Wait for talker ready to send */

	    temp_byte = inb (LSF_BASEADDR + 2) & LSF_CABLECLK;

	    /* Yvo uses this too, I can't see why it's needed */
	    lsf_databit (1);
	    lsf_clkbit (1);

	} while (temp_byte == 0 && lsf_errno == 0);
	lsf_kill_errortimer ();

	/* TAKE CARE OF ERRORS HERE */

	data_byte = lsf_getbits (ATN_flag);
    }

    if (ATN_flag)
    {
	/* This is a ATN byte, check if we support the device */
	/* and respond if we do...                            */

	if (lsf_config.devs[data_byte & 0x0F])
	{
	    /* It's supported, respond */
	    udelay (LSF_TIME_TF);
	    lsf_databit (0);
	    lsf_clkbit (0);
	    udelay (LSF_TIME_TR);
	    lsf_atnbit (1);
	    *flags = *flags | LSF_FLAG_SUPPORTED;
	}
    }
    else
	udelay (LSF_TIME_TBB);

    return data_byte;
}

/*
 * lsf_putbyte
 * Send a byte to the c64, with or without EOI
 *
 */

static void lsf_putbyte (int data_byte, int eoi)
{
    int   temp_byte;
    int   i;

    udelay (LSF_TIME_TBB);
    
    lsf_databit (1);
    if ((inb (LSF_BASEADDR + 2) & LSF_CABLEDATA) == 0)
    {
	/* Error: The dataline is stuck! */
    }

    lsf_clkbit (1);

    /* setup timeout */
    lsf_set_errortimer (LSF_STD_TIMEOUT, LSF_WAITLSTERR);
    do
    {
	/* Wait for respond from c64 on dataline */
	temp_byte = inb (LSF_BASEADDR + 2) & LSF_CABLEDATA;

    } while (temp_byte != 0 && lsf_errno == 0);
    lsf_kill_errortimer ();

    /* TAKE CARE OF ERRORS HERE */

    if (eoi)
    {
	/* We're going EOI */

        /* setup timeout */
        lsf_set_errortimer (LSF_STD_TIMEOUT, LSF_WAITEOIRESPERR);
	do
	{
	    /* Wait for c64 to respond to the EOI byte */
       	    temp_byte = inb (LSF_BASEADDR + 2) & LSF_CABLEDATA;
	} while (temp_byte == 0 && lsf_errno == 0);
	lsf_kill_errortimer ();

	/* TAKE CARE OF ERRORS HERE */

	udelay (LSF_TIME_TRY);

	lsf_databit (1);
    }
    else
	udelay (LSF_TIME_TNE);

    lsf_clkbit (1);

    for (i = 0; i <= 7; i++)
    {
	/* setup bit */
	lsf_databit ((data_byte >> i) & 0x01);
	/* wait setup time */
	udelay (LSF_TIME_TS);
	lsf_clkbit (1);
	/* c64 reads bit */

	/* wait valid time */
	udelay (LSF_TIME_TV);
	lsf_clkbit (0);
    }

    /* setup timeout */
    lsf_set_errortimer (LSF_STD_TIMEOUT, LSF_WAITCLKERR);
    do
    {
	/* Wait for data to go low */
	temp_byte = inb (LSF_BASEADDR + 2) & LSF_CABLEDATA;
    } while (temp_byte == 0 && lsf_errno == 0);
    lsf_kill_errortimer ();

    /* TAKE CARE OF ERRORS HERE */
}


/*
 * The interrupt code
 * Monitor the ATN signal and recieve the request from the c64
 *
 */

static void lsf_interrupt (int irq, void *dev_id, struct pt_regs *regs)
{
    int             flags = 0;
    int             temp_byte = 0;
    int             data_byte = 0; 

    /* We need the machine, don't know if this is needed here */
    /*cli ();*/

    /* We now have a ATN signaled from the c64, read a byte
     * and see if it's a device # that we shall take care of
     */
    
    data_byte = lsf_getbyte (&flags, 1);

    if (flags & LSF_FLAG_SUPPORTED)
    {
	lsf_config.cur_dev = data_byte;
	
	switch (data_byte & 0xf0)
	{
	    case LSF_PROT_LISTEN:
		/* c64 wants us to listen, get channel number */	
		/* pr_debug ("LSF Listen, device %d\n", lsf_config.cur_dev & 0x0f); */
		
		data_byte = lsf_getbyte (&flags, 0);

		/* Should check for errors here */

		lsf_config.cur_channel = data_byte;
		lsf_config.request_waiting = 1;
		break;

	    case LSF_PROT_TALK:
		/* c64 wants us to talk, get channel number and reverse dataflow */
		/* pr_debug ("LSF Talk, device %d\n", lsf_config.cur_dev & 0x0f); */

		data_byte = lsf_getbyte (&flags, 0);
		
		/* Should have error checking here */

		lsf_config.cur_channel = data_byte;
		lsf_config.request_waiting = 1;
		
		/* Ok, so far so good, let's reverse the dataflow */
		udelay (LSF_TIME_TTK);

		/* setup timeout */
		lsf_set_errortimer (LSF_STD_TIMEOUT, LSF_WAITCLKERR);
		do
		{
		    /* Wait for clockbit to rise */
		    temp_byte = inb (LSF_BASEADDR + 2) & LSF_CABLECLK;
		} while (temp_byte != 0 && lsf_errno == 0);
		lsf_kill_errortimer ();

		/* TAKE CARE OF ERRORS HERE */

		lsf_clkbit (0);
		udelay (LSF_TIME_TDA);

		/* We should have this here too, but i'm not sure it's needed */
		/* lsf_clkbit (1); */
		
		break;
	}
    }

    /* Enable interrupts again */
    /*sti ();*/

}

/*
 * lsf_read
 * Reads from the IEC-interface
 */

static int lsf_read (struct inode   *node, 
		     struct file    *file, 
		     char           *buf, 
		     int             count)
{
  /*    pr_debug ("LSF read...\n"); */
    return count;
}


/*
 * lsf_write
 * Writes to the IEC-interface
 */

static int lsf_write (struct inode   *node, 
		      struct file    *file, 
		      const char     *buf, 
		      int             count)
{
  /*  pr_debug ("LSF write...\n"); */
    pr_debug ("LSF state: req wait %0x,debug %0x, cur dev %0x, errno %0x\n", lsf_config.request_waiting, lsf_config.debug, lsf_config.cur_dev, lsf_errno);

    return count;
}


/*
 * lsf_ioctl
 * The ioctl, select wich IEC device number to listen to
 * etc.
 */

static int lsf_ioctl (struct inode    *node, 
		      struct file     *file,
		      unsigned int     cmd, 
		      unsigned long    arg)
{
	int            retval = 0;

	/*	pr_debug ("LSF ioctl, cmd: 0x%x, arg: 0x%x\n", cmd, arg); */

	switch (cmd) 
	{
	    case LSFDEVEN:
		/* enable monitoring of the IEC dev. with # "arg" */ 
		break;
	    case LSFDEVDIS:
		/* disable monitoring of the IEC dev. with # "arg" */
		break;
	    default:
		retval = -EINVAL;
	}
	return retval;
}

/*
 * open_lsf
 * Opens and locks the IEC-interface
 */

static int lsf_open (struct inode   *node,
		     struct file    *file)
{
    if (lsf_lock)
    {
              pr_debug ("!error opening LSF, busy\n");
      
	return -EBUSY;
    }

    lsf_lock = 1;
    MOD_INC_USE_COUNT;
    /*  pr_debug ("LSF opened and locked\n"); */

    return 0;
}

/* 
 * lsf_release
 * Closes and unlocks the IEC-interface
 */

static void lsf_release (struct inode   *node,
		         struct file    *file)
{
    lsf_lock = 0;
    MOD_DEC_USE_COUNT;

    /*    pr_debug ("LSF released and unlocked\n"); */

}

/*
 * The file_operations structure
 */

static struct file_operations lsf_fops = {
    NULL,        /* lseek              */
    lsf_read,    /* read               */
    lsf_write,   /* write              */
    NULL,        /* readdir            */
    NULL,        /* select             */
    lsf_ioctl,   /* ioctl              */
    NULL,        /* mmap               */
    lsf_open,    /* open               */
    lsf_release, /* release device     */
    NULL,        /* fsync              */
    NULL,        /* fasync             */
    NULL,        /* check_media_change */
    NULL         /* revalidate         */
};


/*
 * lsf_init_general
 * The general inititalization, for both module and normal code
 *
 */

static void lsf_init_general (void)
{
    int   i;

    for (i = 0;i <= 15; i++)
#ifdef DEBUG
	lsf_config.devs[i] = 1;
#else
        lsf_config.devs[i] = 0;
#endif

    lsf_config.cur_dev = 0;
    lsf_config.cur_channel = 0;
    lsf_config.request_waiting = 0;

    lsf_config.debug = 0;

    lsf_busstate = 0xd4;
    lsf_errno = 0;
}

/*
 * Initialization of the driver, one if we're a module
 * and a different one if we're not
 */

#ifndef MODULE
long lsf_init (long   mem_start, 
	       long   mem_end)
{
    if (register_chrdev (LSF_MAJOR, "lsf", &lsf_fops))
	pr_info ("LSF unable to get major for lsf (Link64) device, tried %d\n", LSF_MAJOR);
    else
    {
	lsf_init_general ();

	if (request_irq (LSF_IRQ, lsf_interrupt, SA_INTERRUPT, "lsf", NULL) != 0)
	{
	    pr_info ("LSF unable to allocate IRQ %d, unloading...\n", LSF_IRQ);
	    unregister_chrdev (LSF_MAJOR, "lsf");
	}
	else if (check_region (LSF_BASEADDR, 3) != 0)
	{
	    pr_info ("LSF unable to allocate ports %d to %d, unloading...\n", LSF_BASEADDR, LSF_BASEADDR + 2); 
	    unregister_chrdev (LSF_MAJOR, "lsf");
	    free_irq (LSF_IRQ);
	}
	else
	{ 
	    request_region (LSF_BASEADDR, 3, "lsf");
	    outb (lsf_busstate, LSF_BASEADDR + 2);

	    pr_info ("LSF 0.0.1 initialized with major %d, IRQ %d, (c) Jimmy Larsson, Aug 1997\n", LSF_MAJOR, LSF_IRQ);
	}
    }

    return mem_start;
}

#else
int init_module (void)
{
    if (register_chrdev (LSF_MAJOR, "lsf", &lsf_fops) == -EBUSY)
    {
	pr_info ("LSF unable ot get major for lsf (Link64) device, tried %d\n", LSF_MAJOR);
	return -EIO;
    }
    else
    {
	lsf_init_general ();

	if (check_region (LSF_BASEADDR, 3) != 0)
	{
	    pr_info ("LSF unable to allocate ports %d to %d, unloading...\n", LSF_BASEADDR, LSF_BASEADDR + 2); 
	    unregister_chrdev (LSF_MAJOR, "lsf");
	    return -EIO;
	} else 
	{
	    request_region (LSF_BASEADDR, 3, "lsf");
	    outb (lsf_busstate, LSF_BASEADDR + 2);
	    release_region (LSF_BASEADDR, 3);
	}

	if (request_irq (LSF_IRQ, lsf_interrupt, SA_INTERRUPT, "lsf", NULL) != 0)
	{
	    pr_info ("LSF unable to allocate IRQ %d, unloading...\n", LSF_IRQ);
	    unregister_chrdev (LSF_MAJOR, "lsf");
	    return -EIO;
	}
    }      

    pr_info ("LSF 0.0.1 initialized with major %d, IRQ %d, (c) Jimmy Larsson, Aug 1997\n", LSF_MAJOR, LSF_IRQ);

    return 0;
}

void cleanup_module (void)
{ 
    pr_info ("LSF 0.0.1 have been unloaded, (c) Jimmy Larsson, Aug 1997\n");
    release_region (LSF_BASEADDR, 3);
    free_irq (LSF_IRQ, NULL);
    unregister_chrdev (LSF_MAJOR, "lsf");
}
#endif




