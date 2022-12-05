
/**************************************************

file: ble_scan.c
purpose: Send HCI command to do BLE scan

compile with the command: gcc ble_scan.c rs232.c -Wall -Wextra -o2 -o ble_scan

**************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include "rs232.h"

#define TX 0
#define RX 1
#define BUF_SIZE 4095
#define FULL_BUF_SIZE BUF_SIZE*4

//#define DEBUG

#ifdef DEBUG
#else
#endif

int print_hex(int mode, unsigned char *buf, int size);
int rx_pkt_parser(unsigned char *buf, int size);


struct rx_packet_h{
	unsigned char rxType;
	unsigned char rxEventCode;
  unsigned char rxDataLen;
	unsigned char Event[2];
	unsigned char Status;
};

struct event_cmd_st_h{
	unsigned char OpCode[2];
	unsigned char DataLength;
};

struct event_scn_evnt_rep_h{
  unsigned char EventId[4];
	unsigned char AdvRptEventType;
	unsigned char AddressType;
	unsigned char Address[6];
	unsigned char PrimaryPHY;
	unsigned char SecondaryPHY;
	unsigned char AdvSid;
	unsigned char TxPower;
	unsigned char RSSI;
	unsigned char DirectAddrType;
	unsigned char DirectAddr[6];
	unsigned char PeriodicAdvInt[2];
	unsigned char DataLength[2];
	//unsigned char *DataPtr;
};



int main()
{
  int cport_nr,bdrate,n;
	//cport_nr=0,        /* /dev/ttyS0 (COM1 on windows) */
	//bdrate=9600;       /* 9600 baud */
	cport_nr=39,          /*  (ttyMSM1 : 39) */
	bdrate=115200;       /* 115200 baud */
#ifdef DEBUG
	clock_t t;
