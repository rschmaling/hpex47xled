# hpex47xled
This is an attempt at creating a daemon to monitor HD activity and activate the bay lights for the HP EX47x Mediasmart Server for FreeBSD >= 12.3.

You must have libdevstat and libcam installed on your system to run the binary. I suggest compiling for your version as devstat checks its version against
the running kernel and will fail (complain?) if they are different.

It should function on any version of FreeBSD with devstat support - I believe I read somwhere that > FreeBSD 9 will suffice. I have not validated this.

To compile you will need gmake and gcc (mostly because I am lazy and plagerised the makefile from a GNU linux program.

A majority of the code is taken from iostat.c located under /usr/src/usr.sbin/iostat. The program utilizes devstat and the devstat library
to gather kernel usage statistics. 

To run - simply copy the file to /usr/local/bin, place the hpex47xled init file under /usr/local/etc/rc.d/
add 'hpex47xled_enable="YES"' to your /etc/rc.conf file. The file uses syslog() to put messages at LOG_NOTICE into /var/log/messages.
Root privileges are needed to start the program, however, the program attempts to drop privileges after startup to nobody:nobody. 
If that user doesn't exist on your system, the program may fail or it may not...YMMV.

Usage:

*MUST BE RUN AS ROOT*

hpex47xled 

--help - help message
--version - current version of the software
--debug - prints additional information
--daemon - to fork the process into the background. --daemon is only needed if run directly, it is not needed in the hpex47xled rc file.

Do not hesitate to reach out to me with any questions/concerns/suggestions (lots of suggestions and pointers as I am sure I made loads of mistakes)

