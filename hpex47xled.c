/////////////////////////////////////////////////////////////////////////////
///// @file hpex47xled.c
/////
///// Daemon for controlling the LEDs on the HP MediaSmart Server EX47X
///// FreeBSD Support - Version >= 12.3 Release
/////
///// -------------------------------------------------------------------------
/////
///// Copyright (c) 2022 Robert Schmaling
///// 
///// This software is provided 'as-is', without any express or implied
///// warranty. In no event will the authors be held liable for any damages
///// arising from the use of this software.
///// 
///// Permission is granted to anyone to use this software for any purpose,
///// including commercial applications, and to alter it and redistribute it
///// freely, subject to the following restrictions:
///// 
///// 1. The origin of this software must not be misrepresented; you must not
///// claim that you wrote the original software. If you use this software
///// in a product, an acknowledgment in the product documentation would be
///// appreciated but is not required.
///// 
///// 2. Altered source versions must be plainly marked as such, and must not
///// be misrepresented as being the original software.
///// 
///// 3. This notice may not be removed or altered from any source
///// distribution.
/////
///////////////////////////////////////////////////////////////////////////////
//
///// Changelog
/////
///// 
///// 2022-02-09 - Robert Schmaling
/////  - Initial Completion
/////  - Majority of the devstat and other statistical gathering is taken from FreeBSD devstat.c, iostat.c, and others.
/////  - All of the IO work is taken from freebsd_led.c - taken from the http://www.mediasmartserver.net/forums/viewtopic.php?f=2&t=2335 author ndenev
/////  - Need to turn off the lights individually - for now they are all turned off by the ledoff() function.
/////
/////  - 2022-04-02 - Robert Schmaling
/////  - Created disk_init and run_mediasmart functions in place of original process
/////  - added code that could handle device changes - e.g. hotswap. 
/////  - code cleanup
/////   
/* includes */
#include <stdio.h>
#include <err.h>
#include <limits.h>
#include <signal.h>
#include <math.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <kvm.h>
#include <devstat.h>
#include <camlib.h>
#include <getopt.h>
#include <pwd.h>
#include <syslog.h>
#include <machine/cpufunc.h>

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/types.h>

/* defines */
/*
#define BL1      0x0001     // first blue led                           1
#define BL2      0x0002     // second blue led                          2
#define UNKNOWN1 0x0004     // unknown                                  3
#define BL3      0x0008     // third blue led                           4
#define UNKNOWN2 0x0010     // unknown                                  5
#define BL4      0x0020     // fourth blue led                          6
#define UNKNOWN3 0x0040     // unknown                                  7
#define FLASH    0x0080     // hides (0)/shows(1) onboard flash disk    8
#define RL2      0x0100     // second red led                           9
#define RL3      0x0200     // third red led                            10
#define RL4      0x0400     // fourth red led                           11
#define UNKNOWN4 0x0800     // unknown                                  12
#define RL1      0x1000     // first red led                            13
#define PL1      (BL1 | RL1)// first purple led
#define PL2      (BL2 | RL2)// second purple led
#define PL3      (BL3 | RL3)// third purple led
#define PL4      (BL4 | RL4)// forth purple led
*/

#define ADDR   0x1064 // io address
#define CTL   0xffff // defaults
#define   BL1   0x0001 // first blue led
#define   BL2   0x0002 // second blue led
#define LEDOFF   0x0004 // turns off all leds
#define   BL3   0x0008 // third blue led
#define LEDOFF2   0x0010 // turns off all leds
#define   BL4   0x0020 // fourth blue led
#define LEDOFF3   0x0040 // turns off all leds
#define FLASH   0x0080 // hides/shows onboard flash disk
#define RL2   0x0100 // second red led
#define RL3   0x0200 // third red led
#define RL4   0x0400 // fourth red led
#define W4   0x0800 // led off ?
#define RL1   0x1000 // first red led
#define W6   0x2000 // led off ?
#define W7   0x4000 // led off ?
#define W8   0x8000 // led off ?
#define PL1      (BL1 | RL1)// first purple led
#define PL2      (BL2 | RL2)// second purple led
#define PL3      (BL3 | RL3)// third purple led
#define PL4      (BL4 | RL4)// forth purple led
#define OFFSTATE	0X007FFF // state the register should be in when lights are off

#define HDD1   1
#define HDD2   2
#define HDD3   3
#define HDD4   4

