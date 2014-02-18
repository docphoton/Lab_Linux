/* fmeterslow2.c */
/*      here updated with nanosecond time measurement with clock_gettime()

 	NOTE: you must compile these clock_gettime with the realtime libraries,
that is, I actually used; 
     
 gcc nanosecond_test.c -o nanosecond_test.out -lrt -L /usr/local/lib/ -lgsl -lgslcblas -lm -static
	
(the math library was for the function call to sqrt...that is all. But notice the 
prominent -lrt flag!! 

Also compiled with the -O2 flag and that seemed to speed it up a bit and 
there seemed to be less variability in the output times. 

Also removed all non-essential kernel services that I could...seemed to make things
run better as well. 
*/ 

/*  frequency meter for slow frequencies. Continuously running. 
	idea is that it logs not just the total number of pulses per 
	'samples' interval, but logs the time (in muS) for each pair
	of transitions. Then, at end, when it computes the total number 
	of teeth, if there are any time stamps that are some multiple
	of the average (which we assume is pretty tight), or the minimum, 
	we fix those since they correspond to some # of missing teeth seen. 
	THEN use the corrected value to compute the frequency the usual way. 	

	SO; in the main it is to make time calls during the interval and then to 
	post-process some of the pulse-to-pulse intervals found to correct	
	for missing teeth. 

	Note that this is susceptible to (1) noise and (2) sources that are highly
	variable in short intervals (lots of jitter) and (3) sources in which the 
	frequency is too high (because of the increased latency due to the time calls). 
	
	NOTE: removed the duty cycle stuff to save on time through the loop. Of course, 
	since we have time-logged each transition, one could compute the duty cycle apostiori. 
*/ 
/* 
 FORMAT IS : fmeterfile <port> <integrationtime> 
 */ 
/*    Part of this code is from: 
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
/* Modified Sept. 9, 04: to do a better job of timing, consulting the 
"l-rt1.pdf" document by Dr. Edward Bradford, use the timeofday function in
Linux. It will be smoother wrt interrupts in the loop than an actual loop counter! Check out that document for details */ 

#include <stdio.h>
#include <termio.h> 
#include <errno.h> 
#include <stdlib.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <float.h> 
#include <asm/unistd.h>
#include <pthread.h>
#include <strings.h>
#include <sys/time.h>
#include <getopt.h>
#include <libgen.h>
#include <math.h>

int interrupted = 0;
  
/* signal handler, traps ctrl-c commands so that output is not lost */
void handler(int sig){
   printf("Interrupt detected. Saving data and exiting...\n");
   interrupted = 1;
   return;
}

