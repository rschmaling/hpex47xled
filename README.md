# hpex47xled
This is an attempt at creating a daemon to monitor HD activity and activate the bay lights for the HP EX47x Mediasmart Server for FreeBSD >= 12.3.

You must have libdevstat and libcam installed on your system to run the binary. 

It should function on any version of FreeBSD with CAM and devstat support - FreeBSD > 9? Needs validating.

A majority of the code is taken from iostat.c located under /usr/src/usr.sbin/iostat. The program utilizes devstat and the devstat library
to gather kernel usage statistics. 

'make && sudo make install' will place hpex47xled under /usr/local/bin and hpex47xled.rc under /usr/local/etc/rc.d. Ensure the latter exists.
Manually add 'hpex47xled_enable="YES"' to your /etc/rc.conf file. Messages are sent to syslog() at level LOG_NOTICE - you can view these in /var/log/messages.
Root privileges are needed to start the program, however, the program attempts to drop privileges after startup to nobody:nobody. 

Usage:

*MUST BE RUN AS ROOT*

hpex47xled 

--help - help message
--version - current version of the software
--debug - prints additional information
--daemon - to fork the process into the background. --daemon is only needed if run directly, it is not needed in the hpex47xled rc file.

Do not hesitate to reach out to me with any questions/concerns/suggestions (lots of suggestions and pointers as I am sure I made loads of mistakes)

Feel free to open issues here.