#define BLINK_DELAY 65000000 // for nanosleep() timespec struct - blinking delay for LEDs in nanoseconds

enum ledcolor {
	BLUE = 1,
	RED = 2,
	PURPLE = 3,
};

int show_help(char * progname );
void sigterm_handler(int s);
size_t disk_init(void);
size_t run_mediasmart(void);
int blt(int bay_led);
int rlt(int bay_led);
int plt(int bay_led);
int offled(int bay_led, int off_color );
int show_version(char * progname );
void drop_priviledges(void);

struct hpled
{
	u_int64_t b_read;
	u_int64_t b_write;
	u_int64_t n_read;
	u_int64_t n_write;
	size_t dev_index;
	int target_id;
	int path_id;
	int last_color;
	int HDD;
	char path[10];
};

const char *VERSION = "1.0.3";
char *progname;
struct statinfo cur;
int io;
struct device_selection *dev_select;
size_t maxshowdevs, run, num_devices;
size_t global_count = 0;
struct hpled ide0, ide1, ide2, ide3 ;
struct hpled hpex470[4];
u_int16_t encreg;
char *HD = "ide";
devstat_select_mode select_mode;
struct devstat_match *matches;
kvm_t *kd = NULL;
long generation;
int num_devices_specified, num_selected, num_selections, num_matches;
long select_generation;
char **specified_devices;

size_t debug = 0; /* debug option default */
size_t run_as_daemon = 0; /* daemon option default */

