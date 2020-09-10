/* SPDX-License-Identifier: BSD-3-Clause */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <syslog.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <linux/if_ether.h>
#include <linux/types.h>
#include "radius/radius.h"
#include "radius/eap_defs.h"
#include<pthread.h>

#define PROBE_IS_RUNNING    0
#define PROBE_GOT_ACCEPT    1
#define PROBE_GOT_REJECT    2
#define PROBE_GOT_UNKNOWN   3
#define PROBE_GOT_CHALLENGE 4
#define PROBE_TIMED_OUT     5

#define U_P_LEN             30

char * Answers[5] = {"RUNNING", "ACCEPT", "REJECT", "UNKNOWN", "CHALLENGE"};

int Method = 1, Float = 0;
int ProbeDone = PROBE_IS_RUNNING;
long long TimeStart = 0, TimeEnd = 0;
char Realm[100];


extern void strace_install_segv_handler( void );
extern void eloop_run( void );

long long get_time( void )
{
    struct timespec tv;
    long long RetVal;

    clock_gettime( CLOCK_MONOTONIC, &tv );
    RetVal = (long long)tv.tv_sec*1000 + tv.tv_nsec/1000000;
    return( RetVal );
}


static int get_ip_addr( int FloatIp, unsigned char * IpAddr )
{
    int fd;
    struct ifreq ifr;

    fd = socket( AF_INET, SOCK_DGRAM, 0 );
    ifr.ifr_addr.sa_family = AF_INET;
    if( FloatIp )
        strncpy( ifr.ifr_name, "brtrunk:float", IFNAMSIZ-1 );
    else
        strncpy( ifr.ifr_name, "brtrunk", IFNAMSIZ-1 );

    ioctl( fd, SIOCGIFADDR, &ifr );
    close( fd );

    /* IP address bytes will be in network order */
    memcpy( IpAddr, &(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr), 4 );
    return( 0 );
}


static RadiusRxResult radius_msg_receive( struct radius_msg * msg, struct radius_msg * req,
                                          unsigned char * shared_secret, size_t shared_secret_len, void * data )
{
    if( msg->hdr->code == RADIUS_CODE_ACCESS_ACCEPT )
    {
        ProbeDone = PROBE_GOT_ACCEPT;
        TimeEnd = get_time( );
        return RADIUS_RX_UNKNOWN;
    }
    if( msg->hdr->code == RADIUS_CODE_ACCESS_REJECT )
    {
        ProbeDone = PROBE_GOT_REJECT;
        TimeEnd = get_time( );
        return RADIUS_RX_UNKNOWN;
    }
    if( msg->hdr->code == RADIUS_CODE_ACCESS_CHALLENGE )
    {
        ProbeDone = PROBE_GOT_CHALLENGE;
        TimeEnd = get_time( );
        return RADIUS_RX_UNKNOWN;
    }
    ProbeDone = PROBE_GOT_UNKNOWN;
    TimeEnd = get_time( );

    return RADIUS_RX_UNKNOWN;
}
                                                     
                                                     
static int radius_send_req( struct radius_client_data * rClient, unsigned char * addr, char * user, char * password )
{
    struct radius_msg * msg;
    char buf[128];
    unsigned char radius_id = 0;
    unsigned int ipaddr = 0;
    unsigned int Type, Prot, ll = strlen( Realm );

    get_ip_addr( Float, (unsigned char *)&ipaddr );

    msg = radius_msg_new( RADIUS_CODE_ACCESS_REQUEST, radius_id );
    if( msg == NULL )
    {
        return 0;
    }

    radius_msg_make_authenticator( msg, addr, ETH_ALEN );

    snprintf( buf, sizeof( buf ), "%s", user );
    if( !radius_msg_add_attr( msg, RADIUS_ATTR_USER_NAME, (unsigned char *)buf, strlen( buf ) ) )
    {
        goto fail;
    }

    if( !radius_msg_add_attr( msg, RADIUS_ATTR_NAS_IP_ADDRESS, (unsigned char *)&ipaddr, sizeof( unsigned int ) ) )
    {
        goto fail;
    }

    if( !radius_msg_add_attr_int32( msg, RADIUS_ATTR_NAS_PORT_TYPE, RADIUS_NAS_PORT_TYPE_IEEE_802_11 ) )
    {
        goto fail;
    }

    if( ll )
    {
        buf[0] = 0;
        buf[1] = 0;
        buf[2] = 0xB5;
        buf[3] = 0x4B;
        buf[4] = 22;
        buf[5] = ll+2;
        memcpy( &(buf[6]), Realm, ll );
        if( !radius_msg_add_attr( msg, RADIUS_ATTR_VENDOR_SPECIFIC, (u8*) buf, ll+6) )
        {
            goto fail;
        }
    }

    if( Method == 0 )
    {
        memset( buf, 0, sizeof( buf ) );
        snprintf( buf, sizeof(buf), "%s", password );
        if( !radius_msg_add_attr_user_password( msg, (u8 *)buf, strlen(buf), 
                                                rClient->conf->auth_server->shared_secret, rClient->conf->auth_server->shared_secret_len ) )
        {
            goto fail;
        }
    }

    Type = htonl( 2 );
    if( !radius_msg_add_attr( msg, 6, (u8 *)&Type, 4 ) )
    {
        goto fail;
    }
    Prot = htonl( 1 );
    if( !radius_msg_add_attr( msg, 7, (u8 *)&Prot, 4 ) )
    {
        goto fail;
    }

    if( Method == 1 )
    {
        struct eap_hdr eap;
        unsigned int len = 0;

        memset( &eap, 0, sizeof( eap ) );
        /* EAP header */
        eap.code = EAP_CODE_RESPONSE;
        eap.identifier = 0;
        len = sizeof( eap ) + sizeof( unsigned char ) + strlen( user );
        eap.length = htons( len );
        memcpy( buf, &eap, sizeof( eap ) );

        /* Add EAP type */
        *(buf+sizeof( eap )) = EAP_TYPE_IDENTITY;

        /* Add EAP data */
        memcpy( (buf+(sizeof( eap )+sizeof( unsigned char ))), user, strlen( user ) );

        /* Add EAP data to radius message */
        if( !radius_msg_add_eap( msg, (unsigned char *)buf, len ) )
        {
            goto fail;
        }
    }

    if( radius_client_send( rClient, msg, RADIUS_AUTH, addr ) == -2 )
    {
        return 0;
    }
    else
        return 1;

fail:

    radius_msg_free( msg );
    free( msg );
    return 0;
}


