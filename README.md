Lab_Linux
=========

This is the github repository for linux data acquisition software that M. Crescimanno has developed


Idea is the fastest route to lab data...typically through the good 'ol RS232 serial port (long live serial ports!) 
Most of the routines use the real-time extensions for timing routines. (use -lrt in compilation) 


fmeteryslow2.c    is the multichannel strip chart recorder software 

onscreen_counter.c       is a simple pulse timestamper...great for reading multiple NaI:Th crystals or GM tubes, and synthetically recording co-incidences

PID5.c            digital non-linear PID code ; Hey you engineers forget what you learned about laplace transforms!!! Work in the time domain and solve the dang DE already... 




... 


