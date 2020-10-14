/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>

#include <netdb.h>
#include <sys/socket.h>
#include <time.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <unistd.h>
#ifdef CMAKE_BUILD
#include <websocket/lws_config.h>
#endif
#include <websocket/libwebsockets.h>
#include <libwebsockets.c>

#include "redirclient.h"
#include "redirintercomm.h"
#include "redirmessagedef.h"
#include "redirmsgdispatch.h"

#define REDIR_BUFF_MAX         200


typedef struct redirBufDes_s
{
    int isValid;
    int size;
    unsigned char * pBuf;
}redirBufDes_t;

typedef struct redirBufferPool_s
{
    pthread_mutex_t txBufLock;
    pthread_mutex_t rxBufLock;
    int numBuf;
    redirBufDes_t bufTxDes[REDIR_BUFF_MAX];
    redirBufDes_t bufRxDes[REDIR_BUFF_MAX];
} redirBufferPool_t;


redirBufferPool_t commBuf;

struct list_head
{
    struct list_head *next, *prev;
};


#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
    struct list_head name = LIST_HEAD_INIT(name)


static LIST_HEAD( redirMsgHd );

struct redirTxMsg
{
    struct list_head pend;
    int size;
    unsigned char * pBuf;
};

static inline void __list_add( struct list_head *new,
                               struct list_head *prev,
                               struct list_head *next)
{
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
    __list_add(new, head->prev, head);
}


#define container_of(ptr, type, member) ({              \
    const typeof(((type *)0)->member) * __mptr = (ptr); \
    (type *)((char *)__mptr - offsetof(type, member)); })

#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)


static inline void __list_del(struct list_head * prev, struct list_head * next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void list_del(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
}

static inline int list_empty(const struct list_head *head)
{
    return head->next == head;
}


#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); \
        pos = n, n = pos->next)


// global configuration
redirConfig redircfg;
redirStatus redirst;

int RedirWsTxErr = 0;

/*
 * this is specified in the 04 standard, control frames can only have small
 * payload length styles
 */
#define MAX_PING_PAYLOAD    125
#define MAX_REDIR_PAYLOAD   8192

#define PING_RINGBUFFER_SIZE 256

struct lws * redir_wsi[MAX_REDIR_CLIENTS];
static unsigned char pingbuf[LWS_PRE + MAX_REDIR_PAYLOAD];
static char peer_name[128];
static unsigned long started;

static unsigned long rtt_min = 100000000;
static unsigned long rtt_max;
static unsigned long rtt_avg;
static unsigned long global_rx_count;
static unsigned long global_tx_count;

struct ping {
	unsigned long issue_timestamp;
	unsigned long index;
	unsigned int seen;
};

struct per_session_data__ping {
	unsigned long ping_index;

	struct ping ringbuffer[PING_RINGBUFFER_SIZE];
	int ringbuffer_head;
	int ringbuffer_tail;

	unsigned long rx_count;
};


int redirAddOneTxBuffToPool( unsigned char * Buffer, unsigned int len )
{
    int i;
    int found = 0;

    pthread_mutex_lock( &commBuf.txBufLock );
    for( i=0; i<commBuf.numBuf; i++ )
    {
        if( commBuf.bufTxDes[i].isValid == 0 )
        {
            commBuf.bufTxDes[i].pBuf = Buffer;
            commBuf.bufTxDes[i].isValid = 1;
            commBuf.bufTxDes[i].size = len;
            found = 1;
            break;
        }
    }
    if( found )
        gmsgdd.txMsgIsReady = 1;
    else
    {
        free( Buffer );
    }
    pthread_mutex_unlock( &commBuf.txBufLock );

    return 0;
}

