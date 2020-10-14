/* SPDX-License-Identifier: BSD-3-Clause */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <websocket/libwebsockets.h>
#include <net/if.h>

#include "command.h"
#include "redirdebug.h"
#include "redirtask.h"
#include "redirclient.h"
#include "redirintercomm.h"
#include "redirmsgdispatch.h"
#include "websocket/libwcf.h"


static int redirInit( void )
{
    if( redirInitInterComm( ) < 0 )
        return( 0 );

    if( redirInitTasks( ) < 0 )
    {
        return( -1 );
    }

    return( 0 );
}

int port_forwarding(char *ipAddress, char *port)
{
    struct addrinfo hints;
    struct addrinfo *result;
    int s;
    int waitForAddrInfo = 1;
    int waitForCnt = 0;
    int taskIds[REDIR_MAX_TASKS];

    memset( taskIds, 0, sizeof( taskIds ) );
    wc_set_socket( );
    redirConfigInit( );

    strcpy( redircfg.address, ipAddress);
    redircfg.port = atoi(port);
    redircfg.use_ssl = 1;
    strncpy(redircfg.ssl_certdir, "/usr/opensync/certs/", sizeof( redircfg.ssl_certdir )-1); /* copy the root directory of the certificate */
    redircfg.ssl_certdir[sizeof( redircfg.ssl_certdir )-1] = '\0';


    optind++;

    memset( &hints, 0, sizeof( struct addrinfo ) );
    hints.ai_family = AF_UNSPEC;                /* Allow IPv4 or IPv6 */
    hints.ai_socktype = 0;                      /* Datagram socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;                      /* Any protocol */
    do
    {
        s = getaddrinfo( redircfg.address, NULL, &hints, &result );
        if( s != 0 )
        {
            sleep( 2 );
            waitForCnt++;
        }
        else
        {
            waitForAddrInfo = 0;
        }
        if( waitForCnt > 1000 )
        {
	    LOG(ERR, "could not resolve address");
            return( -1 );
        }
        if( !result ){
            freeaddrinfo( result );
	}
    } while( waitForAddrInfo );

    // Some general initialization.
    redirInit( );
    createOneTask( (Address)redirClientTask, "redirClient", &taskIds[0] );
    createOneTask( (Address)redirMsgDispatchTaskFunc, "redirMsgDispatcher", &taskIds[1] );
    /* wait for the other tasks to finish */
    waitForOtherTaskesToFinish( );
    return 0 ;
}


