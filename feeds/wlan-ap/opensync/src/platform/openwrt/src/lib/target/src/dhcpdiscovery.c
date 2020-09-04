/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <net/if.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/ioctl.h>
#include "target.h"
#include "dhcp_opt.h"
#include "dhcpdiscovery.h"
//#include "wc-dhcpdiscoveryDebug.h"

#define BUF_SIZ 256*256
#define MESSAGE_NOT_RECEIVED -1
#define WAIT_TIMEOUT_SECONDS 30

#define DISCOVER 0x01
#define OFFER 0x02
#define REQUEST 0x03
#define ACK_REQUEST 0x05
#define NACK_REQUEST 0x06
#define RELEASE 0x07

#define TRUE 1
#define FALSE 0

int offset=0;
void addpacket(char *pktbuf,char *msgbuf,int size) {
   memcpy(pktbuf+offset,msgbuf,size);
   offset+=size;
}

struct sockaddr_in dhcp_to;
int _serveripaddress;

struct Answer_t
{
   int messageType;
   char offeredIP[255];
   char serverIP[255];
   char primaryDNS[255];
   char secondaryDNS[255];
};

/**
 * Use to setup the DHCP client
 */
int dhcp_setup(const char * serveripaddress, const char * device)
{
   struct hostent *hostent;
   const int flag = 1;
   struct sockaddr_in name;
   struct ifreq ifr;
   int dhcp_socket;

   /* We populate the interface info */
   memset(&ifr, 0, sizeof(ifr));
   snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s\n", device);

   /*
    * setup sending socket
    */
   if ((hostent=gethostbyname(serveripaddress))==NULL) {
      return 0;
   }

   dhcp_to.sin_family=AF_INET;
   bcopy(hostent->h_addr,&dhcp_to.sin_addr.s_addr,hostent->h_length);

   _serveripaddress=ntohl(dhcp_to.sin_addr.s_addr);

   dhcp_to.sin_port=htons(67);

   if ((dhcp_socket=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP))==-1)
   {
      return 0;
   }

   if (setsockopt (dhcp_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&flag, sizeof flag) < 0) {
      return 0;
   }

   if (setsockopt(dhcp_socket,SOL_SOCKET,SO_BROADCAST,(char *)&flag, sizeof flag) < 0) {
      return 0;
   }

   name.sin_family = AF_INET;
   name.sin_port = htons(68);
   name.sin_addr.s_addr = INADDR_ANY;

   memset (name.sin_zero, 0, sizeof (name.sin_zero));

   if (bind (dhcp_socket, (struct sockaddr *)&name, sizeof name) < 0) {
      return 0;
   }

   return dhcp_socket;
}

void parseDhcpMessage(unsigned char *buffer,int size, struct Answer_t * response)
{
   int j;
   unsigned char serveridentifier[4];

   sprintf(response -> offeredIP, "%d.%d.%d.%d", buffer[16],buffer[17],buffer[18],buffer[19]);

   j=236;
   j+=4;	/* cookie */
   while (j<size && buffer[j]!=255 && buffer[j] != 0)
   {
      switch (buffer[j]) {
      case 53:
         response -> messageType = buffer[j+2];
         break;

      case 54:
         memcpy(serveridentifier,buffer+j+2,4);
         sprintf(response -> serverIP, "%d.%d.%d.%d",
               serveridentifier[0],serveridentifier[1],
               serveridentifier[2],serveridentifier[3]);

         break;
      case 6: // DNS server
      {
         int i=0;

         for(i=0; i<buffer[j+1]; i+=4)
         {
            char dnsServer[256];

            /* We keep a string of this */
            sprintf(dnsServer,"%d.%d.%d.%d", buffer[j+2+i],buffer[j+3+i],buffer[j+4+i],buffer[j+5+i]);

            if(i == 0)
            {
               strcpy(response -> primaryDNS, dnsServer);
            }
            else
            {
               strcpy(response -> secondaryDNS, dnsServer);
            }
         }
         break;
      } // case DNS server

      default:
         break;
      }

      /*
	// This might go wrong if a mallformed packet is received.
	// Maybe from a bogus server which is instructed to reply
	// with invalid data and thus causing an exploit.
	// My head hurts... but I think it's solved by the checking
	// for j<size at the begin of the while-loop.
       */
      j+=buffer[j+1]+2;
   }
}