#endif
  char mode[]={'8','N','1',0};

  unsigned char buf[BUF_SIZE];
  unsigned char full_buf[FULL_BUF_SIZE];
  int full_buf_ptr = 0;
  unsigned char HCIExt_ResetSystemCmd[] = {0x01, 0x1D, 0xFC, 0x01, 0x00 };
  int HCIExt_ResetSystemCmd_length = 5;

  unsigned char GAP_DeviceInitCmd[] = {0x01, 0x00, 0xFE, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  int GAP_DeviceInitCmd_length = 12;

  unsigned char GapScan_enableCmd[] = {0x01, 0x51, 0xFE, 0x06, 0x00, 0x00, 0xF4, 0x01, 0x28, 0x00 };
  int GapScan_enableCmd_length = 10;

  if(RS232_OpenComport(cport_nr, bdrate, mode, 0))
  {
    printf("Can not open comport\n");

    return(0);
  }

  RS232_flushRXTX(cport_nr);

  // send reset command
#ifdef DEBUG
  t=clock();
  print_hex(TX, HCIExt_ResetSystemCmd, HCIExt_ResetSystemCmd_length);
  t=clock()-t;
  printf("t=%ld\n",t);  //60 
#else
  /* sleep for 60ms */
    usleep(60000);
#endif


  RS232_SendBuf(cport_nr, HCIExt_ResetSystemCmd, HCIExt_ResetSystemCmd_length);
  /* sleep for 1 Second */
#ifdef DEBUG
  t=clock();
#endif
  usleep(1000000);
#ifdef DEBUG
  t=clock()-t;
  printf("CLOCKS_PER_SEC=%ld\n",t);
#endif
  n = RS232_PollComport(cport_nr, buf, BUF_SIZE);

#ifdef DEBUG
  t=clock();
  print_hex(RX, buf, n);
  t=clock()-t;
  printf("t=%ld\n",t);
#else
  /* sleep for 300ms */
    usleep(300000);
#endif



	// send device initial command
#ifdef DEBUG
  t=clock();
  print_hex(TX, GAP_DeviceInitCmd, GAP_DeviceInitCmd_length);
  t=clock()-t;
  printf("t=%ld\n",t);
#else
  /* sleep for 250 ms */
    usleep(250000);
#endif



  RS232_SendBuf(cport_nr, GAP_DeviceInitCmd, GAP_DeviceInitCmd_length);
/* sleep for 0.5 Second */
    usleep(500000);

  n = RS232_PollComport(cport_nr, buf, BUF_SIZE);

#ifdef DEBUG
  t=clock();
  print_hex(RX, buf, n);
  t=clock()-t;
  printf("t=%ld\n",t);
#else
  /* sleep for 500 ms */
    usleep(500000);
#endif


  // send scan command
#ifdef DEBUG
  t=clock();
  print_hex(TX, GapScan_enableCmd, GapScan_enableCmd_length);
  t=clock()-t;
  printf("t=%ld\n",t);
#else
  /* sleep for 30ms */
    usleep(30000);
#endif


  RS232_SendBuf(cport_nr, GapScan_enableCmd, GapScan_enableCmd_length);

	//read scan respone
	while (n > 0)
	{
	/* sleep for 400 mS */
      usleep(400000);

	  n = RS232_PollComport(cport_nr, buf, BUF_SIZE);
#ifdef DEBUG
  t=clock();
  print_hex(RX, buf, n); 
  t=clock()-t;
  printf("t=%ld\n",t);
#endif
		
		if (full_buf_ptr+n>FULL_BUF_SIZE)
		{
#ifdef DEBUG
			printf("buffer full. break.\n");
#endif
			break;
		}
		memcpy(full_buf+full_buf_ptr, buf, n);
		full_buf_ptr+=n;

#ifdef DEBUG
		printf("n:%d, full_buf_ptr:%d\n",n, full_buf_ptr);
#endif
	}
#ifdef DEBUG
  print_hex(RX, full_buf, full_buf_ptr);
#endif
  rx_pkt_parser( full_buf, full_buf_ptr);
#ifdef DEBUG
	printf("n:%d, full_buf_ptr:%d\n",n, full_buf_ptr);
#endif
	RS232_flushRXTX(cport_nr);
	RS232_CloseComport(cport_nr);
  return(0);
}
/**************************************************
Print buffer in HEX
**************************************************/
int print_hex(int mode, unsigned char *buf, int size)
{

	int ii,jj;

	if (mode == TX)
		printf("TX: ");
	else
		printf("RX: ");

	for(ii=0,jj=0; ii < size; ii++,jj++)
	{
		printf("%02X ",buf[ii]);
		if (jj==15)
		{
				printf("\n");
				jj = 0;
		}
	}
	printf("\n");

	return(0);
}