static int redirLinkClientMsgWritableHandler( struct lws *wsi, struct per_session_data__ping *psd, unsigned char *buff, int len)
{
    int n;
    unsigned char * p;

    p = &pingbuf[LWS_PRE];

    memcpy(p, buff, len);

    global_tx_count++;

    n = lws_write(wsi, &pingbuf[LWS_PRE],
                           len, redircfg.write_options | LWS_WRITE_BINARY);

    if( n < 0 )
    {
        RedirWsTxErr = 1;
        return -1;
    }
    if (n < len) {
        lwsl_err("Partial write\n");
        return -1;
    }

    return 0;
}

static int redirLinkClientPongReceiveHandler(struct lws *wsi, struct per_session_data__ping *psd, void *in,
	size_t len)
{
	struct timespec tv;
	unsigned char *p;
	int shift, n;
	unsigned long l;
	int match = 0;
	unsigned long iv;


    clock_gettime( CLOCK_MONOTONIC, &tv );
    iv = (tv.tv_sec * 1000000) + tv.tv_nsec/1000;

	psd->rx_count++;

	shift = 56;
	p = in;
	l = 0;

	while (shift >= 0) {
		l |= (*p++) << shift;
		shift -= 8;
	}

	/* find it in the ringbuffer, look backwards from head */
	n = psd->ringbuffer_head;
	while (!match) {

		if (psd->ringbuffer[n].index == l) {
			psd->ringbuffer[n].seen++;
			match = 1;
			continue;
		}

		if (n == psd->ringbuffer_tail) {
			match = -1;
			continue;
		}

		if (n == 0)
			n = PING_RINGBUFFER_SIZE - 1;
		else
			n--;
	}

	if (match < 1) {

		return 0;
	}

	if (psd->ringbuffer[n].seen > 1)
		//wlog("DUP! ");

	if ((iv - psd->ringbuffer[n].issue_timestamp) < rtt_min)
		rtt_min = iv - psd->ringbuffer[n].issue_timestamp;

	if ((iv - psd->ringbuffer[n].issue_timestamp) > rtt_max)
		rtt_max = iv - psd->ringbuffer[n].issue_timestamp;

	rtt_avg += iv - psd->ringbuffer[n].issue_timestamp;
	global_rx_count++;

	return 0;
}

static int redirLinkClientMsgReceiveHandler( struct lws * wsi, struct per_session_data__ping * psd, void * in,
                                             size_t len )
{
    int tlen;
    unsigned char * CharBuf;

    psd->rx_count++;

    tlen = len;
    if( tlen > REDIR_MSG_MAX_SIZE )
        tlen = REDIR_MSG_MAX_SIZE;

    CharBuf = (unsigned char *)in;

    if( strstr( (char *)CharBuf, "connect_to_CE_port" ) )
    {
        redirOpenRedirSocket( 22 );
        return( 0 );
    }
    if( strstr( (char *)CharBuf, "disconnect_from_CE_port" ) )
    {
        redirCloseRedirSocket( );
        return( 0 );
    }
    if( (*(CharBuf+0) == 0) && (*(CharBuf+1) == 0) )
    {
        redirMessageRedirect( CharBuf+4, tlen-4 );
        return( 0 );
    }
    return( 0 );
}


void redirInitLastReceivedTime()
{
    struct timespec tv;

    clock_gettime( CLOCK_MONOTONIC, &tv );
    redirst.lastRecvSec = tv.tv_sec;
}


void redirUpdateLastReceivedTime()
{
    struct timespec tv;
    static int cnt = 0;

    cnt++;
#if 1
    clock_gettime( CLOCK_MONOTONIC, &tv );
    redirst.lastRecvSec = tv.tv_sec;
#else
    if (cnt >= 7 && cnt <= 17) {
        ;
    } else {
        gettimeofday(&tv, NULL);
        redirst.lastRecvSec = tv.tv_sec;
    }
#endif

}


