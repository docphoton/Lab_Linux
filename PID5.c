//#include <asm/io.h>
#include <sys/io.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <time.h>
#include <float.h>
#include <getopt.h>
#include <libgen.h>
#include <math.h>

//  PID5: Brandon's PID1 updated with the timing routine that replaces missing teeth. 
//    
//   ./PID5.out ttyS0 1000 85 1000  340.54
// 
// which means: control is though port '0' (com1) 1 sec PID cycle, with 85 % max duty 
// cycle and 1 second frequency read (the temperatures) and 340.54 Hz is the set 
// value (frequency) target
// 
// Note that one must compile this is the gcc -O flag (to allow it the 
//	low level access to the port) Actually use 
//   gcc -O PID2.c -o PID2.out  -lrt -L /usr/local/lib -lgsl -lgslcblas -lm -static
//
// The timing segment that replaces the missing teeth is from fmeteryslow2.c
//  	and thus has all the drawbacks and limitations (and advantages!) thereof. 
// Oct. 07: added section to filter the frequency readings for quick changes...
// 	we veto and average any frequency reading that changes too fast. 
// 	We do this by keeping a running average and a running deviation, all
//	based on the t1 timescale. If the next frequency reading is outside the 
//	2\sigma of the running average I consider it too polluted to use, and fold it 
// 	into the running average and use the running average. 
//   In this way the filtering is adaptive. 
const int comms[] = {0x3f8, 0x2f8, 0x3e8, 0x2e8};
  
int interrupted = 0;
   
/* signal handler, traps ctrl-c commands so that output is not lost */
void handler(int sig){

   printf("Interrupt detected. Saving data and exiting...\n");
   interrupted = 1;
   return;
   
}

