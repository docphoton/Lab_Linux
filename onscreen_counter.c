/* onscreen counter
	time tags asynchronous pulses, and gives the combined 
		deadtime per pulse (pulsewidth + computer deadtime) 
		for each pulse. Can collect on up to three channels
	at once. 
*/ 
/*
 * Copyright 1996 Doug Hughes Auburn University
 * @(#)upssta t.c	1.3 08/26/96	this notice must be left intact
 * every second print in a nice table which RS-232 lines are on
 * and which are off. (X means on)
 */
/* Modified Dec. 17, 2002 : to make it a counter for the GM tubes
   given out by the ANS YAPA session. "Do it for the students!" 
   Modified May 14th and 28th by Joel Lepak to do coincidence between 
	two GM counters	
 */
#include <stdio.h>
#include <sys/time.h>
#include <termio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <time.h>
main(int argc, char *argv[]) 
{
	int fd, len=10; 
 	 /* len=1000 is roughly one (a few) hour(s)  with background radiation */
	int flag, i, j, ilast, COUNT, hit, list[2][len], pause, hitlast;
	double time_start, time_elapsed, deadtime; 
	struct stat sb;
	char devname[20]="/dev/";
	char status[40], startstring, stopstring;
static struct timeval _tstart, _tend;
static struct timezone tz;
void tstart(void) { gettimeofday(&_tstart, &tz); }
void tend(void) { gettimeofday(&_tend,&tz); }
double tval()
   { int t1, t2;
t1 = 1000000*(int)_tstart.tv_sec + (int)_tstart.tv_usec;
t2 = 1000000*(int)_tend.tv_sec + (int)_tend.tv_usec;
 return t2/(double)(1000000);
   }
	FILE* fn;

	if (argc < 3) {      
		fprintf(stderr, "must supply device name (e.g. term/a, ttya) and output file name\n");
		fprintf(stderr, "usage: ./a.out device filename\n");
		exit(1);
	}
	fn = fopen(argv[2],"w");

	if (strcmp(argv[1], "-ttytab") == 0) {
    		fd = 0;		/* terminal has already been opened as fd 0 */
					/* This is for experimental support of SunOS */
	} else  {
		strcat(devname, argv[1]);
		if ((fd = open(devname, O_RDONLY)) < 0) {
			fprintf(stderr, "couldn't open device: %s\n", devname);
			exit(1);
		}
	}
	
	/* Get terminal modes */
	i = TIOCM_DTR;
	ioctl(fd, TIOCMSET, &i);
	i=0;
	j=0; 
	flag=0;
	COUNT=0;
	//	startstring=asctime();
	hit=0;
	pause=1; 
	//        printf("%.A\n",tm);
 time_elapsed=0; 
 hit = 0 ;    // line normally low out of SCA, redone for cosmic rays with NaI xtals and camac crate electronics
 pause = hit; 
 tstart();
 tend(); 
 time_start = (double)((int)_tend.tv_sec + (double)_tend.tv_usec/(double)(1000000));
	while (1) 
        {	
	  //if (i % 22  == 0) printf("LE DTR RTS ST SR CTS CD RNG DSR\n");
	  //sprintf(status,          "O   O   O  O  O   O  O   O   O   ");
	        i++;
		hit=0;
		if (ioctl(fd, TIOCMGET, &j) < 0) {
			perror("Getting hardware status bits");
			exit(1);
		}
		//if (j & TIOCM_LE) status[0] = 'X'; 
		//if (j & TIOCM_DTR) status[4] = 'X';
		//if (j & TIOCM_RTS) status[8] = 'X';
		//if (j & TIOCM_ST) status[11] = 'X';
		//if (j & TIOCM_SR) status[14] = 'X';
		if ((j & TIOCM_CTS)) hit+=4;
		if ((j & TIOCM_CAR)) hit++; //pin 5 lights up
		if ((j & TIOCM_RNG)) hit+=2; // pin 6 ''
		//  {
		//    status[25] = 'X';
if (hit != pause){ 
	tend(); 
	if (pause == 0) { //log up going edge, normally low; pulse is logic level high.
	time_elapsed = (double)((int)_tend.tv_sec + (double)((int)_tend.tv_usec/(double)(1000000)));
	ilast = i; 
	} 
	else{ // log down-going edge, with i count, and zero out time difference
	  time_elapsed=time_start; 
	  j = i-ilast;
	  i = 0; 
	  ilast = 0; 
	} 

//                    tstart();
//	else{  
//	deadtime = (double)((int)_tend.tv_sec + (double)((int)_tend.tv_usec/(double)(1000000)) - time_elapsed);
 printf("%.i   %6.3f   %.i   %.i\n",COUNT+1, time_elapsed-time_start, j , hit);
	COUNT++;
		    //	} 
	pause = hit; 
		 //   usleep(0); 
 }
                //if (j & TIOCM_DSR) status[29] = 'X';
		//sleep(50);
 }
	for(COUNT=0; COUNT<len; COUNT++)
        {
             fprintf(fn, "%.i %.i  %.i", COUNT+1, list[0][COUNT], list[1][COUNT]);
             if (list[0][COUNT]==list[0][COUNT-1]+1)
	       fprintf(fn,"x\n");
	     else
	       fprintf(fn,"\n");
             fflush(NULL);
        }
}