void * receive_from_socket( void * data )
{
    eloop_run( );
    return( NULL );
}


int radius_probe(const char * ipaddr,  const char * secret, int port)
{
    unsigned char addr[6] = {0, 1, 2, 3, 4, 5};
    struct radius_server Server;
    struct radius_servers Servers;
    struct radius_client_data * rClient;
    pthread_t recv_thread;
    int recv_data;
    long long TimeCurr;
    char Username[U_P_LEN], Password[U_P_LEN];

    if( inet_aton( ipaddr, &(Server.addr.u.v4) ) == 0 )
    {
        return 0;
    }
    Server.port = port;
    Server.addr.af = AF_INET;
    Server.shared_secret = (unsigned char*)secret;
    Server.shared_secret_len = strlen( "testing123" );

    strcpy( Username, "openwrt" );
    strcpy( Password, secret );
    Server.shared_secret_len = strlen( Password );
    Realm[0] = 0;
    memset( &Servers, 0, sizeof( Servers ) );
    Servers.auth_server = Servers.auth_servers = &Server;
    Servers.num_auth_servers = 1;
    Servers.retry_primary_interval = 2;
    if( Float )
    {
        Servers.a2w_signature = 0xA22A2AA2;
        get_ip_addr( 1, (unsigned char *)&(Servers.my_ip) );
    }

    rClient = radius_client_init( NULL, &Servers );
    
    if( rClient == NULL )
    {
        return 0;
    }

    if( radius_client_register( rClient, RADIUS_AUTH, radius_msg_receive, NULL ) < 0 )
    {
        return 0;
    }

    if( pthread_create( &recv_thread, NULL, receive_from_socket, &recv_data ) < 0 )
    {
        return 0;
    }

    TimeStart = get_time( );
    radius_send_req( rClient, addr, Username, Password );
    while( ProbeDone == PROBE_IS_RUNNING )
    {
        sleep( 1 );
        TimeCurr = get_time( );

        if( (TimeCurr-TimeStart) > 9000 )
        {
            TimeEnd = TimeCurr;
            break;
        }
    }
    if( ProbeDone == PROBE_IS_RUNNING )
    {
        printf("No answer from RADIUS server\n" );
    }
    else
    {
        if( (ProbeDone < 0) || (ProbeDone > 4) )
            ProbeDone = 0;
       // LOG(INFO, "Got %s from RADIUS server in %lld msec\n", Answers[ProbeDone], TimeEnd-TimeStart );
	pthread_cancel(recv_thread);
	return 1;
    }
	pthread_cancel(recv_thread);
        return 0;

}