void send_dhcp_packet(int dhcp_socket,int type, const char *ipaddr,const char *opt50,const char *gwaddr, const char *hardware) {
   static time_t l=0;
   char msgbuf[BUF_SIZ];
   char pktbuf[BUF_SIZ];
   int ip[4],gw[4],hw[16],ip50[4];
   int hwcount;

   sscanf(ipaddr,"%d.%d.%d.%d",&ip[0],&ip[1],&ip[2],&ip[3]);
   sscanf(gwaddr,"%d.%d.%d.%d",&gw[0],&gw[1],&gw[2],&gw[3]);
   if (opt50)
      sscanf(opt50,"%d.%d.%d.%d",&ip50[0],&ip50[1],&ip50[2],&ip50[3]);
   memset(&hw,0,sizeof(hw));
   hwcount=sscanf(hardware,"%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x",
         &hw[0],&hw[1],&hw[2],&hw[3],
         &hw[4],&hw[5],&hw[6],&hw[7],
         &hw[8],&hw[9],&hw[10],&hw[11],
         &hw[12],&hw[13],&hw[14],&hw[15]);

   memset(msgbuf,0,sizeof(msgbuf));
   sprintf(msgbuf,"\1\1%c%c",hwcount,0);
   addpacket(pktbuf,msgbuf,4);

   /* xid */
   if (l>time(NULL))
      l++;
   else
      l=time(NULL);
   memcpy(msgbuf,&l,4);
   addpacket(pktbuf,msgbuf,4);

   /* secs */
   memset(msgbuf,0,2);
   addpacket(pktbuf,msgbuf,2);

   /* Broacast flag */
   unsigned short int flag = htons(0/*32768*/);
   memcpy(msgbuf, &flag, 2);
   addpacket(pktbuf,msgbuf,2);

   /*  sprintf(msgbuf,"%c%c",0x80,0x00); */
   /*  sprintf(msgbuf,"%c%c",0x00,0x00); */
   /*  addpacket(pktbuf,msgbuf,2); */

   /* ciaddr. We don't assume we're the client's ip until it's confirmed */
   memset(msgbuf,0,4);
   if(type != REQUEST)
   {
      sprintf(msgbuf,"%c%c%c%c",ip[0],ip[1],ip[2],ip[3]);
   }
   addpacket(pktbuf,msgbuf,4);

   /* yiaddr */
   memset(msgbuf,0,4);
   addpacket(pktbuf,msgbuf,4);

   /* siaddr */
   memset(msgbuf,0,4);
   addpacket(pktbuf,msgbuf,4);

   /* giaddr */
   sprintf(msgbuf,"%c%c%c%c",gw[0],gw[1],gw[2],gw[3]);
   addpacket(pktbuf,msgbuf,4);

   /* chaddr */
   sprintf(msgbuf,"%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
         hw[0],hw[1],hw[2],hw[3],hw[4],hw[5],hw[6],hw[7],
         hw[8],hw[9],hw[10],hw[11],hw[12],hw[13],hw[14],hw[15]);
   addpacket(pktbuf,msgbuf,16);

   /* sname */
   memset(msgbuf,0,64);
   addpacket(pktbuf,msgbuf,64);

   /* file */
   memset(msgbuf,0,128);
   addpacket(pktbuf,msgbuf,128);

   /* options */
   {
      /* cookie */
      sprintf(msgbuf,"%c%c%c%c",99,130,83,99);
      addpacket(pktbuf,msgbuf,4);

      /* dhcp-type */
      sprintf(msgbuf,"%c%c%c",53,1,type);
      addpacket(pktbuf,msgbuf,3);

      /* Not for inform */
      if (type!=8)
      {
         if(type == REQUEST)
         {
            // We need to put the server we're requesting from
            sprintf(msgbuf,"%c%c%c%c%c%c",54,4,ip[0],ip[1],ip[2],ip[3]);
            addpacket(pktbuf,msgbuf,6);
         }

         /* requested IP address */
         if (opt50)
         {
            sprintf(msgbuf,"%c%c%c%c%c%c",50,4,ip50[0],ip50[1],ip50[2],ip50[3]);
            addpacket(pktbuf,msgbuf,6);
         }

         //			/* server-identifier */
         //			if (serveridentifier[0])
         //			{
         //				sprintf(msgbuf,"%c%c%c%c%c%c",54,4,
         //						serveridentifier[0],serveridentifier[1],
         //						serveridentifier[2],serveridentifier[3]);
         //				addpacket(pktbuf,msgbuf,6);
         //			}
      }

      /* client-identifier */
      //	sprintf(msgbuf,"%c%c%c%c%c%c%c%c",61,6,
      //		hw[0],hw[1],hw[2],hw[3],hw[4],hw[5]);
      //	addpacket(pktbuf,msgbuf,8);

      /* parameter request list */
      if (type==8 || type == 1 || type == REQUEST)
      {
         sprintf(msgbuf,"%c%c%c%c",55,2,0x01,0x06);
         addpacket(pktbuf,msgbuf,4);
      }

      /* If we're discovering */
      if(type == 1)
      {

      }

      /* end of options */
      sprintf(msgbuf,"%c",255);
      addpacket(pktbuf,msgbuf,1);
   }

   //dhcp_dump(pktbuf,offset);

   sendto(dhcp_socket,pktbuf,offset,0,(struct sockaddr *)&dhcp_to,sizeof(dhcp_to));

   offset=0;
}