/* initialize struct statinfo cur, kvm_t *kd, and configure struct hpled ide0-3 */
size_t disk_init(void) 
{
    size_t dn, di;
    u_int64_t total_bytes_read, total_bytes_write;
    char *devicename;
	struct cam_device *cam_dev = NULL;
	long double etime = 1.00;
	size_t disks = 0;
	num_matches = 0;
	matches = NULL;

	if (devstat_buildmatch(HD, &matches, &num_matches) != 0)
		errx(1, "%s in %s line %d", devstat_errbuf,__FUNCTION__, __LINE__);

	if(debug) printf("\nAfter devstat_buildmatch - Matches = %d Number of Matches = %d \n", matches->num_match_categories, num_matches);

	if (devstat_checkversion(kd) < 0)
		errx(1, "%s in %s line %d", devstat_errbuf, __FUNCTION__, __LINE__);

	if ((num_devices = devstat_getnumdevs(kd)) < 0)
		err(1, "can't get number of devices in %s line %d", __FUNCTION__, __LINE__);

	if(debug) printf("Number of devices is: %ld \n", num_devices);

	cur.dinfo = (struct devinfo *)calloc(1, sizeof(struct devinfo));

	if (cur.dinfo == NULL)
		err(1, "calloc failed in %s line %d", __FUNCTION__, __LINE__);

    if (devstat_getdevs(kd, &cur) == -1)
        err(1, "%s in %s line %d", devstat_errbuf, __FUNCTION__, __LINE__);
	
    specified_devices = calloc(num_matches, sizeof(char *));
	
	if (specified_devices == NULL)
		err(1, "calloc failed for specified_device in %s line %d", __FUNCTION__, __LINE__);

	/* Two characters would suffice - but bigger is sometimes better, especially when its zeroed */
	specified_devices[0] = calloc(1, strlen("111")); 

	if( specified_devices[0] == NULL )
		err(1, "malloc failed for specified_devices[a]");
	
	if(num_devices != cur.dinfo->numdevs)
		err(1, "Number of devices is inconsistent in %s line %d", __FUNCTION__, __LINE__);

	assert(sizeof(specified_devices[0]) > sizeof("4"));
	strlcpy(specified_devices[0], "4", sizeof(specified_devices[0]));

	maxshowdevs = 4;
	num_devices = cur.dinfo->numdevs;
	generation = cur.dinfo->generation;
	num_devices_specified = num_matches;

	/* calculate all updates since boot */
	cur.snap_time = 0;

	if(debug) {
		printf("Max Show Devices = %ld \n", maxshowdevs);
		printf("Number of Devices = %ld \n", num_devices);
		printf("Generation = %ld \n", generation);
		printf("Number of Devices Specified = %d \n", num_devices_specified);
		printf("Specified Devices is = %s \n", specified_devices[0]);
		printf("End of devstat selection section in %s line %d\n\n\n", __FUNCTION__, __LINE__);
	}

	dev_select = NULL;
	select_mode = DS_SELECT_ONLY;

	if (devstat_selectdevs(&dev_select, &num_selected,
                            &num_selections, &select_generation, generation,
                            cur.dinfo->devices, num_devices, matches,
                            num_matches, specified_devices,
                            num_devices_specified, select_mode, maxshowdevs,
                            0) == -1)
    		errx(1, "%s", devstat_errbuf);


    for (dn = 0; dn < num_devices; dn++) {

		if (dev_select[dn].selected > maxshowdevs)
                       continue;

        di = dev_select[dn].position;

		if (devstat_compute_statistics(&cur.dinfo->devices[di], NULL, etime, DSM_TOTAL_BYTES_READ, &total_bytes_read, 				
			DSM_TOTAL_BYTES_WRITE, &total_bytes_write, DSM_NONE) != 0)
            err(1, "%s in %s line %d", devstat_errbuf, __FUNCTION__, __LINE__);

        if ((dev_select[dn].selected == 0) || (dev_select[dn].selected > maxshowdevs))
                continue;

	    if (asprintf(&devicename, "/dev/%s%d", cur.dinfo->devices[di].device_name, cur.dinfo->devices[di].unit_number) == -1)
	 		errx(1, "asprintf"); 

		cam_dev = cam_open_device(devicename, O_RDWR);

		if(debug) {
			printf("The device name is    : %s \n", devicename);
			printf("CAM device name is    : %s \n", cam_dev->device_name);
			printf("The Unit Number is    : %i \n", cam_dev->dev_unit_num);
			printf("The Sim Name is       : %s \n", cam_dev->sim_name);
			printf("The sim_unit_number is: %i \n", cam_dev->sim_unit_number);
			printf("The bus_id is         : %i \n", cam_dev->bus_id);
			printf("The target_lun is     : %li \n", cam_dev->target_lun);
			printf("The target_id is      : %i \n", cam_dev->target_id);
			printf("The path_id is        : %i \n", cam_dev->path_id);
			printf("The pd_type is        : %i \n", cam_dev->pd_type);
			printf("The file descriptor is: %i \n", cam_dev->fd);
		}

		/* on a HP EX47x there are only 4 IDE devices (provided you set the bios to 4(IDE) 4(IDE) per the mediasmart forum. These will always be the same */
		/* rather than mess around with dynamically allocating and figuring them out, I'm just hardcoding them here */
		if( cam_dev->path_id == 0 && cam_dev->target_id == 0) {
			assert(sizeof(devicename) < sizeof(ide0.path));
			strlcpy(ide0.path, devicename, sizeof(ide0.path));
			ide0.target_id = cam_dev->target_id;		
			ide0.path_id = cam_dev->path_id;
			ide0.dev_index = di;
			ide0.b_read = total_bytes_read;
			ide0.b_write = total_bytes_write;
			ide0.n_read = 0;
			ide0.n_write = 0;
			ide0.HDD = 1;
			hpex470[di] = ide0;

			if(debug){
				printf("HP Disk %d :\nTotal bytes read: %ld\nTotal bytes write: %ld\n\n",ide0.HDD, ide0.b_read, ide0.b_write);
				printf("Now Monitoring %s in HP Mediasmart Server Slot %i \n\n",ide0.path, ide0.HDD);
			}

			syslog(LOG_NOTICE,"Now Monitoring %s in HP Mediasmart Server Slot %i for activity",ide0.path, ide0.HDD);
			++disks;	
		}
		else if ( cam_dev->path_id == 0 && cam_dev->target_id == 1) {
			assert(sizeof(devicename) < sizeof(ide1.path));
			strlcpy(ide1.path,devicename, sizeof(ide1.path));
			ide1.target_id = cam_dev->target_id;		
			ide1.path_id = cam_dev->path_id;
			ide1.dev_index = di;
			ide1.b_read = total_bytes_read;
			ide1.b_write = total_bytes_write;
			ide1.n_read = 0;
			ide1.n_write = 0;
			ide1.HDD = 2;
			hpex470[di] = ide1;

			if(debug){
				printf("HP Disk %d :\nTotal bytes read: %ld \nTotal bytes write: %ld\n\n",ide1.HDD, ide1.b_read, ide1.b_write);
				printf("Now Monitoring %s in HP Mediasmart Server Slot %i \n\n",ide1.path, ide1.HDD);
			}

			syslog(LOG_NOTICE,"Now Monitoring %s in HP Mediasmart Server Slot %i for activity",ide1.path, ide1.HDD);

		}
		else if ( cam_dev->path_id == 1 && cam_dev->target_id == 0) {
			assert(sizeof(devicename) < sizeof(ide2.path));
			strlcpy(ide2.path,devicename, sizeof(ide2.path));
			ide2.target_id = cam_dev->target_id;		
			ide2.path_id = cam_dev->path_id;
			ide2.dev_index = di;
			ide2.b_read = total_bytes_read;
			ide2.b_write = total_bytes_write;
			ide2.n_read = 0;
			ide2.n_write = 0;
			ide2.HDD = 3;
			hpex470[di] = ide2;

			if(debug){
				printf("HP Disk %d :\nTotal bytes read: %ld\nTotal bytes write: %ld\n\n",ide2.HDD, ide2.b_read, ide2.b_write);
				printf("Now Monitoring %s in HP Mediasmart Server Slot %i \n\n",ide2.path, ide2.HDD);
			}

			syslog(LOG_NOTICE,"Now Monitoring %s in HP Mediasmart Server Slot %i for activity",ide2.path, ide2.HDD);

		}
		else if ( cam_dev->path_id == 1 && cam_dev->target_id == 1) {
			assert(sizeof(devicename) < sizeof(ide3.path));
			strlcpy(ide3.path,devicename, sizeof(ide3.path));
			ide3.target_id = cam_dev->target_id;		
			ide3.path_id = cam_dev->path_id;
			ide3.dev_index = di;
			ide3.b_read = total_bytes_read;
			ide3.b_write = total_bytes_write;
			ide3.n_read = 0;
			ide3.n_write = 0;
			ide3.HDD = 4;
			hpex470[di] = ide3;

			if(debug){
				printf("HP Disk %d :\nTotal bytes read: %ld \nTotal bytes write: %ld\n\n",ide3.HDD, ide3.b_read, ide3.b_write);
				printf("Now Monitoring %s in HP Mediasmart Server Slot %i \n\n",ide3.path, ide3.HDD);
			}

			syslog(LOG_NOTICE,"Now Monitoring %s in HP Mediasmart Server Slot %i for activity",ide3.path, ide3.HDD);

		}
		else { /* something went wrong here */
			err(1, "unknown path_id or target_id in %s line %d", __FUNCTION__, __LINE__);
		}

		if(di > 3)
			err(1, "Illegal number of devices - di = %ld in %s line %d", di, __FUNCTION__, __LINE__);

		cam_close_device(cam_dev);
		free(devicename);
	}
	free(specified_devices[0]);
	specified_devices[0] = NULL;
	free(specified_devices);
	specified_devices = NULL;
	free(dev_select);
	dev_select = NULL;
	free(matches);
	matches = NULL;

	if(debug)
		printf("\nThe number of disks is %ld in %s line %d\n", disks, __FUNCTION__, __LINE__);

	return (disks);
};
 /* function to monitor disk activity. if a device change is detected, break and re-initialize */
