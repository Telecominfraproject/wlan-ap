#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <ctype.h>
#ifndef _DEBUG_
#include <syslog.h>
#else
#define syslog(x,fmt,args...) printf(fmt,## args)
#endif
#include <sys/time.h>



int tmp_cnt = 0;
int led_can_blink =0;
struct itimerval value, ovalue;


struct cmdobj {
	char	        cmd_name[32];
	int            (*cmdfun)(int argc, char**argv);	
};

void wps_led_blink_timer(int signo)
{
	if(led_can_blink){

		if(tmp_cnt%2)
			system("echo 1 > /sys/class/leds/rtl819x\:green\:wps/brightness");
		else
			system("echo 0 > /sys/class/leds/rtl819x\:green\:wps/brightness");

		tmp_cnt ++;
		
	} else {

		value.it_value.tv_sec = 0;
		value.it_value.tv_usec = 0;
		value.it_interval.tv_sec = 0;
		value.it_interval.tv_usec = 0;

	}
	

}


void rtk_wps_led_on(int argc, char**argv)
{
	led_can_blink = 0;
	system("echo 1 > /sys/class/leds/rtl819x\:green\:wps/brightness");

}

void rtk_wps_led_off(int argc, char**argv)
{
	led_can_blink = 0;
	system("echo 0 > /sys/class/leds/rtl819x\:green\:wps/brightness");

}

void rtk_wps_led_blink(int argc, char**argv)
{
	int sec = atoi(argv[2]);

	led_can_blink = 1;
	
	if(!sec)
	sec=1;
	
	if(1){

   		//printf("process id is %d\n", getpid());
   		tmp_cnt = 0;

   		value.it_value.tv_sec = 1;

   		value.it_value.tv_usec = 0;

   		value.it_interval.tv_sec = sec;

   		value.it_interval.tv_usec = 0;

		signal(SIGALRM, wps_led_blink_timer);
   		setitimer(ITIMER_REAL, &value, &ovalue); 

	}

}


struct cmdobj	rtk_wpsled_cmds[] = 
{
    {"on",						rtk_wps_led_on},                                              
    {"off",						rtk_wps_led_off},    
    {"blink",					rtk_wps_led_blink},     
 
    {"LAST",					NULL} //This must put in the end of this struct !!                                                                                       
};


int main(int argc, char *argv[])
{
	int i = 0;
	int (*cmdcallback)(int argc, char**argv) = NULL;
	
	if ( argc > 1 ) {
  		
		while(1) {
			if(!strcmp("LAST", rtk_wpsled_cmds[i].cmd_name))
				break;
		
			if(!strcmp(argv[1], rtk_wpsled_cmds[i].cmd_name)){
				cmdcallback = rtk_wpsled_cmds[i].cmdfun;
				break;			
			}		

			i ++;
		}

		if(cmdcallback) {
			cmdcallback(argc, argv);
			return 0;
		}

	}

	return -1;

}