int rx_pkt_parser(unsigned char *buf, int size)
{
	int pkt_index=0;
	int pkt_size=0;
	int temp_event=0;
	int temp_EventId=0;
	int total_device_count=0;
	char szAddress[18];
	struct rx_packet_h *rx_packet;
	struct event_scn_evnt_rep_h *event_scn_evnt_rep;
#ifdef DEBUG
	int dump_i=0;
	unsigned char *pkt_ptr;
#endif
	if(size<=0){printf("size error\n");return -1;}

	printf("BLE scan start:\n");
	rx_packet = (struct rx_packet_h *)(buf);

	while(pkt_index<size)
	{
#ifdef DEBUG
		printf("--------------------------------------------------------------------\n");
		printf("-Type           : 0x%02X (%s)\n",rx_packet->rxType,rx_packet->rxType==0x4?"Event":"Unknown");

		if(rx_packet->rxType!=0x4)
		{
			printf(" Type unknown, rxType:0x%02X, pkt_index:%d\n",rx_packet->rxType,pkt_index);
		}

		printf("-EventCode      : 0x%02X (%s)\n",rx_packet->rxEventCode,rx_packet->rxEventCode==0xff?"HCI_LE_ExtEvent":"Unknown");
		if(rx_packet->rxEventCode!=0xff)
		{
			printf(" EventCode unknown, rxEventCode:0x%02X, pkt_index:%d\n",rx_packet->rxEventCode,pkt_index);
		}

		printf("-Data Length    : 0x%02X (%d) bytes(s)\n",rx_packet->rxDataLen,rx_packet->rxDataLen);
#endif
		temp_event = (rx_packet->Event[1]<<8)+rx_packet->Event[0] ;
#ifdef DEBUG
		printf(" Event          : 0x%02X%02X (%d) ",rx_packet->Event[1],rx_packet->Event[0],temp_event);
		if(temp_event==0x067F)
		{
			printf("(GAP_HCI_ExtentionCommandStatus)\n");
		}
		else if(temp_event==0x0600)
		{
			printf("(GAP_DeviceInitDone)\n");
		}
		else if(temp_event==0x0613)
		{
			printf("(GAP_AdvertiserScannerEvent)\n");
		}
		else 
		{
			printf(" Event unknown, Event:0x%04X, pkt_index:%d\n",temp_event,pkt_index);
		}
		
		printf(" Status         : 0x%02X (%d) (%s)\n",rx_packet->Status,rx_packet->Status,rx_packet->Status==0?"SUCCESS":"FAIL");
#endif

		if(temp_event==0x0613)
		{
			event_scn_evnt_rep = (struct event_scn_evnt_rep_h *)(&(rx_packet->Status) + 1);
			temp_EventId = (event_scn_evnt_rep->EventId[3]<<24) + (event_scn_evnt_rep->EventId[2]<<16) +
			               (event_scn_evnt_rep->EventId[1]<<8) + (event_scn_evnt_rep->EventId[0]) ;
#ifdef DEBUG
			printf(" EventId        : 0x%02X%02X%02X%02X (%d) ", event_scn_evnt_rep->EventId[3],
																																	event_scn_evnt_rep->EventId[2],
																																	event_scn_evnt_rep->EventId[1],
																																	event_scn_evnt_rep->EventId[0],temp_EventId);

			if(temp_EventId==0x00010000)
			{
				printf("(GAP_EVT_SCAN_ENABLED)\n");
			}
			else if(temp_EventId==0x00020000)
			{
				printf("(GAP_EVT_SCAN_DISABLED)\n");
			}
			else if(temp_EventId==0x00400000)
			{
				printf("(GAP_EVT_ADV_REPORT)\n");
			}
			else
			{
				printf(" EventId unknown, EventId:0x%08X, pkt_index:%d\n",temp_EventId,pkt_index);
			}
#endif
			if(temp_EventId==0x00400000)
			{
				sprintf(szAddress,"%02X:%02X:%02X:%02X:%02X:%02X", event_scn_evnt_rep->Address[5],
																																	event_scn_evnt_rep->Address[4],
																																	event_scn_evnt_rep->Address[3],
																																	event_scn_evnt_rep->Address[2],
																																	event_scn_evnt_rep->Address[1],
																																	event_scn_evnt_rep->Address[0]);
#ifdef DEBUG
				printf("%04d", total_device_count);
				printf(" Address        : %s", szAddress);
				printf(" RSSI           : 0x%02X (%d)(%d)",event_scn_evnt_rep->RSSI,event_scn_evnt_rep->RSSI,event_scn_evnt_rep->RSSI-256);
#else
				printf(" Address: %s RSSI: %d", szAddress, event_scn_evnt_rep->RSSI-256);
#endif
				printf("\n");
				total_device_count++;
			}
		}


		pkt_size = 3+rx_packet->rxDataLen;

#ifdef DEBUG
		pkt_ptr = (unsigned char *)rx_packet;
		printf(" <Info   > Dump(Rx):");
		for(dump_i=0; dump_i < pkt_size; dump_i++)
		{
			if (dump_i%16==0)
			{
					printf("\n");
					printf("%04x:",dump_i);
			}
			printf("%02X ",pkt_ptr[dump_i]);
		}
		printf("\n");
#endif
		
		pkt_index+=pkt_size;
#ifdef DEBUG
		printf(" pkt_size:%d, pkt_index:%d\n",pkt_size,pkt_index);
#endif
		rx_packet = (struct rx_packet_h *)(&(rx_packet->rxDataLen) + rx_packet->rxDataLen + 1);

	}
	printf("Total: %d Device found.\n",total_device_count);
	return 0;
}