int main(int argc, char *argv[]) 
{  
  int m=100, fd, maxfreq = 50000, lastdownfive, lastdownsix, firstupfive, firstupsix, len=120000; //maximum freq is maximum of pulses per data point...and len is the maximum record length of the file outputed 
  unsigned long long il, jl, fives[maxfreq], sixes[maxfreq], totfivetime, totsixtime; 
  int sign, flag, i, kk,  j, jj, COUNT, hit, hitprevious, hitfive, five,  fivelast, six, sixlast, transitions, transitionfive, transitionsix, samples = 10000, totallines, pulsesfive[maxfreq], pulsessix[maxfreq], counter, extrabit, totsixteeth, totfiveteeth, junk, gg=1;
  double periodfast, accum, last, periodfive, periodsix, sec_per_bin, listlist[4], timeupfive, timeupsix, rr, runningavg, a, ss, avgf, ff, sq, outputr, naiveperiod, numerator, avgff, fff, sqf, outputff;
	struct stat sb;
  struct timespec time0, time1; 
	char devname[20]="/dev/";
	char status[40], startstring, stopstring;
	//	time_t start, stop;  /* time stuff not currently working! */  
	//	clock_t tm;          /* need to fix the time stuff */
        FILE *outFile;
	if (argc < 2) {      
	       	fprintf(stderr, "usage: ./a.out device integrationtime\n");
		exit(1);
	}      
	//        outFile = fopen(argv[4],"w");
	//        if (outFile == NULL) {
      
	//                printf("error opening %s for writing", argv[4]);
        //        exit(2);
      
        //}
   
	//	totallines = atoi(argv[2]);
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
   
	//        signal(SIGINT, handler);
	
	/* Get terminal modes */
	//clock_gettime(CLOCK_REALTIME,&time0);
	i = TIOCM_DTR;
	ioctl(fd, TIOCMSET, &i);
	// setup for the main loop
	kk=0;
	counter = 0;
	samples = 4000*atoi(argv[2]);
	//	printf(" Frequency of lines 5 and 6 and their periods too ; Integration time is %3.5e sec\n", samples/40000. ); 
        fivelast = 0;
        sixlast = 0;
	avgf = 0.; 
	sq = 0.; 
	runningavg = 1.e6; 
	clock_gettime(CLOCK_REALTIME,&time0);
	// main loop
	while(1)
	  {
	    jj = 0;
	    kk++; 
	transitionsix = 0;
	transitionfive = 0;
	//firstupfive = 0;
	//firstupsix = 0;
	//lastdownfive = 0;
	//lastdownsix = 0;
	//timeupfive = 0;
	//timeupsix = 0;
	//        for (counter = 0; counter < kk; counter++) {
	//        printf(" %3i  %.8e  %.8e\n", kk, 1./(listlist[2]+DBL_MIN), 1./(listlist[3]+DBL_MIN));
 	//clock_gettime(CLOCK_REALTIME,&time1);
	//extrabit0 = 1000000000LL - (time0.tv_nsec - time1.tv_nsec) +(time1.tv_sec - time0.tv_sec -1)*1000000000LL;    
	while(jj<samples) // sets the mean reporting interval. 
	  {
	    flag = flag ^ flag; // clear flag each run through. Flag just captures cases of simultaneous multichannel transitions. 
	    if (ioctl(fd, TIOCMGET, &j) < 0) {
			perror("Getting hardware status bits");
			exit(1); 
		}
	        //if (j & TIOCM_LE) status[0] = 'X'; 
		//if (j & TIOCM_DTR) status[4] = 'X';
		//if (j & TIOCM_RTS) status[8] = 'X';
		//if (j & TIOCM_ST) status[11] = 'X';
		//if (j & TIOCM_SR) status[14] = 'X';
		//if (j & TIOCM_CTS) status[18] = '7';
		five=(j & TIOCM_CAR)/64; //pin 5 lights up;e
		six=(j & TIOCM_RNG)/128; // pin 6 '' 
		//  {
		//    status[25] = 'X';
		if (five ^ fivelast) { 
			clock_gettime(CLOCK_REALTIME,&time1);
			transitionfive++;
			fives[transitionfive] = 1000000000LL*time1.tv_sec+time1.tv_nsec; // record the edges, for apostiori frequency refinement calculation.
			flag=1; 
			}
		if (six ^ sixlast) {
			if (!flag) {
			        clock_gettime(CLOCK_REALTIME,&time1);
				}
			transitionsix++;
			sixes[transitionsix] =  1000000000LL*time1.tv_sec+time1.tv_nsec;
			flag=1; 
			}
		sixlast = six;
		fivelast = five; 
		jj++;
	  }
	// end of the samples loop.
	// also note that the last value of 'six' oir 'five' sets the polarity -as it were- of the entire pulse sequence, that is, the algorithm must keep track of the fact that the square wave is not at 50% duty cycle. 
	// algorithm to find -and replace- the missing teeth. Assume that one is never missing too many! 
	// replace missing teeth on '5' 
	totfiveteeth = 0; 
	totfivetime = fives[transitionfive]-fives[0]; 
	numerator = 1.*(double)(totfivetime); 
	naiveperiod = 2.*numerator/(double)(transitionfive); // this will be too long, because of missing teeth. But probably not too far off ! 
	for (jj=0; jj<transitionfive-2; jj=jj+2) { 
	  a = (double)(fives[jj+2]-fives[jj])/naiveperiod; 
	  if(a<0) {
	    //	    printf("   %lu   %lu   %1.8e  %5i\n", fives[jj], a, totfiveteeth);
	    printf(" problem fives a<0\n"); 
	  } 
	  totfiveteeth = totfiveteeth + 1 + (int)(a*(1.05)-1);//5% margin due to estimate of shortest interval being too high perhaps. I think that this is OK. 
	}
	totfivetime = fives[jj]-fives[0]; 
	fff = (double)(totfiveteeth)/(double)(totfivetime)*1.e9; // factor of 1.e9 since the totsixtime is in nanoseconds. 
	//printf(" %lu  %1.8e  %1.8e\n", totfivetime, fff, a); 
	// Now replace missing teeth on '6'
	totsixteeth = 0; // actual teeth count. 
	totsixtime = sixes[transitionsix]-sixes[0]; 
	numerator = 1.*(double)(totsixtime); 
	naiveperiod = 2.*numerator/(double)(transitionsix); // this will be too long, because of missing teeth. But probably not too far off ! 
	//printf(" %lu  %5i  %1.8e\n", totsixtime, transitionsix, numerator); 
	for (jj=0; jj<transitionsix-2; jj=jj+2) { 
	  a = (double)(sixes[jj+2]-sixes[jj])/naiveperiod; 
	  if(a<0) {
	    //	    printf("   %lu   %lu   %1.8e  %5i\n", sixes[jj], a, totsixteeth);
	  printf(" problem sixes a<0\n"); 
	  } 
	  totsixteeth = totsixteeth + 1 + (int)(a*(1.05)-1);//5% margin due to estimate of shortest interval being too high perhaps. I think that this is OK. 
	}
	totsixtime = sixes[jj]-sixes[0]; 
	ff = (double)(totsixteeth)/(double)(totsixtime)*1.e9; // factor of 1.e9 since the totsixtime is in nanoseconds. 
	//printf(" %lu  %1.8e  %1.8e\n", totsixtime, ff, a); 
// save for the next round only after correct for the potential problem of lost teeth. For example, could have gotten unlucky and gotten hung up by an interruption during the missing teeth search. Then the last times will be screwed up. Detect anomalously long breaks here and fill in the teeth. Is this what they call a 'hanging bridge'? But, if so, the next samples and missing teeth detection should automatically re-constitute them ! It shouldn't be a problem to just leave these as is for the next loop! 
	fives[0] = fives[transitionfive]; 
	sixes[0] = sixes[transitionsix]; 
// form running average and standard deviation
	if(kk!=1){ 
	  avgff = (avgff*(double)(kk-2) + fff)/(double)(kk-1); 
	  sqf = sqf+fff*fff;
	  outputff = sqrt((sqf-(kk-1)*avgff*avgff)/(kk-2)); 
	  avgf = (avgf*(double)(kk-2) + ff)/(double)(kk-1); 
	  sq = sq+ff*ff;
	  outputr = sqrt((sq-(kk-1)*avgf*avgf)/(kk-2)); 
	  //	  printf("%5i  %1.8e  %1.8e  %1.5e       %1.8e  %1.8e  %1.5e\n", kk-1, fff, avgff, outputff, ff, avgf, outputr); 	
	  printf("%5i  %1.8e  %1.8e\n", kk-1, fff, ff); 	
	} 
	// back to the top of the main loop. 
	}
	//   fclose(outFile);
	return(0); 
}

