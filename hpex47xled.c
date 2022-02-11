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

#define HDD1   1
#define HDD2   2
#define HDD3   3
#define HDD4   4

int show_help(char * progname );
void sigterm_handler(int s);
static void devstats(int init, long double etime);
int blt(int led);
int rlt(int led);
int plt(int led);
int offled(int led);
int show_version(char * progname );
int show_help(char * progname );
void drop_priviledges( );

struct hpled
{
	char path[10];
	int target_id;
	int path_id;
	u_int64_t b_read;
	u_int64_t b_write;
	u_int64_t n_read;
	u_int64_t n_write;
	int HDD;
};

static struct statinfo cur, last;
static int io;
static int num_devices;
static struct device_selection *dev_select;
static int maxshowdevs;
static int global_count = 0;
static struct hpled ide0, ide1, ide2, ide3 ;
static struct hpled hpex470[4];
u_int16_t encreg;

static int debug = 0; /* set it in here for now. I'll make it a command line option later */
static int run_as_daemon = 0; /* set it in here for now. I'll make it a command line option later */

static void
devstats(int init, long double etime)
{
        int dn;
        u_int64_t total_bytes, total_bytes_read, total_bytes_write; 
        char *devicename;
	struct cam_device *cam_dev = NULL;

        for (dn = 0; dn < num_devices; dn++) {
                int di;

		//if ((dev_select[dn].selected == 0) || (dev_select[dn].selected > maxshowdevs))
		if (dev_select[dn].selected > maxshowdevs)
                       continue;

                di = dev_select[dn].position;
/*
		    What is NULL'ed out below:
     		    DSM_TOTAL_TRANSFERS, &total_transfers,
                    DSM_TOTAL_TRANSFERS_READ, &total_transfers_read,
                    DSM_TOTAL_TRANSFERS_WRITE, &total_transfers_write,
                    DSM_TOTAL_BLOCKS, &total_blocks,
                    DSM_KB_PER_TRANSFER, &kb_per_transfer,
                    DSM_TRANSFERS_PER_SECOND, &transfers_per_second,
                    DSM_TRANSFERS_PER_SECOND_READ, &transfers_per_second_read,
                    DSM_TRANSFERS_PER_SECOND_WRITE, &transfers_per_second_write,
                    DSM_MB_PER_SECOND, &mb_per_second,
                    DSM_MB_PER_SECOND_READ, &mb_per_second_read,
                    DSM_MB_PER_SECOND_WRITE, &mb_per_second_write,
                    DSM_BLOCKS_PER_SECOND, &blocks_per_second,
                    DSM_MS_PER_TRANSACTION, &ms_per_transaction,
                    DSM_MS_PER_TRANSACTION_READ, &ms_per_read,
                    DSM_MS_PER_TRANSACTION_WRITE, &ms_per_write,
                    DSM_MS_PER_TRANSACTION_OTHER, &ms_per_other,
                    DSM_BUSY_PCT, &busy_pct,
                    DSM_QUEUE_LENGTH, &queue_len,
                    DSM_TOTAL_DURATION, &total_duration,
                    DSM_TOTAL_BUSY_TIME, &busy_time,
*/


                  if (devstat_compute_statistics(&cur.dinfo->devices[di], NULL, etime,
                    DSM_TOTAL_BYTES, &total_bytes,
                    DSM_TOTAL_BYTES_READ, &total_bytes_read,
                    DSM_TOTAL_BYTES_WRITE, &total_bytes_write,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
		    NULL,
                    NULL,
                    NULL, 
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL, 
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    DSM_NONE) != 0)
                        errx(1, "%s", devstat_errbuf);

                if ((dev_select[dn].selected == 0) || (dev_select[dn].selected > maxshowdevs))
                     continue;

		if ( init == 1 ) {
			/* I know, this is suboptimal */	
	        	if (asprintf(&devicename, "/dev/%s%d", cur.dinfo->devices[di].device_name, cur.dinfo->devices[di].unit_number) == -1)
	 			errx(1, "asprintf"); 
			cam_dev = cam_open_device(devicename, O_RDWR);
			if(debug) {
				printf("struct devinfo device name after adding 0-3 is %s \n",devicename);
				printf("CAM device name is : %s \n", cam_dev->device_name);
				printf("The Unit Number is: %i \n", cam_dev->dev_unit_num);
				printf("The Sim Name is: %s \n", cam_dev->sim_name);
				printf("The sim_unit_number is: %i \n", cam_dev->sim_unit_number);
				printf("The bus_id is: %i \n", cam_dev->bus_id);
				printf("The target_lun is: %li \n",cam_dev->target_lun);
				printf("The target_id is: %i \n",cam_dev->target_id);
				printf("The path_id is: %i \n",cam_dev->path_id);
				printf("The pd_type is: %i \n",cam_dev->pd_type);
				printf("The file descriptor is: %i \n",cam_dev->fd);
			}

			/* on a HP EX47x there are only 4 IDE devices (provided you set the bios to 4(IDE) 4(IDE) per the mediasmart forum. These will always be the same */
			/* rather than mess around with dynamically allocating and figuring them out, I'm just hardcoding them here */
			if( cam_dev->path_id == 0 && cam_dev->target_id == 0) {
				sprintf(ide0.path,devicename);
				ide0.target_id = cam_dev->target_id;		
				ide0.path_id = cam_dev->path_id;
				ide0.b_read = total_bytes_read;
				ide0.b_write = total_bytes_write;
				ide0.n_read = 0;
				ide0.n_write = 0;
				ide0.HDD = 1;
				hpex470[di] = ide0;

				if(debug)
					printf("Now Monitoring %s in HP Mediasmart Server Slot %i \n",ide0.path, ide0.HDD);

				syslog(LOG_NOTICE,"Now Monitoring %s in HP Mediasmart Server Slot %i for activity",ide0.path, ide0.HDD);
				
			}
			else if ( cam_dev->path_id == 0 && cam_dev->target_id == 1) {
				sprintf(ide1.path,devicename);
				ide1.target_id = cam_dev->target_id;		
				ide1.path_id = cam_dev->path_id;
				ide1.b_read = total_bytes_read;
				ide1.b_write = total_bytes_write;
				ide1.n_read = 0;
				ide1.n_write = 0;
				ide1.HDD = 2;
				hpex470[di] = ide1;

				if(debug)
					printf("Now Monitoring %s in HP Mediasmart Server Slot %i \n",ide1.path, ide1.HDD);

				syslog(LOG_NOTICE,"Now Monitoring %s in HP Mediasmart Server Slot %i for activity",ide1.path, ide1.HDD);

			}
			else if ( cam_dev->path_id == 1 && cam_dev->target_id == 0) {
				sprintf(ide2.path,devicename);
				ide2.target_id = cam_dev->target_id;		
				ide2.path_id = cam_dev->path_id;
				ide2.b_read = total_bytes_read;
				ide2.b_write = total_bytes_write;
				ide2.n_read = 0;
				ide2.n_write = 0;
				ide2.HDD = 3;
				hpex470[di] = ide2;

				if(debug)
					printf("Now Monitoring %s in HP Mediasmart Server Slot %i \n",ide2.path, ide2.HDD);

				syslog(LOG_NOTICE,"Now Monitoring %s in HP Mediasmart Server Slot %i for activity",ide2.path, ide2.HDD);

			}
			else if ( cam_dev->path_id == 1 && cam_dev->target_id == 1) {
				sprintf(ide3.path,devicename);
				ide3.target_id = cam_dev->target_id;		
				ide3.path_id = cam_dev->path_id;
				ide3.b_read = total_bytes_read;
				ide3.b_write = total_bytes_write;
				ide3.n_read = 0;
				ide3.n_write = 0;
				ide3.HDD = 4;
				hpex470[di] = ide3;

				if(debug)
					printf("Now Monitoring %s in HP Mediasmart Server Slot %i \n",ide3.path, ide3.HDD);

				syslog(LOG_NOTICE,"Now Monitoring %s in HP Mediasmart Server Slot %i for activity",ide3.path, ide3.HDD);

			}
			else { /* something went wrong here */
				err(1, "unknown path_id or target_id");
			}

			if(di > 3)
				err(1, "Illegal number of devices in devstat()");

			hpex470[di].b_read = total_bytes_read;
			hpex470[di].b_write = total_bytes_write;
			global_count = di;
			cam_close_device(cam_dev);
			free(devicename);
		}
		else {
			hpex470[di].n_read = total_bytes_read;
			hpex470[di].n_write = total_bytes_write;
		}
        }
}