static int callback_redir_link(struct lws *wsi, enum lws_callback_reasons reason,
	void *user, void *in, size_t len)
{
	int n;
	struct per_session_data__ping *psd = user;
	struct lws_pollargs *pa = (struct lws_pollargs *)in;
    //struct timeval tv;

	switch (reason) {
	case LWS_CALLBACK_CLOSED:
		/* remove closed guy */
		for (n = 0; n < redircfg.clients; n++)
			if (redir_wsi[n] == wsi) {
				redircfg.clients--;
				while (n < redircfg.clients) {
					redir_wsi[n] = redir_wsi[n + 1];
					n++;
				}
			}

		break;

	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		psd->rx_count = 0;
		psd->ping_index = 1;
		psd->ringbuffer_head = 0;
		psd->ringbuffer_tail = 0;

		/*
		 * start the ball rolling,
		 * LWS_CALLBACK_CLIENT_WRITEABLE will come next service
		 */

		lws_callback_on_writable( wsi);
		break;

	case LWS_CALLBACK_CLIENT_RECEIVE:
    case LWS_CALLBACK_CLIENT_RECEIVE_PONG:
        redirUpdateLastReceivedTime();
		if (reason == LWS_CALLBACK_CLIENT_RECEIVE) {
			redirLinkClientMsgReceiveHandler( wsi, psd, in, len);
		} else {
			redirLinkClientPongReceiveHandler( wsi, psd, in, len);
		}

		break;

    case LWS_CALLBACK_CLIENT_WRITEABLE:
        if( lws_partial_buffered( wsi ) )
        {
            //wlog( "******************************** writable, wait for nexttime \n" );
            lws_callback_on_writable( wsi );
            break;
        }
        if( gmsgdd.txMsgIsReady )
        {
            int done, i;
            redirBufDes_t tmpBTD[REDIR_BUFF_MAX];

            pthread_mutex_lock( &(commBuf.txBufLock) );
            memcpy( tmpBTD, commBuf.bufTxDes, sizeof( commBuf.bufTxDes ) );
            memset( commBuf.bufTxDes, '\0', sizeof( commBuf.bufTxDes ) );
            pthread_mutex_unlock( &(commBuf.txBufLock) );

            // construct the linked list
            for( i = 0; i < REDIR_BUFF_MAX; i++ )
            {
                if( tmpBTD[i].isValid )
                {
                    struct redirTxMsg * ctm = calloc( 1, sizeof( struct redirTxMsg ) );
                    ctm->pBuf = tmpBTD[i].pBuf;
                    ctm->size = tmpBTD[i].size;
                    list_add_tail(&ctm->pend, &redirMsgHd);
                    //wlog( "===www adding to list %d", ctm->size );
                }
            }

            // now to send the list
            done = 1;
            {
                struct list_head *p, *n;
                struct redirTxMsg *ctm;

                list_for_each_safe(p, n, &redirMsgHd)
                {
                    ctm = list_entry(p, struct redirTxMsg, pend);

                    //wlog("*** send client msg cnt %d  pbuf %p\n", cnt, ctm->pBuf);
                    //wlog( "===www processing from list %d", ctm->size );
                    redirLinkClientMsgWritableHandler( wsi, psd, ctm->pBuf, ctm->size );
                    //wlog("--- send client msg cnt %d  done\n", cnt);

                    list_del(&ctm->pend);
                    free(ctm->pBuf);
                    free(ctm);

                    if (lws_partial_buffered(wsi))
                    {
                        //wlog("***** skip rest messages for this time\n");
                        lws_callback_on_writable( wsi);
                        done = 0;
                        break;
                    }
                }
            }
            if( done )
                gmsgdd.txMsgIsReady = 0;
        }
        break;
#if 0
    case LWS_CALLBACK_CLIENT_WRITEABLE:
        gettimeofday(&tv, NULL);
        redirst.lastSentSec = tv.tv_sec;
        //wlog("send ws frame, lastSentUs updated %ld sec \n", tv.tv_sec);
		if (gmsgdd.needToPing) {
			gmsgdd.needToPing = 0;
			redirLinkClientPingWritableHandler(this, wsi, psd);
		}
		if (gmsgdd.txMsgIsReady) {
			int i;
			int idx = 1;
			redirBufDes_t tmpBTD[REDIR_BUFF_MAX];
			redirGetAndClearAllTxBufDescritors(redircfg.rcvArray[idx].pBP, tmpBTD);
			for (i=0; i<REDIR_BUFF_MAX; i++ ) {
				if (tmpBTD[i].isValid) {
					redirLinkClientMsgWritableHandler(this, wsi, psd, tmpBTD[i].pBuf);
					free(tmpBTD[i].pBuf);
				} else{
					break;
				}
			}
			gmsgdd.txMsgIsReady = 0;
		}

		break;
#endif

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        //wlog("LWS_CALLBACK_CLIENT_CONNECTION_ERROR **********************\n");
        break;

	case LWS_CALLBACK_ADD_POLL_FD:
		redirAddPOnePollFd(pa->fd, pa->events);
		break;

	case LWS_CALLBACK_DEL_POLL_FD:
		redirRemoveOnePollFd(pa->fd);
		break;

	case LWS_CALLBACK_CHANGE_MODE_POLL_FD:
		redirChangeModePollFd(pa->fd, pa->events);
		break;

	default:
		break;
	}

	return 0;
}