int dhcp_read(int dhcp_socket, int serverIP, struct Answer_t * response)
{
   unsigned char msgbuf[BUF_SIZ];
   struct sockaddr_in fromsock;
   socklen_t fromlen=sizeof(fromsock);
   int addr;
   int i;

   i=recvfrom(dhcp_socket,msgbuf,BUF_SIZ,0,(struct sockaddr *)&fromsock,&fromlen);
   addr=ntohl(fromsock.sin_addr.s_addr);

   if (serverIP!=addr)
   {
      fprintf(stderr,"received from %d.%d.%d.%d, expected from %d.%d.%d.%d\n",
            ( addr >> 24 ) & 0xFF, ( addr >> 16 ) & 0xFF,
            ( addr >>  8 ) & 0xFF, ( addr       ) & 0xFF,
            ( serverIP >> 24 )&0xFF,(serverIP >> 16 )&0xFF,
            ( serverIP >>  8 )&0xFF,(serverIP)&0xFF
      );
   }

   parseDhcpMessage(msgbuf,i, response);

   return response -> messageType;
}


/**
 * Will return the Message Type received
 */
int waitForResponse(
      int dhcp_socket,
      int serverIP,
      long maxWaitInSecs,
      int expectedResponse,
      struct Answer_t * response)
{
   fd_set read;
   struct timeval timeout;
   int foundpacket=0;
   //const char * returnValue = NULL;

   while (foundpacket != expectedResponse)
   {
      FD_ZERO(&read);
      FD_SET(dhcp_socket,&read);
      timeout.tv_sec=maxWaitInSecs;
      timeout.tv_usec=0;
      if(select(dhcp_socket+1,&read,NULL,NULL,&timeout)<0) {
         return 0;
      }
      if (FD_ISSET(dhcp_socket,&read))
      {
         // If a expected packet was found, then also release it.
         if ((foundpacket=dhcp_read(dhcp_socket, serverIP, response)) == expectedResponse)
         {
            return expectedResponse;
         }
      }
      else
      {
         return MESSAGE_NOT_RECEIVED;
      }
   }

   return MESSAGE_NOT_RECEIVED;

}

const char * getMACAddress(const char * interface) {
   struct ifreq s;
   char * returnValue = NULL;
   int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

   strcpy(s.ifr_name, interface);

   if (0 == ioctl(fd, SIOCGIFHWADDR, &s))
   {
      returnValue = malloc(16*3);
      memset(returnValue, 0x00, 16*3);

      sprintf(returnValue,"%.2X:%.2X:%.2X:%.2X:%.2X:%.2X",
            s.ifr_addr.sa_data[0],s.ifr_addr.sa_data[1],s.ifr_addr.sa_data[2],
            s.ifr_addr.sa_data[3],s.ifr_addr.sa_data[4],s.ifr_addr.sa_data[5]);
   }

   close(fd);

   return returnValue;

}

const char * getDeviceIP(const char * interface) {

   char * returnValue = NULL;
   struct ifreq ifr;
   int fd = socket(AF_INET, SOCK_DGRAM, 0);

   /* I want to get an IPv4 IP address */
   ifr.ifr_addr.sa_family = AF_INET;

   /* I want IP address attached to the interface */
   strncpy(ifr.ifr_name, interface, IFNAMSIZ-1);

   if(0 == ioctl(fd, SIOCGIFADDR, &ifr))
   {
      returnValue = malloc(256);
      int addr = ((struct sockaddr_in *)(&ifr.ifr_addr))->sin_addr.s_addr;
      sprintf(returnValue, "%d.%d.%d.%d", (addr & 0xFF), (addr >> 8 & 0xFF), (addr >> 16 & 0xFF), (addr >> 24 & 0xFF));
   }

   close(fd);

   return returnValue;
}


bool generateQuery(const char *ifname)
{
   bool rc = false;
   const char * device = ifname;
   const char * serverIP = "255.255.255.255";
   const char * mac_address = getMACAddress(device);
   struct Answer_t response;

   int dhcp_socket = dhcp_setup(serverIP, device);

   if( dhcp_socket != 0)
   {
      // We sent a DISCOVER
      send_dhcp_packet(dhcp_socket, DISCOVER,"0.0.0.0",NULL,"0.0.0.0",mac_address);

      if(waitForResponse(dhcp_socket, _serveripaddress, WAIT_TIMEOUT_SECONDS, OFFER, &response) == OFFER /* DHCP TYPE: Offer */)
      {
         /* We close our initial socket (which is configured for broadcasting) */
         close(dhcp_socket);

         /* We open a direct connection (as opposed to broadcasted one) */
         dhcp_socket = dhcp_setup(response.serverIP, device);

         /* We send a request */
         send_dhcp_packet(dhcp_socket, REQUEST /* Request */, response.serverIP, response.offeredIP, "0.0.0.0", mac_address);

         if(waitForResponse(dhcp_socket, _serveripaddress, WAIT_TIMEOUT_SECONDS, ACK_REQUEST, &response) == ACK_REQUEST /* DHCP TYPE: ACK */)
         {
		rc = true;
         }
      }
      else
      {
                close(dhcp_socket);
		rc = false;
      }
   }

   if(mac_address)
   {
      free( (void*) mac_address);
   }

   return rc;
}