/* blue led toggle */
int
blt(int led)
{
   switch (led) {
      case HDD1:
         encreg = encreg ^ BL1;
         break;
      case HDD2:
         encreg = encreg ^ BL2;
         break;
      case HDD3:
         encreg = encreg ^ BL3;
         break;
      case HDD4:
         encreg = encreg ^ BL4;
         break;
   }
   outw(ADDR, encreg);
   return(0);
}

/* red led toggle */
int
rlt(int led)
{
   switch (led) {
      case HDD1:
         encreg = encreg ^ RL1;
         break;
      case HDD2:
         encreg = encreg ^ RL2;
         break;
      case HDD3:
         encreg = encreg ^ RL3;
         break;
      case HDD4:
         encreg = encreg ^ RL4;
         break;
   }
   outw(ADDR, encreg);
   return(0);
}

/* purple led toggle */
int
plt(int led)
{
	switch (led) {
  	   case HDD1:
	      encreg = encreg ^ PL1;
	      break;
	   case HDD2:
	      encreg = encreg ^ PL2;
	      break;
	   case HDD3:
	      encreg = encreg ^ PL3;
	      break;
	   case HDD4:
	      encreg = encreg ^ PL4;
	      break;
	}
	outw(ADDR, encreg);
	return(0);
}

int
offled(int led)
{
	// usleep(100000);
	usleep(85000);
	/* doing this until I can figure out how to turn off the each light individually */
	encreg = CTL;
	outw(ADDR, encreg);
	return(0);
}