/* list of supported protocols and callbacks */

static struct lws_protocols protocols[] = {
    {
        "redir-link-protocol", callback_redir_link,
        sizeof(struct per_session_data__ping), 0, 0, NULL
    },
    {
        NULL, NULL, 0, 0, 0, NULL /* end of list */
    }
};


int redirNotifyTxMessageIsReady(struct lws_context *context, unsigned long lastSentSec)
{
    int n;
    struct timespec tv;

    if( lastSentSec == 0 )
    {
        clock_gettime( CLOCK_MONOTONIC, &tv );
        lastSentSec = tv.tv_sec;
    }

    for( n = 0; n < redircfg.clients; n++ )
        lws_callback_on_writable( redir_wsi[n] );
    redirst.lastSentSec = lastSentSec;

    return( 1 );
}

static const struct lws_extension exts[] = {
	{
		"permessage-deflate",
		lws_extension_callback_pm_deflate,
		"permessage-deflate; client_no_context_takeover; client_max_window_bits"
	},
	{
		"deflate-frame",
		lws_extension_callback_pm_deflate,
		"deflate_frame"
	},
	{ NULL, NULL, NULL /* terminator */ }
};


static int redirStartWebSocketConnection( struct lws_context_creation_info * pInfo )
{
    struct lws_context * context;
    char ip[30];
    struct timespec tv;
    int n;
    unsigned long l;
    //int rc = 0;
    int ret = 0;
   	struct lws_client_connect_info i;


    redircfg.clients = 1;

    context = lws_create_context( pInfo );
    if( context == NULL )
    {
        return( -1 );
    }

	memset(&i, 0, sizeof(i));
	i.context = context;
	i.address = redircfg.address;
	i.port = redircfg.port;
	i.ssl_connection = redircfg.use_ssl;
	i.path = redircfg.urlname;
	i.origin = "origin";
	i.protocol = protocols[0].name;
	i.client_exts = exts;
    i.host = i.address;
	i.ietf_version_or_minus_one = redircfg.ietf_version;

    do
    {
        redir_wsi[0] = lws_client_connect_via_info(&i);
        if (redir_wsi[0] == NULL)
        {
            printf( "cmClient failed to connect\n" );
            sleep( 5 );
            //	return NULL;
        }
    } while( redir_wsi[0] == NULL );


    memset( peer_name, '\0', sizeof( peer_name ) );

    if( redir_wsi[0] ) 
    {
        lws_get_peer_addresses( redir_wsi[0], lws_get_socket_fd(redir_wsi[0]), peer_name,
                                               sizeof( peer_name ), ip, sizeof( ip ) );
    }

    clock_gettime( CLOCK_MONOTONIC, &tv );
    started = (tv.tv_sec * 1000000) + tv.tv_nsec/1000;

    redircfg.wsLinkIsReady = 1;
    redirNotifyWebSocketIsReady( );
    redirInitLastReceivedTime( );
    /* service loop */
    n = 0;

    while( n >= 0 )
    {
        clock_gettime( CLOCK_MONOTONIC, &tv );
        l = tv.tv_sec;

		/* servers can hang up on us */
        if (redircfg.clients == 0) {
            n = -1;
            continue;
        }

        if( RedirWsTxErr )
        {
            n = -4;
            continue;
        }

        if (l- redirst.lastRecvSec > redircfg.wsResetIntervalSec) {
            n = -3;
            redirDisableMsgSending();
            continue;
        }
        if ((l - redirst.lastSentSec) > redircfg.pingIntervalSec) {
            gmsgdd.needToPing = 1;
            redirNotifyTxMessageIsReady( context, l );
        }

        /*
         * this represents an existing server's single poll action
         * which also includes libwebsocket sockets
         */
        redirWaitforFds( context, 2000 );
    }

    /* stats */
    redircfg.wsLinkIsReady = 0;
    lws_context_destroy( context );
    ret = n;
    return( ret );
}