size_t run_mediasmart(void)
{
    long double etime = 1.00;
	int led_state = 0;
	int retval = 0;
	struct timespec tv = { .tv_sec = 0, .tv_nsec = BLINK_DELAY };

	while( run ) {
		
		retval = devstat_getdevs(kd, &cur);

		if( retval == 1) {
			run = 0;
			break;
		}
		if( retval == -1) {
			syslog(LOG_CRIT, "Bad return from devstat_getdevs() in function %s line %d",__FUNCTION__, __LINE__ );
			fprintf(stderr, "invalid return from devstat_getdevs() in %s line %d", __FUNCTION__, __LINE__);
			run = 0;
			break;
		}

		for (int x = 0; x < global_count; x++) {
			/* we only need read and write. we don't have a statinfo last thus NULL. etime isn't used in these stats but passed for completeness */
			if (devstat_compute_statistics(&cur.dinfo->devices[hpex470[x].dev_index], NULL, etime,
     		    DSM_TOTAL_BYTES_READ, &hpex470[x].n_read, DSM_TOTAL_BYTES_WRITE, &hpex470[x].n_write, DSM_NONE) != 0)
					err(1, "%s in %s line %d", devstat_errbuf, __FUNCTION__, __LINE__);

			if ((hpex470[x].b_read != hpex470[x].n_read) && (hpex470[x].b_write != hpex470[x].n_write)) {
				/* we both read and wrote at the same time - yes this happens */
				hpex470[x].b_read = hpex470[x].n_read;
				hpex470[x].b_write = hpex470[x].n_write;

				if(debug)
					printf("HDD %i - total bytes read: %li  total bytes write: %li \n",hpex470[x].HDD, hpex470[x].n_read, hpex470[x].n_write);

				plt(hpex470[x].HDD);
				led_state = 1;
				hpex470[x].last_color = PURPLE;
			}
			else if (hpex470[x].b_read != hpex470[x].n_read ) {
				/* we read some number of bytes */
				hpex470[x].b_read = hpex470[x].n_read;
				
				if(debug)
					printf("HDD %i - total bytes read: %li \n", hpex470[x].HDD, hpex470[x].n_read);

				plt(hpex470[x].HDD);	
				led_state = 1;
				hpex470[x].last_color = PURPLE;
			}
			else if (hpex470[x].b_write != hpex470[x].n_write) {
				/* we wrote some number of bytes */
				hpex470[x].b_write = hpex470[x].n_write;
				
				if(debug)
					printf("HDD %i - total bytes written: %li \n", hpex470[x].HDD, hpex470[x].n_write);
				
				blt(hpex470[x].HDD);
				led_state = 1;
				hpex470[x].last_color = BLUE;
			}
			else {
				nanosleep(&tv, NULL);

				if( led_state ) {
					/* we turn off the leds */
					/* off_color: 1 = blue    2 = red    3 = purple */
					led_state = offled(hpex470[x].HDD, hpex470[x].last_color);
					hpex470[x].last_color = 0;
				}
				continue ;
			}

		}

	}
	return(retval);
};
/* blue led toggle */
int blt(int bay_led)
{
   switch (bay_led) {
      case HDD1:
         encreg &= ~BL1;
         break;
      case HDD2:
         encreg &= ~BL2;
         break;
      case HDD3:
         encreg &= ~BL3;
         break;
      case HDD4:
         encreg &= ~BL4;
         break;
   }
   outw(ADDR, encreg);
   return(0);
};

