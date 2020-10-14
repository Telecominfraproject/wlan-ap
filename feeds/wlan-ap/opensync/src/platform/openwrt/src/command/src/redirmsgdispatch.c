/* SPDX-License-Identifier: BSD-3-Clause */

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
#include <websocket/libwebsockets.h>

#include "redirmessagedef.h"
#include "redirclient.h"
#include "redirintercomm.h"
#include "redirmsgdispatch.h"

msgDispData_t gmsgdd;

pthread_mutex_t state_machine_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t state_machine_cv = PTHREAD_COND_INITIALIZER;

int hasCloudMsg = 0;
pthread_mutex_t waitForMessageResp_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t waitForMessageResp_cv = PTHREAD_COND_INITIALIZER;

extern struct lws * redir_wsi[MAX_REDIR_CLIENTS];
extern int RedirWsTxErr;

extern int redirAddOneTxBuffToPool( unsigned char * Buffer, unsigned int len );
extern int redirSendNotification( redirNotify_t * cNotify );

int RedirSockFd = -1;


int redirOpenRedirSocket( unsigned short Port )
{
    struct sockaddr_in serv_addr;
    int nn;
    char RedirBuf[50];

    RedirSockFd = socket( AF_INET, SOCK_STREAM, 0 );
    if( RedirSockFd < 0 )
    {
        RedirSockFd = -1;
        return( -1 );
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons( Port );
    serv_addr.sin_addr.s_addr = htonl( INADDR_LOOPBACK );
    if( connect( RedirSockFd, (struct sockaddr *)&serv_addr, sizeof( serv_addr ) ) < 0 )
    {
        RedirSockFd = -1;
        return( -1 );
    }
    sprintf( RedirBuf, "connected_to_CE_port:%d", 22 );
    nn = lws_write( redir_wsi[0], (unsigned char *)RedirBuf, strlen( RedirBuf ), redircfg.write_options );
    if( nn < 0 )
        RedirWsTxErr = 1;
    return( 0 );
}


void redirCloseRedirSocket( void )
{
    if( RedirSockFd != -1 )
    {
        close( RedirSockFd );
        RedirSockFd = -1;
    }
}


void redirMessageRedirect( unsigned char * Msg, size_t Len )
{
    if( RedirSockFd == -1 )
        return;
    write( RedirSockFd, Msg, Len );
}


static int redirWaitForWebSocketToEstablish( )
{
    pthread_mutex_lock( &state_machine_mutex );
    if( !redirst.wsIsReady )
        pthread_cond_wait( &state_machine_cv, &state_machine_mutex );

    pthread_mutex_unlock( &state_machine_mutex );

    return( 0 );
}

int redirNotifyWebSocketIsReady( )
{
    pthread_mutex_lock( &state_machine_mutex );
    redirst.wsIsReady = 1;

    pthread_cond_signal( &state_machine_cv );
    pthread_mutex_unlock( &state_machine_mutex );

    return( 0 );
}

void redirResetStateMachine()
{
    gmsgdd.backOffTime = 5;
    gmsgdd.dropAllMsgBackOff = 1;
    gmsgdd.applyFailedNum = 0;
}


unsigned char RecBuf[REDIR_MSG_MAX_SIZE];
unsigned char RedirBuf[REDIR_MSG_MAX_SIZE];

void * redirMsgDispatchTaskFunc( void * x_void_ptr )
{
    int n;

    // initialize some global configuration.
    memset( &gmsgdd, '\0', sizeof gmsgdd );

    // wait for the websocket link to establish first.
    redirWaitForWebSocketToEstablish( );

    while( 1 )
    {
        while( redirWsLinkIsNotReady( ) )
        {
            sleep( 3 );
        }
        if( RedirSockFd != -1 )
        {
            n = recv( RedirSockFd, (void *)RecBuf, sizeof( RecBuf ) - 100, 0 );
            if( n > 0 )
            {
                unsigned char * SendBuf = calloc( 1, n+4 );

                if( SendBuf )
                {
                    redirNotify_t cn;

                    *(SendBuf+3) = 22;
                    memcpy( SendBuf+4, RecBuf, n );
                    redirAddOneTxBuffToPool( SendBuf, n+4 );
                    redirSendNotification( &cn );
                }
            }
        }
    }

    return( NULL );
}