int main(int argc, char *argv[]) 
{
  double freq5, freq6, setpoint, dutyCycle, dutyCycleMax, dutyCyclelast, changelimit, sec_per_bin, k_p, k_diff, k_int,  t1, error, error_diff, error_diff_smooth, error_integ, errorlast, error_avg, excursion2, error_square, error_sigma2,capture_region, a, numerator, naiveperiod;  
  int len=410000, maxfreq = 50000, freq, flag, port,i,junk, fd, up,down, transitionfive, transitionsix, pulsesfive[len], pulsessix[len], jj, kk, counter, five, six, samples,j, sixlast, fivelast, num, totsixteeth, totfiveteeth, mode;
  unsigned long long il, jl, fives[maxfreq], sixes[maxfreq], totfivetime, totsixtime; 
	struct stat sb;
	struct timespec time0, time1; 
	char devname[20]="/dev/";
	char status[40], startstring, stopstring;
	time_t start, stop;  /* time stuff not currently working! */  
	clock_t tm;          /* need to fix the time stuff */
  //char device[] = "/dev/ttyS0";
  //port = comms[atoi(argv[1])];
  //device[9] = argv[1][0];
	if (strcmp(argv[1], "-ttytab") == 0) {
    		fd = 0;		/* terminal has already been opened as fd 0 */
					/* This is for experimental support of SunOS */
	} else  {
		strcat(devname, argv[1]);
		if ((fd = open(devname, O_RDWR)) < 0) {
                        fprintf(stderr, "%s ", devname);
			perror("error");
			exit(1);
		}
	}
   freq = atoi(argv[2]);
   dutyCycle = atoi(argv[3]);
   dutyCycleMax = dutyCycle/100; //duty cycle max; so it doesn't catch fire ;> 
   dutyCycle= .08; // sets initial duty cycle. 
   samples = atoi(argv[4]); 
   setpoint = atoi(argv[5]); 
   //fprintf(stderr, "%d, %f\n", freq, dutyCycle);
   //fd = open(device, O_RDONLY | O_NOCTTY | O_NDELAY);
   // (fd == -1) 
   //{
   //	perror(device);
   //	exit(1);	
   //}
   //se fcntl(fd, F_SETFL, 0);
   ioctl(fd, TIOCMGET, &junk);
   up = junk | TIOCM_RTS; 
   up = up | TIOCM_DTR;
   down = junk & ~TIOCM_RTS; 
   down = down & ~TIOCM_DTR;
   ioperm(comms[3], 1 ,1);
	/* Get terminal modes */
   i = TIOCM_DTR;
   ioctl(fd, TIOCMSET, &i);
   //fprintf(stderr, "status = 0x%x, up = 0x%x, down = 0x%x", status, up, down); 
   printf("-----------------------------------------------------\n");
   printf("Wecome to PID2: columns are freq1, freq2, setpoint, dutyCycle \n" ); 
   printf("-----------------------------------------------------\n");
	// setup for the main loop   
	counter = 0;
	samples = 400*samples; 
	// main loop: PID parameters here. 
	k_p = 1.5; 
	k_diff = k_p; 
	k_int = 3.0;
	t1=120; // timescale for the heating delay. 	
        capture_region = .02;  
	changelimit = .02; 
	//  main loop : initializations. 
	errorlast=0.; 
	error_diff_smooth=0;
	error_integ=0.;
	error_square=0.; 
	num = 0; 
	while(1) 
	  {
       // fmetery segment: read the frequencies on the two input line	    
	    kk++;
	    jj = 0;
 	if (ioctl(fd, TIOCMGET, &j) < 0) {   //section to set fivelast and sixlast. 
			perror("Getting hardware status bits");
			exit(1);
		}
	        //if (j & TIOCM_LE) status[0] = 'X'; 
		//if (j & TIOCM_DTR) status[4] = 'X';
		//if (j & TIOCM_RTS) status[8] = 'X';
		//if (j & TIOCM_ST) status[11] = 'X';
		//if (j & TIOCM_SR) status[14] = 'X';
		//if (j & TIOCM_CTS) status[18] = '7';
		fivelast=(j & TIOCM_CAR)&&1; //pin 5 lights up;e
		sixlast=(j & TIOCM_RNG)&&1; // pin 6 '' 
	    transitionsix = 0;
	    transitionfive = 0;
	clock_gettime(CLOCK_REALTIME,&time0);
	sixes[0] = 1000000000LL*time0.tv_sec+time0.tv_nsec; // note that this is garbage, since there is no line transition there !! 
	while(jj<samples) // sample to get the average fast freqeuncy wrt the computer clock
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
		five=(j & TIOCM_CAR)&&64; //pin 5 lights up;e
		six=(j & TIOCM_RNG)&&128; // pin 6 '' 
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
	totfiveteeth = 0; // actual teeth count. 
	totfivetime = fives[transitionfive]-fives[1]; // sixes[0] is garbarge, so use [1] for a guess. 
	numerator = 1.*(double)(totfivetime); 
	naiveperiod = 2.*numerator/(double)(transitionfive-1); // this will be too long, because of missing teeth. But probably not too far off ! 
	//printf(" %lu  %5i  %1.8e\n", totsixtime, transitionsix, numerator); 
	for (jj=1; jj<transitionfive-3; jj=jj+2) {  // start from 1, not 0, since 0 is junk...that is, since we are doing a long heating/cooling duty cycle after this section, I cannot use the temporal carryover that I did for fmeteryslow2 which read the frequencies continuously. 
	  a = (double)(fives[jj+2]-fives[jj])/naiveperiod; 
	  if(a<0) {
	    printf("problem at  %lu   %lu   %1.8e  %5i\n", fives[jj], a, totfiveteeth);
	  } 
	  totfiveteeth = totfiveteeth + 1 + (int)(a*(1.05)-1);//5% margin due to estimate of shortest interval being too high perhaps. I think that this is OK. 
	}
	totfivetime = fives[jj]-fives[1]; 
	freq5 = (double)(totfiveteeth)/(double)(totfivetime)*1.e9; // factor of 1.e9 since the totsixtime is in nanoseconds. 
	// replace missing teeth on '6'
	totsixteeth = 0; // actual teeth count. 
	totsixtime = sixes[transitionsix]-sixes[1]; // sixes[0] is garbarge, so use [1] for a guess. 
	numerator = 1.*(double)(totsixtime); 
	naiveperiod = 2.*numerator/(double)(transitionsix-1); // this will be too long, because of missing teeth. But probably not too far off ! 
	//printf(" %lu  %5i  %1.8e\n", totsixtime, transitionsix, numerator); 
	for (jj=1; jj<transitionsix-3; jj=jj+2) {  // start from 1, not 0, since 0 is junk...that is, since we are doing a long heating/cooling duty cycle after this section, I cannot use the temporal carryover that I did for fmeteryslow2 which read the frequencies continuously. 
	  a = (double)(sixes[jj+2]-sixes[jj])/naiveperiod; 
	  if(a<0) {
	    printf("problem at  %lu   %lu   %1.8e  %5i\n", sixes[jj], a, totsixteeth);
	  } 
	  totsixteeth = totsixteeth + 1 + (int)(a*(1.05)-1);//5% margin due to estimate of shortest interval being too high perhaps. I think that this is OK. 
	}
	totsixtime = sixes[jj]-sixes[1]; 
	freq6 = (double)(totsixteeth)/(double)(totsixtime)*1.e9; // factor of 1.e9 since the totsixtime is in nanoseconds. 
	// printf(" %lu  %1.8e  %1.8e\n", totsixtime, ff, a); 
// save for the next round only after correct for the potential problem of lost teeth. For example, could have gotten unlucky and gotten hung up by an interruption during the missing teeth search. Then the last times will be screwed up. Detect anomalously long breaks here and fill in the teeth. Is this what they call a 'hanging bridge'? But, if so, the next samples and missing teeth detection should automatically re-constitute them ! It shouldn't be a problem to just leave these as is for the next loop! 
	fives[0] = fives[transitionfive]; 
	sixes[0] = sixes[transitionsix]; 
	//---------------------------------------------------------------------
	// Decision making area, percent error, running average,and running sigma. 
	error = (setpoint-freq6)/setpoint;
// INTERGRATION: here implemented so there is no windup ! 
	error_integ = (1.-1./t1)*error_integ+error;
	error_avg = error_integ/t1;  // average error of last t1 recordings.
	excursion2 = (error-error_avg)*(error-error_avg);  
	error_square = (1.-1./t1)*error_square+excursion2; 
	error_sigma2 = error_square/t1; 
	// veto area: If the one-time measurement 'error' is more than 3 sigma from 
	// 	the running average, I ignore it and use instead the running average. 
	//	this kills the spurious high frequency added by sporadic measurement 
	// 	noise. 
	if ((excursion2>3.*error_sigma2) && (num>2*t1)) { 
		error = error_avg; 		
		} 	
	//else { error=0.; } 	
	if (error < capture_region) {
		dutyCycle = k_int*error_integ;
		mode=3;
	} else { 
	// PROPORTIONAL WINDOW
	  dutyCycle = k_p*error;	  
	  mode=2;
	} 
	// DIFFERENTIATION
	error_diff = (error-errorlast); // this could be too noisy...so smooth it! 
	error_diff_smooth = ((1.-1./t1)*error_diff_smooth+error_diff/t1);	
	    // here implement a kind of Newton's method to update the dutycycle if the previous determination of it has it changing too fast. 
	if ((error/error_diff_smooth < t1) && (error/error_diff_smooth > 0.) ) {
	  dutyCycle = k_diff*t1*error_diff_smooth; 
	  mode=1;
	} 
	// HEATING ONLY, this case...make sure dutyCycle positive
        dutyCycle = dutyCycle+fabs(dutyCycle); // zeros it out if negative...do nothing! 
	errorlast = error; 	
// GOVENOR! no  fast moves...
        if ((fabs(dutyCycle-dutyCyclelast)>changelimit*dutyCycle)&&(dutyCyclelast!=0.0)){
	  dutyCycle = dutyCyclelast*(1.0+changelimit*copysign(1.0, dutyCycle-dutyCyclelast)); 
	  mode = -mode; 
	} 
	dutyCyclelast= dutyCycle; 
// GOVENOR! upper and lower windows... 	
	if (dutyCycle > dutyCycleMax) { 
	  dutyCycle = dutyCycleMax; 	// don't burn the thing up! 
	} 
	if (dutyCycle < .05) { 
	  dutyCycle = 0; 		// Save the relay ! 
	} 
	// REPORTING
	num++; 
//	printf("%5i %6e  %6e  %6e  %6e   %6e    %6e\n", num, freq5, freq6, setpoint, dutyCycle, error_integ, error_diff_smooth); 
	printf("%4.3f  %3.3f  %2.3f %2i\n", (float)(freq5), (float)(setpoint-freq6), (float)(100.0*dutyCycle), mode); 
	// ----------------end of decision making area ------------------------
	// PULSING SECTION FOR ADDING (SUBTRACTING ) HEAT 
	ioctl(fd, TIOCMSET, &up);
	//ioctl(fd, TIOCMGET, &status);
	//fprintf(stderr, "status is now 0x%x\n", status);
	usleep((int)(1000.*freq*dutyCycle));
	  //for (i=1; i < dutyCycle*1000000/freq; i++) {
	   // ioctl(fd, TIOCMGET, &status);
	   // fprintf(stderr, "0x%x\n", status);
	   // fprintf(stderr, "On: %d\n", i);
           //outb(0, comms[3]);
	  //}
	ioctl(fd, TIOCMSET, &down);
	//ioctl(fd, TIOCMGET, &status);
	//fprintf(stderr, "status is now 0x%x\n", status);
	usleep((int)(1000.*freq*(1-dutyCycle))); 
	  //for (i=(dutyCycle*1000000/freq); i < 1000000/freq; i++) {
	   // ioctl(fd, TIOCMGET, &status);
	   // fprintf(stderr, "0x%x\n", status);
	   // fprintf(stderr, "Off: %d\n", i);
	   //outb(0, comms[3]);
	  //}
	  }
   return 0;
}