/* red led toggle */
int rlt(int bay_led)
{
   switch (bay_led) {
      case HDD1:
         encreg &= ~RL1;
         break;
      case HDD2:
         encreg &= ~RL2;
         break;
      case HDD3:
         encreg &= ~RL3;
         break;
      case HDD4:
         encreg &= ~RL4;
         break;
   }
   outw(ADDR, encreg);
   return(0);
};

/* purple led toggle */
int plt(int bay_led)
{
	switch (bay_led) {
  	   case HDD1:
	      encreg &= ~PL1;
	      break;
	   case HDD2:
	      encreg &= ~PL2;
	      break;
	   case HDD3:
	      encreg &= ~PL3;
	      break;
	   case HDD4:
	      encreg &= ~PL4;
	      break;
	}
	outw(ADDR, encreg);
	return(0);
};
/* turn off the bay light led based on last color */
int offled(int bay_led, int off_color )
{
	/* 1 = blue    2 = red    3 = purple */
	encreg = inw(ADDR);
	switch( off_color ) {
	case 1:
		switch (bay_led) {
			case HDD1:
				encreg |= BL1;
				break;
			case HDD2:
				encreg |= BL2;
				break;
			case HDD3:
				encreg |= BL3;
				break;
			case HDD4:
				encreg |= BL4;
				break;
			}
		break;
	case 2:
		switch (bay_led) {
			case HDD1:
				encreg |= RL1;
				break;
			case HDD2:
				encreg |= RL2;
				break;
			case HDD3:
				encreg |= RL3;
				break;
			case HDD4:
				encreg |= RL4;
				break;
			}
		break;
	case 3:
		switch (bay_led) {
			case HDD1:
				encreg |= PL1;
				break;
			case HDD2:
				encreg |= PL2;
				break;
			case HDD3:
				encreg |= PL3;
				break;
			case HDD4:
				encreg |= PL4;
				break;
		}
		break;

	}
	outw(ADDR, encreg);
	return(0);
};
/* attempt to drop privileges after initialization */
void drop_priviledges(void) {
	struct passwd* pw = getpwnam( "nobody" );
	if ( !pw ) return; /* huh? */
	if ( (setgid( pw->pw_gid )) && (setuid( pw->pw_uid )) != 0 )
		err(1, "Unable to set gid or uid to nobody in %s line %d", __FUNCTION__, __LINE__);

	if(debug) {
		printf("Successfully dropped priviledges to %s \n",pw->pw_name);
	}
};
/* return the name of the program */
char* curdir(char *str)
{
	char *cp = strrchr(str, '/');
	return cp ? cp+1 : str;
};
/* display help function */
int show_help(char * progname ) {

	char *this = curdir(progname);
	printf("%s %s %s", "Usage: ", this,"\n");
	printf("-d, --debug 	Print Debug Messages\n");
	printf("-D, --daemon 	Detach and Run as a Daemon - do not use this in service setup \n");
	printf("-h, --help	Print This Message\n");
	printf("-v, --version	Print Version Information\n");

       return 0;
};
/* display version */
int show_version(char * progname ) {
	char *this = curdir(progname);
        printf("%s %s %s %s %s %s %s",this,VERSION,"compiled on", __DATE__,"at", __TIME__ ,"\n") ;
        return 0;
};