void
drop_priviledges( ) {
	struct passwd* pw = getpwnam( "nobody" );
	if ( !pw ) return; /* huh? */
	if ( (setgid( pw->pw_gid )) && (setuid( pw->pw_uid )) != 0 )
		err(1, "Unable to set gid or uid to nobody");

	if(debug) {
		printf("Successfully dropped priviledges to %s \n",pw->pw_name);
		printf("We should now be safe to continue \n");
	}
}

char*
curdir(char *str)
{
	char *cp = strrchr(str, '/');
	return cp ? cp+1 : str;
}

int show_help(char * progname ) {

	char *this = curdir(progname);
	printf("%s %s %s", "Usage: ", this,"\n");
	printf("-d, --debug 	Print Debug Messages\n");
	printf("-D, --daemon 	Detach and Run as a Daemon - do not use this in service setup \n");
	printf("-h, --help	Print This Message\n");
	printf("-v, --version	Print Version Information\n");

       return 0;
}

int show_version(char * progname ) {
	char *this = curdir(progname);
        printf("%s %s %s %s %s %s",this,"Version 0.0.1 compiled on", __DATE__,"at", __TIME__ ,"\n") ;
        return 0;
}

int
main (int argc, char **argv)
{
   struct devstat_match *matches;
   int num_matches = 0;
   kvm_t *kd = NULL;
   long generation;
   int num_devices_specified;
   int num_selected, num_selections;
   long select_generation;
   char **specified_devices;
   char *HD = "ide\0";
   devstat_select_mode select_mode;
   long double etime;
   int init = 0;

	if (geteuid() !=0 ) {
		printf("Try running as root to avoid Segfault and core dump \n");
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

	openlog("hpex47xled:", LOG_CONS | LOG_PID, LOG_DAEMON );
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
	matches = NULL;

	if (devstat_buildmatch(HD, &matches,&num_matches) != 0)
               errx(1, "%s", devstat_errbuf);

	if(debug) {
		printf("Match Categories from struct matches is: %i \n", matches->num_match_categories);
		printf("Number of matched devices is: %i \n", num_matches);
	}

	if (devstat_checkversion(kd) < 0)
                errx(1, "%s", devstat_errbuf);

        if ((num_devices = devstat_getnumdevs(kd)) < 0)
                err(1, "can't get number of devices");

	if(debug) {
        	printf("Total Devices: %i \n", num_devices);
        	printf("Total Matches: %i \n", num_matches);
		printf("Device selection for buildmatch: %s\n", HD);
	}

        cur.dinfo = (struct devinfo *)calloc(1, sizeof(struct devinfo));
        if (cur.dinfo == NULL)
                err(1, "calloc failed");

        last.dinfo = (struct devinfo *)calloc(1, sizeof(struct devinfo));
        if (last.dinfo == NULL)
                err(1, "calloc failed");

        if (devstat_getdevs(kd, &cur) == -1)
                errx(1, "%s", devstat_errbuf);

 	/* specified_devices = (char **)malloc(sizeof(char *)); */
 	specified_devices = calloc(num_matches, sizeof(char *));
       	if (specified_devices == NULL)
	        err(1, "calloc failed for specified_device");

	specified_devices[0] = malloc(strlen("11") * sizeof(char *));
	if( specified_devices[0] == NULL )
		err(1, "malloc failed for specified_devices[a]");

        sprintf(specified_devices[0],"%i", 4);

	// maxshowdevs = num_matches;
	maxshowdevs = 4;
        num_devices = cur.dinfo->numdevs;
        generation = cur.dinfo->generation;
	num_devices_specified = num_matches;
	// num_devices_specified = 4;

	if(debug) {
		printf("The number of devices specified at specified_devices[0] is : %s \n",specified_devices[0]);
		printf("maxshowdevs after assignment from num_matches: %i \n", maxshowdevs);
		printf("num_devices after assginment from cur.dinfo->numdevs: %i \n", num_devices);
		printf("generation from cur.dinfo->generation: %li \n", generation);
		printf("num_devices_specified after assginment from num_matches: %i \n", num_devices_specified);
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

	if(debug) {
	for(int y = 0; y < num_selected ; y++)
		printf("Devices selected: %s \n", dev_select[y].device_name);
	}

        etime = cur.snap_time - last.snap_time;
        if (etime == 0.0)
             etime = 1.0;

	/* a kludge to set the global array with the initial read/write totals */
	init = 1;

        devstats(init, etime);

	if(debug) {
		printf("Global Count is: %d \n", global_count);
		printf("Zero is normal here - we start at 0 through 3 \n");
		printf("Just before the while loop to check for activity \n");
		for(int y = 0; y < num_selected ; y++)
			printf("Devices selected: %s \n", dev_select[y].device_name);
	}

	syslog(LOG_NOTICE,"Initialized. Dropping priviledges now. Now monitoring for drive activity - Enjoy the light show!");

	/* Try and drop root priviledges now that we have initialized */
	drop_priviledges();

	while(1) {
		init = 0;
                /*
                 * Here what we want to do is refresh our device stats.
                 * devstat_getdevs() returns 1 when the device list has changed.
                 * If the device list has changed, we want to go through
                 * the selection process again, in case a device that we
                 * were previously displaying has gone away.
                 */

                switch (devstat_getdevs(kd, &cur)) {
                case -1:
                        errx(1, "%s", devstat_errbuf);
                        break;
                case 1: {
                        int retval;
				
        		select_mode = DS_SELECT_ONLY;
                        num_devices = cur.dinfo->numdevs;
                        generation = cur.dinfo->generation;
                        retval = devstat_selectdevs(&dev_select, &num_selected,
                                                    &num_selections,
                                                    &select_generation,
                                                    generation,
                                                    cur.dinfo->devices,
                                                    num_devices, matches,
                                                    num_matches,
                                                    specified_devices,
                                                    num_devices_specified,
                                                    select_mode, maxshowdevs,
                                                    0);
                        switch(retval) {
                        case -1:
                                errx(1, "%s", devstat_errbuf);
                                break;
                        case 1:
                                break;
                        default:
                                break;
                        }
                        break;
                }
                default:
                        break;
            }
		devstats(init, etime);
		for (int x = 0; x <= global_count; x++) {
			if ((hpex470[x].b_read != hpex470[x].n_read) && (hpex470[x].b_write != hpex470[x].n_write)) {
				/* we both read and wrote at the same time - yes this happens */
				hpex470[x].b_read = hpex470[x].n_read;
				hpex470[x].b_write = hpex470[x].n_write;
				if(debug) {
					printf("Total bytes read: %li  Total bytes written: %li \n",hpex470[x].n_read, hpex470[x].n_write);
					printf("HP HDD is : %i \n", hpex470[x].HDD);
				}
				plt(hpex470[x].HDD);
			}
			else if (hpex470[x].b_read != hpex470[x].n_read ) {
				/* we read some number of bytes */
				hpex470[x].b_read = hpex470[x].n_read;
				if(debug) {
				 	printf("HP HDD is : %i \n", hpex470[x].HDD);
					printf("Total bytes read: %li \n",hpex470[x].n_read);
				}
				plt(hpex470[x].HDD);	
			}
			else if (hpex470[x].b_write != hpex470[x].n_write) {
				/* we wrote some number of bytes */
				hpex470[x].b_write = hpex470[x].n_write;
				if(debug) {
					printf("Total bytes written: %li \n", hpex470[x].n_write);
					printf("HP HDD is : %i \n", hpex470[x].HDD);
				}
				blt(hpex470[x].HDD);
			}
			else {
				/* we turn off the leds */
				offled(hpex470[x].HDD);
			}
	
		}
	}	

	close(io);
	syslog(LOG_NOTICE,"Closing Down");
	closelog();
	free(specified_devices[0]);
	free(specified_devices);

	
	return(0);
}
void sigterm_handler(int s)
{
	syslog(LOG_NOTICE,"Closing Down");
	closelog();
	close(io);
	free(last.dinfo);
	free(cur.dinfo);
	err(1, "Exiting from signal");
}