void * redirClientTask( void * handler )
{
    struct lws_context_creation_info info;
    char ca_filepath[1032];
    char cert_filepath[1035];
    char key_filepath[1039];
    FILE *fptr;

    memset( &info, 0, sizeof info );
    memset( &commBuf, 0, sizeof( commBuf ) );
    pthread_mutex_init( &(commBuf.txBufLock), NULL );
    pthread_mutex_init( &(commBuf.rxBufLock), NULL );
    commBuf.numBuf = REDIR_BUFF_MAX;

    redirInitializePollFd( );

    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.extensions = exts;
    if( redircfg.use_ssl )
    {
        fprintf( stderr, "use_ssl %d (%s) \n", redircfg.use_ssl, redircfg.ssl_certdir );
        {
            snprintf( ca_filepath, sizeof( ca_filepath ), "%s/%s", redircfg.ssl_certdir, "ca.pem" );    //ca.crt
            snprintf( cert_filepath, sizeof( cert_filepath ), "%s/%s", redircfg.ssl_certdir, "client.pem" );  //client.crt
            snprintf( key_filepath, sizeof( key_filepath ), "%s/%s", redircfg.ssl_certdir, "client_dec.key" );   //client.key
            fprintf( stderr, "use pathhhhh %s \n", ca_filepath );
        }
        info.ssl_ca_filepath = ca_filepath;
        info.ssl_cert_filepath = cert_filepath;
        info.ssl_private_key_filepath = key_filepath;
    }
    info.gid = -1;
    info.uid = -1;

    if( redircfg.use_ssl )
    {
        info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
        fptr = fopen( ca_filepath, "r" );
        if( fptr != NULL )
        {
            fclose( fptr );

            do
            {
                if( redirStartWebSocketConnection( &info ) < 0 )
                    break;
            } while( 1 );
        }
        else
            printf("No certificate files present \n");
    }
    else
    {
        do
        {
            if( redirStartWebSocketConnection( &info ) < 0 )
                break;
        } while( 1 );
    }
	printf("exiting websockets\n");
    return( NULL );
}

int redirConfigInit( void )
{
    memset( &redircfg, '\0', sizeof( redircfg ) );

    redircfg.port = WS_DEFAULT_PORT;
    redircfg.pingIntervalSec = WS_PING_INTERVAL;
    redircfg.wsResetIntervalSec = WS_RESET_LINK_INTERVAL;
    redircfg.size = 64;
    redircfg.pingsize = MAX_PING_PAYLOAD;
    redircfg.flood = 0;
    redircfg.wsLinkIsReady = 0;
    redircfg.clients = 1;
    redircfg.ietf_version = -1;
    strcpy( redircfg.urlname, "/" );

    memset( &redirst, '\0', sizeof( redirst ) );

    return( 0 );
}


int redirWsLinkIsNotReady()
{
    return( !redircfg.wsLinkIsReady );
}


void redirDisableMsgSending()
{
    redircfg.wsLinkIsReady = 0;
}