int main (int argc, char **argv)
{
   
	if (geteuid() !=0 ) {
		printf("Must be run as root\n");
		err(1, "not running as root user");
	}

        // long command line arguments
        const struct option long_opts[] = {
                { "debug",          no_argument,       0, 'd' },
                { "daemon",         no_argument,       0, 'D' },
                { "help",           no_argument,       0, 'h' },
                { "version",        no_argument,       0, 'v' },
                { 0, 0, 0, 0 },
        };

        // pass command line arguments
        while ( 1 ) {
                const int c = getopt_long( argc, argv, "adDhv?", long_opts, 0 );
                if ( -1 == c ) break;

                switch ( c ) {
		case 'D': // daemon
			++run_as_daemon;
			break;
                case 'd': // debug
                        ++debug;
                        break;
                case 'h': // help!
                        return show_help(argv[0]);
                case 'v': // our version
                        return show_version(argv[0] );
                case '?': // no idea
                        return show_help(argv[0] );
                default:
                      printf("++++++....\n"); 
                }
        }
	progname = curdir(argv[0]);

	openlog("hpex47xled:", LOG_CONS | LOG_PID, LOG_DAEMON );
	syslog( LOG_NOTICE, "Starting %s version %s",progname, VERSION );
	signal( SIGTERM, sigterm_handler);
	signal( SIGINT, sigterm_handler);
	signal( SIGQUIT, sigterm_handler);
	signal( SIGILL, sigterm_handler);

	if ( run_as_daemon ) {
		if (daemon( 0, 0 ) > 0 )
			err(1, "Unable to daemonize :");
	  }

	io = open("/dev/io", 000);
	encreg = CTL;
	outw(ADDR, encreg); 

	global_count = disk_init();

	/* Try and drop root priviledges now that we have initialized */
	drop_priviledges();

	syslog(LOG_NOTICE,"Initialized. Now monitoring for drive activity");

	run = 1;
	while(1) {
 
        switch (run) {
            case 1: {
                int retval = run_mediasmart();

                switch(retval) {
                    case -1:
                        errx(1, "%s", devstat_errbuf);
                        break;
                    case 1:
						free(cur.dinfo);
						cur.dinfo = NULL;
						free(dev_select);
						free(matches);
						syslog(LOG_NOTICE, "New or removed device detected - reinitializing");
						if(debug)
							fprintf(stderr, "\n\n**** New/Removed Device Detected - re-initializing ****\n\n");
						global_count = disk_init();
						if(global_count <= 0)
							err(1, "Unknown return from disk initialization in %s line %d", __FUNCTION__, __LINE__);
						run = 1;
                        break;
					default:
						fprintf(stderr,"In default case in retval switch loop - function %s line %d\n", __FUNCTION__, __LINE__);
						break;
                }
            }
            default:
				fprintf(stderr,"In default case in run switch - function %s line %d\n", __FUNCTION__, __LINE__);
                break;
        }

	}	

	close(io);
	syslog(LOG_NOTICE,"Closing Down");
	closelog();	
	return(0);
};
/* signal handling and cleanup */
void sigterm_handler(int s)
{
	outw(ADDR, encreg);
	syslog(LOG_NOTICE,"Caught signal %d and closing down", s);
	closelog();
	close(io);
	free(cur.dinfo);
	free(dev_select);
	free(matches);
	err(1, "Exiting from signal");
};
