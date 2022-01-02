#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/if_ether.h> 
#include <arpa/inet.h>
#include <signal.h>
#include <netinet/ether.h>
#include <time.h>

#include "pcap/pcap.h"


/***************************************
	Global Variable 
***************************************/

#define PCAP_DEBUG 
#ifdef PCAP_DEBUG
#define pcap_printf(fmt,...)  do{printf("PCAP: ");printf(fmt,##__VA_ARGS__);}while(0)
#else
#define pcap_printf(fmt,...) 
#endif

struct rtl_wifi_header{
	unsigned char 	frame_type;			// Frame type
	unsigned char 	sub_frame_type;		//mac subframe type => BEACON_FRAME 	0x80, PROBE_RESP 0x50
	unsigned char   rssi; 				//rssi.
	unsigned char   wi_add1[6];			//DA mac addr
	unsigned char   wi_add2[6];			//SA mac addr
	unsigned char  	data_rate;			//rx rate
	unsigned char 	channel_num;		//present channel num
}__attribute__((packed));

#define ASSOCIATION_REQUEST 	0x00	
#define ASSOCIATION_RESPONSE 	0x10    
#define REASSICIATION_REQUEST   0x20   
#define REASSICIATION_RESPONSE  0x30   
#define PROBE_REQUEST 			0x40	
#define PROBE_RESPONSE 			0x50	
#define BEACON_FRAME 			0x80	
#define DIASSOCIATION 			0xA0	
#define AUTHENTICAION 			0xB0	
#define DEAUTHENTICAION 		0xc0	

#define MANAGEMENT_FRAME 0x00
#define DATA_FRAME		 0x08

pcap_t *descr = NULL;
/***************************************
			Function 
***************************************/
 
void macSubFrameType(struct rtl_wifi_header *rh)
{
	//printf("rh->sub_frame_type = %x\n", rh->sub_frame_type);
	switch(rh->sub_frame_type)
	{
		case ASSOCIATION_REQUEST:
			pcap_printf("SubType: ASSOCIATION_REQUEST\n");
			break;
		case ASSOCIATION_RESPONSE:
			pcap_printf("SubType: ASSOCIATION_RESPONSE\n");
			break;
		case REASSICIATION_REQUEST:
			pcap_printf("SubType: REASSICIATION_REQUEST\n");
			break;
		case REASSICIATION_RESPONSE:
			pcap_printf("SubType: REASSICIATION_RESPONSE\n");
			break;
		case PROBE_REQUEST:
			pcap_printf("SubType: PROBE_REQUEST\n");
			break;
		case PROBE_RESPONSE:
			pcap_printf("SubType: PROBE_RESPONSE\n");
			break;
		case BEACON_FRAME :
			pcap_printf("SubType: BEACON \n");
			pcap_printf("Present Channel Num: %u \n", rh->channel_num);
			break;
		case DIASSOCIATION:
			pcap_printf("SubType: DIASSOCIATION\n");
			break;
		case AUTHENTICAION:
			pcap_printf("SubType: AUTHENTICAION\n");
			break;
		case DEAUTHENTICAION:
			pcap_printf("SubType: DEAUTHENTICAION\n");
			break;
		default:
			pcap_printf("Unknown Subtype\n");
			break;
	}
}

void macFrameType(struct rtl_wifi_header *rh)
{
	//printf("rh->frame_type = %x\n", rh->frame_type);
	switch(rh->frame_type)
	{
		case MANAGEMENT_FRAME:
			pcap_printf("FrameType: Management Frame\n");
			macSubFrameType(rh);
			break;
		case DATA_FRAME:
			pcap_printf("FrameType: Data Frame\n");
			pcap_printf("SubType: none\n");
			break;
		default:
			pcap_printf("Unknown Frametype\n");
			break;
	}
}

void processPacket(u_char *argu, const struct pcap_pkthdr* pkthdr, const u_char* packet)
{
	struct rtl_wifi_header *rh = (struct rtl_wifi_header* )packet;
	char mac[32] = {0};  
	static int count = 0;
	
	pcap_printf("Packet Count: %d\n", ++count);
	macFrameType(rh);
 	pcap_printf("RX Data Rate: %d bps\n", rh->data_rate);
	pcap_printf("RSSI: %d dBm\n", (-100)+rh->rssi);		
	
	sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",  
                        (unsigned char)rh->wi_add1[0],  
                        (unsigned char)rh->wi_add1[1],  
                        (unsigned char)rh->wi_add1[2],  
                        (unsigned char)rh->wi_add1[3],  
                        (unsigned char)rh->wi_add1[4],  
                        (unsigned char)rh->wi_add1[5]); 
	pcap_printf("DA: %s\n", mac);  

	sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",  
                        (unsigned char)rh->wi_add2[0],  
                        (unsigned char)rh->wi_add2[1],  
                        (unsigned char)rh->wi_add2[2],  
                        (unsigned char)rh->wi_add2[3],  
                        (unsigned char)rh->wi_add2[4],  
                        (unsigned char)rh->wi_add2[5]); 
	pcap_printf("SA: %s\n", mac);  
	
    pcap_printf("Packet Info End:\n\n");	
	return;
}

void terminal_process(int signum)
{
	printf("Terminal Capture Process\n");
	pcap_breakloop(descr);
	pcap_close(descr);
	descr = NULL;
}

int main(int argc, char **argv)
{
    char errbuf[PCAP_ERRBUF_SIZE];   

    descr = pcap_create(argv[1],errbuf);   		//depends on wlan interface
    if (descr == NULL)
    {
		printf("pcap_create failed\n");
    	return (1); 
    }
    if(pcap_set_rfmon(descr,1)==0 )
  		printf("monitor mode enabled\n");

    pcap_set_snaplen(descr, 2048);  	
    pcap_set_promisc(descr, 1);      	 
    
    int status = pcap_activate(descr);
	if(status < 0)
    	printf("WLAN status = %d\n",status);

    //int dl = pcap_datalink(descr);
    //printf("The Data Link Type = %s\n", pcap_datalink_val_to_name(dl));

	signal(SIGINT, terminal_process);    
    pcap_loop(descr, -1, processPacket, NULL); 
    return 0;
}

