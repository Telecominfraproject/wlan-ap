#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
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

#define CMD_SET_ETHERNET			0x01
#define CMD_SET_WIFI				0x02

#define MAX_2G_CHANNEL_NUM_MIB		14
#define MAX_5G_CHANNEL_NUM_MIB		196


#define MAX_5G_DIFF_NUM		14

#define PIN_LEN					8
#ifdef HAVE_RTK_DUAL_BAND_SUPPORT
#define NUM_WLAN_INTERFACE 2
#else
#define NUM_WLAN_INTERFACE 1
#endif
#define HW_SETTING_HEADER_TAG		((char *)"H6")
#define HW_WLAN_SETTING_OFFSET	13

#define HW_SETTING_HEADER_OFFSET 	6
#define HW_SETTING_ETHMAC_OFFSET 	1
#define ETH_ALEN					6

#define __PACK__			__attribute__ ((packed))


typedef struct hw_wlan_setting {
	unsigned char macAddr[6] __PACK__;
	unsigned char macAddr1[6] __PACK__;
	unsigned char macAddr2[6] __PACK__;
	unsigned char macAddr3[6] __PACK__;
	unsigned char macAddr4[6] __PACK__;
	unsigned char macAddr5[6] __PACK__; 
	unsigned char macAddr6[6] __PACK__; 
	unsigned char macAddr7[6] __PACK__; 
	unsigned char pwrlevelCCK_A[MAX_2G_CHANNEL_NUM_MIB] __PACK__; 	
	unsigned char pwrlevelCCK_B[MAX_2G_CHANNEL_NUM_MIB] __PACK__; 
	unsigned char pwrlevelHT40_1S_A[MAX_2G_CHANNEL_NUM_MIB] __PACK__;	
	unsigned char pwrlevelHT40_1S_B[MAX_2G_CHANNEL_NUM_MIB] __PACK__;	
	unsigned char pwrdiffHT40_2S[MAX_2G_CHANNEL_NUM_MIB] __PACK__;	
	unsigned char pwrdiffHT20[MAX_2G_CHANNEL_NUM_MIB] __PACK__; 
	unsigned char pwrdiffOFDM[MAX_2G_CHANNEL_NUM_MIB] __PACK__; 
	unsigned char regDomain __PACK__; 	
	unsigned char rfType __PACK__; 
	unsigned char ledType __PACK__; // LED type, see LED_TYPE_T for definition	
	unsigned char xCap __PACK__;	
	unsigned char TSSI1 __PACK__;	
	unsigned char TSSI2 __PACK__;	
	unsigned char Ther __PACK__;	
	unsigned char Reserved1 __PACK__;
	unsigned char Reserved2 __PACK__;
	unsigned char Reserved3 __PACK__;
	unsigned char Reserved4 __PACK__;
	unsigned char Reserved5 __PACK__;	
	unsigned char Reserved6 __PACK__;
	unsigned char Reserved7 __PACK__;	
	unsigned char Reserved8 __PACK__;
	unsigned char Reserved9 __PACK__;
	unsigned char Reserved10 __PACK__;
	unsigned char pwrlevel5GHT40_1S_A[MAX_5G_CHANNEL_NUM_MIB] __PACK__;	
	unsigned char pwrlevel5GHT40_1S_B[MAX_5G_CHANNEL_NUM_MIB] __PACK__;	
	unsigned char pwrdiff5GHT40_2S[MAX_5G_CHANNEL_NUM_MIB] __PACK__; 
	unsigned char pwrdiff5GHT20[MAX_5G_CHANNEL_NUM_MIB] __PACK__;	
	unsigned char pwrdiff5GOFDM[MAX_5G_CHANNEL_NUM_MIB] __PACK__;

	
	unsigned char wscPin[PIN_LEN+1] __PACK__;	

#if defined(HAVE_RTK_AC_SUPPORT) || defined(HAVE_RTK_4T4R_AC_SUPPORT)
	unsigned char pwrdiff_20BW1S_OFDM1T_A[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
	unsigned char pwrdiff_40BW2S_20BW2S_A[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
	unsigned char pwrdiff_OFDM2T_CCK2T_A[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
	unsigned char pwrdiff_40BW3S_20BW3S_A[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
	unsigned char pwrdiff_4OFDM3T_CCK3T_A[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
	unsigned char pwrdiff_40BW4S_20BW4S_A[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
	unsigned char pwrdiff_OFDM4T_CCK4T_A[MAX_2G_CHANNEL_NUM_MIB] __PACK__;

	unsigned char pwrdiff_5G_20BW1S_OFDM1T_A[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_40BW2S_20BW2S_A[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_40BW3S_20BW3S_A[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_40BW4S_20BW4S_A[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_RSVD_OFDM4T_A[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_80BW1S_160BW1S_A[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_80BW2S_160BW2S_A[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_80BW3S_160BW3S_A[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_80BW4S_160BW4S_A[MAX_5G_DIFF_NUM] __PACK__;


	unsigned char pwrdiff_20BW1S_OFDM1T_B[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
	unsigned char pwrdiff_40BW2S_20BW2S_B[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
	unsigned char pwrdiff_OFDM2T_CCK2T_B[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
	unsigned char pwrdiff_40BW3S_20BW3S_B[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
	unsigned char pwrdiff_OFDM3T_CCK3T_B[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
	unsigned char pwrdiff_40BW4S_20BW4S_B[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
	unsigned char pwrdiff_OFDM4T_CCK4T_B[MAX_2G_CHANNEL_NUM_MIB] __PACK__;

	unsigned char pwrdiff_5G_20BW1S_OFDM1T_B[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_40BW2S_20BW2S_B[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_40BW3S_20BW3S_B[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_40BW4S_20BW4S_B[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_RSVD_OFDM4T_B[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_80BW1S_160BW1S_B[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_80BW2S_160BW2S_B[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_80BW3S_160BW3S_B[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_80BW4S_160BW4S_B[MAX_5G_DIFF_NUM] __PACK__;
#endif

#if defined(HAVE_RTK_4T4R_AC_SUPPORT)
  unsigned char  pwrdiff_20BW1S_OFDM1T_C[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrdiff_40BW2S_20BW2S_C[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrdiff_OFDM2T_CCK2T_C[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrdiff_40BW3S_20BW3S_C[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrdiff_4OFDM3T_CCK3T_C[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrdiff_40BW4S_20BW4S_C[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrdiff_OFDM4T_CCK4T_C[MAX_2G_CHANNEL_NUM_MIB] __PACK__;

  unsigned char  pwrdiff_5G_20BW1S_OFDM1T_C[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_40BW2S_20BW2S_C[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_40BW3S_20BW3S_C[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_40BW4S_20BW4S_C[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_RSVD_OFDM4T_C[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_80BW1S_160BW1S_C[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_80BW2S_160BW2S_C[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_80BW3S_160BW3S_C[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_80BW4S_160BW4S_C[MAX_5G_DIFF_NUM] __PACK__;

  unsigned char  pwrdiff_20BW1S_OFDM1T_D[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrdiff_40BW2S_20BW2S_D[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrdiff_OFDM2T_CCK2T_D[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrdiff_40BW3S_20BW3S_D[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrdiff_OFDM3T_CCK3T_D[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrdiff_40BW4S_20BW4S_D[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrdiff_OFDM4T_CCK4T_D[MAX_2G_CHANNEL_NUM_MIB] __PACK__;

  unsigned char  pwrdiff_5G_20BW1S_OFDM1T_D[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_40BW2S_20BW2S_D[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_40BW3S_20BW3S_D[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_40BW4S_20BW4S_D[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_RSVD_OFDM4T_D[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_80BW1S_160BW1S_D[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_80BW2S_160BW2S_D[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_80BW3S_160BW3S_D[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_80BW4S_160BW4S_D[MAX_5G_DIFF_NUM] __PACK__;

  unsigned char  pwrlevelCCK_C[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrlevelCCK_D[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrlevelHT40_1S_C[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrlevelHT40_1S_D[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrlevel5GHT40_1S_C[MAX_5G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrlevel5GHT40_1S_D[MAX_5G_CHANNEL_NUM_MIB] __PACK__; 
#endif
} HW_WLAN_SETTING_T, *HW_WLAN_SETTING_Tp;
typedef struct hw_wlan_ac_setting{
	unsigned char pwrdiff_20BW1S_OFDM1T_A[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
	unsigned char pwrdiff_40BW2S_20BW2S_A[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
	unsigned char pwrdiff_OFDM2T_CCK2T_A[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
	unsigned char pwrdiff_40BW3S_20BW3S_A[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
	unsigned char pwrdiff_4OFDM3T_CCK3T_A[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
	unsigned char pwrdiff_40BW4S_20BW4S_A[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
	unsigned char pwrdiff_OFDM4T_CCK4T_A[MAX_2G_CHANNEL_NUM_MIB] __PACK__;

	unsigned char pwrdiff_5G_20BW1S_OFDM1T_A[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_40BW2S_20BW2S_A[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_40BW3S_20BW3S_A[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_40BW4S_20BW4S_A[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_RSVD_OFDM4T_A[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_80BW1S_160BW1S_A[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_80BW2S_160BW2S_A[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_80BW3S_160BW3S_A[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_80BW4S_160BW4S_A[MAX_5G_DIFF_NUM] __PACK__;


	unsigned char pwrdiff_20BW1S_OFDM1T_B[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
	unsigned char pwrdiff_40BW3S_20BW3S_B[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
	unsigned char pwrdiff_OFDM3T_CCK3T_B[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
	unsigned char pwrdiff_40BW4S_20BW4S_B[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
	unsigned char pwrdiff_OFDM4T_CCK4T_B[MAX_2G_CHANNEL_NUM_MIB] __PACK__;

	unsigned char pwrdiff_5G_20BW1S_OFDM1T_B[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_40BW2S_20BW2S_B[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_40BW3S_20BW3S_B[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_40BW4S_20BW4S_B[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_RSVD_OFDM4T_B[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_80BW1S_160BW1S_B[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_80BW2S_160BW2S_B[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_80BW3S_160BW3S_B[MAX_5G_DIFF_NUM] __PACK__;
	unsigned char pwrdiff_5G_80BW4S_160BW4S_B[MAX_5G_DIFF_NUM] __PACK__;
#if defined(HAVE_RTK_4T4R_AC_SUPPORT)
  unsigned char  pwrdiff_20BW1S_OFDM1T_C[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrdiff_40BW2S_20BW2S_C[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrdiff_OFDM2T_CCK2T_C[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrdiff_40BW3S_20BW3S_C[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrdiff_4OFDM3T_CCK3T_C[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrdiff_40BW4S_20BW4S_C[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrdiff_OFDM4T_CCK4T_C[MAX_2G_CHANNEL_NUM_MIB] __PACK__;

  unsigned char  pwrdiff_5G_20BW1S_OFDM1T_C[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_40BW2S_20BW2S_C[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_40BW3S_20BW3S_C[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_40BW4S_20BW4S_C[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_RSVD_OFDM4T_C[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_80BW1S_160BW1S_C[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_80BW2S_160BW2S_C[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_80BW3S_160BW3S_C[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_80BW4S_160BW4S_C[MAX_5G_DIFF_NUM] __PACK__;


  unsigned char  pwrdiff_20BW1S_OFDM1T_D[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrdiff_40BW2S_20BW2S_D[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrdiff_OFDM2T_CCK2T_D[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrdiff_40BW3S_20BW3S_D[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrdiff_OFDM3T_CCK3T_D[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrdiff_40BW4S_20BW4S_D[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrdiff_OFDM4T_CCK4T_D[MAX_2G_CHANNEL_NUM_MIB] __PACK__;

  unsigned char  pwrdiff_5G_20BW1S_OFDM1T_D[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_40BW2S_20BW2S_D[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_40BW3S_20BW3S_D[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_40BW4S_20BW4S_D[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_RSVD_OFDM4T_D[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_80BW1S_160BW1S_D[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_80BW2S_160BW2S_D[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_80BW3S_160BW3S_D[MAX_5G_DIFF_NUM] __PACK__;
  unsigned char  pwrdiff_5G_80BW4S_160BW4S_D[MAX_5G_DIFF_NUM] __PACK__;

  unsigned char  pwrlevelCCK_C[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrlevelCCK_D[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrlevelHT40_1S_C[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrlevelHT40_1S_D[MAX_2G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrlevel5GHT40_1S_C[MAX_5G_CHANNEL_NUM_MIB] __PACK__;
  unsigned char  pwrlevel5GHT40_1S_D[MAX_5G_CHANNEL_NUM_MIB] __PACK__; 
#endif
}HW_WLAN_AC_SETTING_T, *HW_WLAN_AC_SETTING_Tp;
typedef struct hw_setting {
	unsigned char boardVer __PACK__;	// h/w board version
	unsigned char nic0Addr[6] __PACK__;
	unsigned char nic1Addr[6] __PACK__;
	HW_WLAN_SETTING_T wlan[NUM_WLAN_INTERFACE];	
} HW_SETTING_T, *HW_SETTING_Tp;
#define TAG_LEN					2
#define SIGNATURE_LEN			4
#define HW_SETTING_VER			3	// hw setting version
/* Config file header */
typedef struct param_header {
	unsigned char signature[SIGNATURE_LEN] __PACK__;  // Tag + version
	unsigned short len __PACK__;
} PARAM_HEADER_T, *PARAM_HEADER_Tp;

#ifdef CONFIG_MTD_NAND
#define FLASH_DEVICE_NAME		("/hw_setting/hw.bin")
#define FLASH_DEVICE_NAME1		("/hw_setting/hw1.bin")
#else
#define FLASH_DEVICE_NAME		("/dev/mtdblock0")
#define FLASH_DEVICE_NAME1		("/dev/mtdblock1")
#endif
#if 0
#ifdef RTK_HW_OFFSET
#define HW_SETTING_OFFSET		RTK_HW_OFFSET
#else
#define HW_SETTING_OFFSET		0x6000
#endif
#endif
unsigned int HW_SETTING_OFFSET = 0x6000; //mark_hw , non-8198c default hw setting

static void read_hw_setting_offset(void) 
{
	FILE *hwpart_proc;
	hwpart_proc = fopen ( "/proc/flash/hwpart", "r" );
	if ( hwpart_proc != NULL )
	{
		 char buf[16];
		 unsigned int hw_setting_off=0;

		 fgets(buf, sizeof(buf), hwpart_proc);        /* eat line */
		 sscanf(buf, "%x",&hw_setting_off);
#ifndef CONFIG_MTD_NAND
		if(hw_setting_off == 0)
			hw_setting_off = HW_SETTING_OFFSET;
#endif
		 HW_SETTING_OFFSET = hw_setting_off;
		 fclose(hwpart_proc);		 
		 //printf("read_hw_setting_offset = %x \n",HW_SETTING_OFFSET);
	}
}
static int flash_read(char *buf, int offset, int len)
{
	int fh;
	int ok=1;

#ifdef CONFIG_MTD_NAND
	fh = open(FLASH_DEVICE_NAME, O_RDWR|O_CREAT);
#else
	fh = open(FLASH_DEVICE_NAME, O_RDWR);
#endif

	if ( fh == -1 )
	{
		printf("open file error\n");
		return 0;
	}

	lseek(fh, offset, SEEK_SET);

	if ( read(fh, buf, len) != len)
		ok = 0;

	close(fh);

	return ok;
}
static int read_hw_setting(char *buf)
{
	PARAM_HEADER_T header;
	if(flash_read(&header,HW_SETTING_OFFSET,sizeof(PARAM_HEADER_T))==0){
		syslog(LOG_ERR,"Read wlan hw setting header failed\n");
		return -1;
	}
	if(memcmp(header.signature, HW_SETTING_HEADER_TAG, TAG_LEN)){
		syslog(LOG_ERR,"Invild wlan hw setting signature!\n");
		return -1;
	}
	if(flash_read(buf,HW_SETTING_OFFSET+sizeof(PARAM_HEADER_T),header.len)==0){
		syslog(LOG_ERR,"Read wlan hw setting to memory failed\n");
		
		return -1;
	}
	return 0;
	
}
static int read_hw_setting_length()
{
	PARAM_HEADER_T header;
	int len;
	if(flash_read(&header,HW_SETTING_OFFSET,sizeof(PARAM_HEADER_T))==0){
		syslog(LOG_ERR,"Read wlan hw setting header failed\n");
		return -1;
	}
	if(memcmp(header.signature, HW_SETTING_HEADER_TAG, TAG_LEN)){
		syslog(LOG_ERR,"Invild wlan hw setting signature %s!\n",header.signature);
		return -1;
	}
	len = header.len;
	return len;
	
}

static int hex_to_string(unsigned char *hex,char *str,int len)
{
	int i;
	char *d,*s;
	const static char hexdig[] = "0123456789abcdef";
	if(hex == NULL||str == NULL)
		return -1;
	d = str;
	s = hex;
	
	for(i = 0;i < len;i++,s++){
		*d++ = hexdig[(*s >> 4) & 0xf];
		*d++ = hexdig[*s & 0xf];
	}
	*d = 0;
	return 0;
}
#if defined(HAVE_RTK_AC_SUPPORT) || defined(HAVE_RTK_4T4R_AC_SUPPORT)

#define B1_G1	40
#define B1_G2	48

#define B2_G1	56
#define B2_G2	64

#define B3_G1	104
#define B3_G2	112
#define B3_G3	120
#define B3_G4	128
#define B3_G5	136
#define B3_G6	144

#define B4_G1	153
#define B4_G2	161
#define B4_G3	169
#define B4_G4	177

void assign_diff_AC(unsigned char* pMib, unsigned char* pVal)
{
	int x=0, y=0;

	memset((pMib+35), pVal[0], (B1_G1-35));
	memset((pMib+B1_G1), pVal[1], (B1_G2-B1_G1));
	memset((pMib+B1_G2), pVal[2], (B2_G1-B1_G2));
	memset((pMib+B2_G1), pVal[3], (B2_G2-B2_G1));
	memset((pMib+B2_G2), pVal[4], (B3_G1-B2_G2));
	memset((pMib+B3_G1), pVal[5], (B3_G2-B3_G1));
	memset((pMib+B3_G2), pVal[6], (B3_G3-B3_G2));
	memset((pMib+B3_G3), pVal[7], (B3_G4-B3_G3));
	memset((pMib+B3_G4), pVal[8], (B3_G5-B3_G4));
	memset((pMib+B3_G5), pVal[9], (B3_G6-B3_G5));
	memset((pMib+B3_G6), pVal[10], (B4_G1-B3_G6));
	memset((pMib+B4_G1), pVal[11], (B4_G2-B4_G1));
	memset((pMib+B4_G2), pVal[12], (B4_G3-B4_G2));
	memset((pMib+B4_G3), pVal[13], (B4_G4-B4_G3));

}
void assign_diff_AC_hex_to_string(unsigned char* pmib,char* str,int len)
{
	char mib_buf[MAX_5G_CHANNEL_NUM_MIB];
	memset(mib_buf,0,sizeof(mib_buf));
	assign_diff_AC(mib_buf, pmib);
	hex_to_string(mib_buf,str,MAX_5G_CHANNEL_NUM_MIB);
}
#endif

int set_led_type(HW_WLAN_SETTING_Tp phw)
{
	unsigned char tmpbuff[100] = {0};    
#ifdef HAVE_RTK_DUAL_BAND_SUPPORT
	HW_WLAN_SETTING_Tp phw_5g = (HW_WLAN_SETTING_Tp)((unsigned char *)phw+sizeof(HW_WLAN_SETTING_T));
#endif

	sprintf(tmpbuff,"iwpriv wlan0 set_mib led_type=%d",phw->ledType);
	system(tmpbuff);
	printf("%s\n",tmpbuff);
#ifdef HAVE_RTK_DUAL_BAND_SUPPORT
	sprintf(tmpbuff,"iwpriv wlan1 set_mib led_type=%d",phw_5g->ledType);
	system(tmpbuff);
	printf("%s\n",tmpbuff);
#endif
	return 0;
}

int set_tx_calibration(HW_WLAN_SETTING_Tp phw,char* interface)
{
	char tmpbuff[1024],p[MAX_5G_CHANNEL_NUM_MIB*2+1];
	if(!phw)
		return -1;

	sprintf(tmpbuff,"iwpriv %s set_mib ther=%d",interface,phw->Ther);
 	system(tmpbuff);
	printf("%s\n",tmpbuff);

	sprintf(tmpbuff,"iwpriv %s set_mib xcap=%d",interface,phw->xCap);
 	system(tmpbuff);
	printf("%s\n",tmpbuff);

	hex_to_string(phw->pwrlevelCCK_A,p,MAX_2G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrlevelCCK_A=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);
	
	hex_to_string(phw->pwrlevelCCK_B,p,MAX_2G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrlevelCCK_B=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);
	
	hex_to_string(phw->pwrlevelHT40_1S_A,p,MAX_2G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrlevelHT40_1S_A=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);
	
	hex_to_string(phw->pwrlevelHT40_1S_B,p,MAX_2G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrlevelHT40_1S_B=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);
	
	hex_to_string(phw->pwrdiffHT40_2S,p,MAX_2G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiffHT40_2S=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);
	
	hex_to_string(phw->pwrdiffHT20,p,MAX_2G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiffHT20=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);
	
	hex_to_string(phw->pwrdiffOFDM,p,MAX_2G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiffOFDM=%s",interface,p);
	system(tmpbuff);

	hex_to_string(phw->pwrlevel5GHT40_1S_A,p,MAX_5G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrlevel5GHT40_1S_A=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);
	
	hex_to_string(phw->pwrlevel5GHT40_1S_B,p,MAX_5G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrlevel5GHT40_1S_B=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

#if defined(HAVE_RTK_92D_SUPPORT) || defined(HAVE_RTK_AC_SUPPORT)
	hex_to_string(phw->pwrdiff5GHT40_2S,p,MAX_5G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff5GHT40_2S=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);
	
	hex_to_string(phw->pwrdiff5GHT20,p,MAX_5G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff5GHT20=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	hex_to_string(phw->pwrdiff5GOFDM,p,MAX_5G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff5GOFDM=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);
#endif

#if defined(HAVE_RTK_AC_SUPPORT) || defined(HAVE_RTK_4T4R_AC_SUPPORT)
	hex_to_string(phw->pwrdiff_20BW1S_OFDM1T_A,p,MAX_2G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_20BW1S_OFDM1T_A=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);
	
	hex_to_string(phw->pwrdiff_40BW2S_20BW2S_A,p,MAX_2G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_40BW2S_20BW2S_A=%s",interface,p);
	system(tmpbuff);	
	printf("%s\n",tmpbuff);
	
	assign_diff_AC_hex_to_string(phw->pwrdiff_5G_20BW1S_OFDM1T_A,p,MAX_5G_DIFF_NUM);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_5G_20BW1S_OFDM1T_A=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	assign_diff_AC_hex_to_string(phw->pwrdiff_5G_40BW2S_20BW2S_A,p,MAX_5G_DIFF_NUM);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_5G_40BW2S_20BW2S_A=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	assign_diff_AC_hex_to_string(phw->pwrdiff_5G_80BW1S_160BW1S_A,p,MAX_5G_DIFF_NUM);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_5G_80BW1S_160BW1S_A=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);
	
	assign_diff_AC_hex_to_string(phw->pwrdiff_5G_80BW2S_160BW2S_A,p,MAX_5G_DIFF_NUM);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_5G_80BW2S_160BW2S_A=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);
	
	hex_to_string(phw->pwrdiff_20BW1S_OFDM1T_B,p,MAX_2G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_20BW1S_OFDM1T_B=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);
	
	hex_to_string(phw->pwrdiff_40BW2S_20BW2S_B,p,MAX_2G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_40BW2S_20BW2S_B=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);	
	
	assign_diff_AC_hex_to_string(phw->pwrdiff_5G_20BW1S_OFDM1T_B,p,MAX_5G_DIFF_NUM);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_5G_20BW1S_OFDM1T_B=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);
	
	assign_diff_AC_hex_to_string(phw->pwrdiff_5G_40BW2S_20BW2S_B,p,MAX_5G_DIFF_NUM);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_5G_40BW2S_20BW2S_B=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);
	
	assign_diff_AC_hex_to_string(phw->pwrdiff_5G_80BW1S_160BW1S_B,p,MAX_5G_DIFF_NUM);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_5G_80BW1S_160BW1S_B=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);
	
	assign_diff_AC_hex_to_string(phw->pwrdiff_5G_80BW2S_160BW2S_B,p,MAX_5G_DIFF_NUM);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_5G_80BW2S_160BW2S_B=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);
#endif

//#if defined(CONFIG_WLAN_HAL_8814AE)
#if defined(HAVE_RTK_4T4R_AC_SUPPORT)
            //3 5G  
	//apmib_get(MIB_HW_TX_POWER_DIFF_5G_40BW3S_20BW3S_A, (void *)buf1);
	//assign_diff_AC(pmib->dot11RFEntry.pwrdiff_5G_40BW3S_20BW3S_A, (unsigned char*)buf1);
	assign_diff_AC_hex_to_string(phw->pwrdiff_5G_40BW3S_20BW3S_A,p,MAX_5G_DIFF_NUM);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_5G_40BW3S_20BW3S_A=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//apmib_get(MIB_HW_TX_POWER_DIFF_5G_80BW3S_160BW3S_A, (void *)buf1);
	//assign_diff_AC(pmib->dot11RFEntry.pwrdiff_5G_80BW3S_160BW3S_A, (unsigned char*)buf1);
	assign_diff_AC_hex_to_string(phw->pwrdiff_5G_80BW3S_160BW3S_A,p,MAX_5G_DIFF_NUM);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_5G_80BW3S_160BW3S_A=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//apmib_get(MIB_HW_TX_POWER_DIFF_5G_40BW3S_20BW3S_B, (void *)buf1);
	//assign_diff_AC(pmib->dot11RFEntry.pwrdiff_5G_40BW3S_20BW3S_B, (unsigned char*)buf1);
	assign_diff_AC_hex_to_string(phw->pwrdiff_5G_40BW3S_20BW3S_B,p,MAX_5G_DIFF_NUM);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_5G_40BW3S_20BW3S_B=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//apmib_get(MIB_HW_TX_POWER_DIFF_5G_80BW3S_160BW3S_B, (void *)buf1);
	//assign_diff_AC(pmib->dot11RFEntry.pwrdiff_5G_80BW3S_160BW3S_B, (unsigned char*)buf1);
	assign_diff_AC_hex_to_string(phw->pwrdiff_5G_80BW3S_160BW3S_B,p,MAX_5G_DIFF_NUM);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_5G_80BW3S_160BW3S_B=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//apmib_get(MIB_HW_TX_POWER_DIFF_5G_20BW1S_OFDM1T_C, (void *)buf1);
	//assign_diff_AC(pmib->dot11RFEntry.pwrdiff_5G_20BW1S_OFDM1T_C, (unsigned char*) buf1);
	assign_diff_AC_hex_to_string(phw->pwrdiff_5G_20BW1S_OFDM1T_C,p,MAX_5G_DIFF_NUM);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_5G_20BW1S_OFDM1T_C=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//apmib_get(MIB_HW_TX_POWER_DIFF_5G_40BW2S_20BW2S_C, (void *)buf1);
	//assign_diff_AC(pmib->dot11RFEntry.pwrdiff_5G_40BW2S_20BW2S_C, (unsigned char*)buf1);
	assign_diff_AC_hex_to_string(phw->pwrdiff_5G_40BW2S_20BW2S_C,p,MAX_5G_DIFF_NUM);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_5G_40BW2S_20BW2S_C=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//apmib_get(MIB_HW_TX_POWER_DIFF_5G_80BW1S_160BW1S_C, (void *)buf1);
	//assign_diff_AC(pmib->dot11RFEntry.pwrdiff_5G_80BW1S_160BW1S_C, (unsigned char*)buf1);
	assign_diff_AC_hex_to_string(phw->pwrdiff_5G_80BW1S_160BW1S_C,p,MAX_5G_DIFF_NUM);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_5G_80BW1S_160BW1S_C=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//apmib_get(MIB_HW_TX_POWER_DIFF_5G_80BW2S_160BW2S_C, (void *)buf1);
	//assign_diff_AC(pmib->dot11RFEntry.pwrdiff_5G_80BW2S_160BW2S_C, (unsigned char*)buf1);
	assign_diff_AC_hex_to_string(phw->pwrdiff_5G_80BW2S_160BW2S_C,p,MAX_5G_DIFF_NUM);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_5G_80BW2S_160BW2S_C=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//apmib_get(MIB_HW_TX_POWER_DIFF_5G_40BW3S_20BW3S_C, (void *)buf1);
	//assign_diff_AC(pmib->dot11RFEntry.pwrdiff_5G_40BW3S_20BW3S_C, (unsigned char*)buf1);
	assign_diff_AC_hex_to_string(phw->pwrdiff_5G_40BW3S_20BW3S_C,p,MAX_5G_DIFF_NUM);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_5G_40BW3S_20BW3S_C=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//apmib_get(MIB_HW_TX_POWER_DIFF_5G_80BW3S_160BW3S_C, (void *)buf1);
	//assign_diff_AC(pmib->dot11RFEntry.pwrdiff_5G_80BW3S_160BW3S_C, (unsigned char*)buf1);
	assign_diff_AC_hex_to_string(phw->pwrdiff_5G_80BW3S_160BW3S_C,p,MAX_5G_DIFF_NUM);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_5G_80BW3S_160BW3S_C=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//apmib_get(MIB_HW_TX_POWER_DIFF_5G_20BW1S_OFDM1T_D, (void *)buf1);
	//assign_diff_AC(pmib->dot11RFEntry.pwrdiff_5G_20BW1S_OFDM1T_D, (unsigned char*)buf1);
	assign_diff_AC_hex_to_string(phw->pwrdiff_5G_20BW1S_OFDM1T_D,p,MAX_5G_DIFF_NUM);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_5G_20BW1S_OFDM1T_D=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//apmib_get(MIB_HW_TX_POWER_DIFF_5G_40BW2S_20BW2S_D, (void *)buf1);
	//assign_diff_AC(pmib->dot11RFEntry.pwrdiff_5G_40BW2S_20BW2S_D, (unsigned char*)buf1);
	assign_diff_AC_hex_to_string(phw->pwrdiff_5G_40BW2S_20BW2S_D,p,MAX_5G_DIFF_NUM);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_5G_40BW2S_20BW2S_D=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//apmib_get(MIB_HW_TX_POWER_DIFF_5G_40BW3S_20BW3S_D, (void *)buf1);
	//assign_diff_AC(pmib->dot11RFEntry.pwrdiff_5G_40BW3S_20BW3S_D, (unsigned char*)buf1);
	assign_diff_AC_hex_to_string(phw->pwrdiff_5G_40BW3S_20BW3S_D,p,MAX_5G_DIFF_NUM);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_5G_40BW3S_20BW3S_D=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//apmib_get(MIB_HW_TX_POWER_DIFF_5G_80BW1S_160BW1S_D, (void *)buf1);
	//assign_diff_AC(pmib->dot11RFEntry.pwrdiff_5G_80BW1S_160BW1S_D, (unsigned char*)buf1);
	assign_diff_AC_hex_to_string(phw->pwrdiff_5G_80BW1S_160BW1S_D,p,MAX_5G_DIFF_NUM);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_5G_80BW1S_160BW1S_D=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//apmib_get(MIB_HW_TX_POWER_DIFF_5G_80BW2S_160BW2S_D, (void *)buf1);
	//assign_diff_AC(pmib->dot11RFEntry.pwrdiff_5G_80BW2S_160BW2S_D, (unsigned char*)buf1);
	assign_diff_AC_hex_to_string(phw->pwrdiff_5G_80BW2S_160BW2S_D,p,MAX_5G_DIFF_NUM);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_5G_80BW2S_160BW2S_D=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//apmib_get(MIB_HW_TX_POWER_DIFF_5G_80BW3S_160BW3S_D, (void *)buf1);
	//assign_diff_AC(pmib->dot11RFEntry.pwrdiff_5G_80BW3S_160BW3S_D, (unsigned char*)buf1);
	assign_diff_AC_hex_to_string(phw->pwrdiff_5G_80BW3S_160BW3S_D,p,MAX_5G_DIFF_NUM);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_5G_80BW3S_160BW3S_D=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);
	//3 2G

	//apmib_get(MIB_HW_TX_POWER_HT40_1S_C, (void *)buf1);
	//memcpy(pmib->dot11RFEntry.pwrlevelHT40_1S_C, buf1, MAX_2G_CHANNEL_NUM_MIB);
	hex_to_string(phw->pwrlevelHT40_1S_C,p,MAX_2G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrlevelHT40_1S_C=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//apmib_get(MIB_HW_TX_POWER_HT40_1S_D, (void *)buf1);
	//memcpy(pmib->dot11RFEntry.pwrlevelHT40_1S_D, buf1, MAX_2G_CHANNEL_NUM_MIB);
	hex_to_string(phw->pwrlevelHT40_1S_D,p,MAX_2G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrlevelHT40_1S_D=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//apmib_get(MIB_HW_TX_POWER_CCK_C, (void *)buf1);
	//memcpy(pmib->dot11RFEntry.pwrlevelCCK_C, buf1, MAX_2G_CHANNEL_NUM_MIB);
	hex_to_string(phw->pwrlevelCCK_C,p,MAX_2G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrlevelCCK_C=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//apmib_get(MIB_HW_TX_POWER_CCK_D, (void *)buf1);
	//memcpy(pmib->dot11RFEntry.pwrlevelCCK_D, buf1, MAX_2G_CHANNEL_NUM_MIB);
	hex_to_string(phw->pwrlevelCCK_D,p,MAX_2G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrlevelCCK_D=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//apmib_get(MIB_HW_TX_POWER_DIFF_40BW3S_20BW3S_A, (void *)buf1);
	//memcpy(pmib->dot11RFEntry.pwrdiff_40BW3S_20BW3S_A, buf1, MAX_2G_CHANNEL_NUM_MIB);
    hex_to_string(phw->pwrdiff_40BW3S_20BW3S_A,p,MAX_2G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_40BW3S_20BW3S_A=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//apmib_get(MIB_HW_TX_POWER_DIFF_40BW3S_20BW3S_B, (void *)buf1);
	//memcpy(pmib->dot11RFEntry.pwrdiff_40BW3S_20BW3S_B, buf1, MAX_2G_CHANNEL_NUM_MIB);
	hex_to_string(phw->pwrdiff_40BW3S_20BW3S_B,p,MAX_2G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_40BW3S_20BW3S_B=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//apmib_get(MIB_HW_TX_POWER_DIFF_20BW1S_OFDM1T_C, (void *)buf1);
	//memcpy(pmib->dot11RFEntry.pwrdiff_20BW1S_OFDM1T_C, buf1, MAX_2G_CHANNEL_NUM_MIB);
	hex_to_string(phw->pwrdiff_20BW1S_OFDM1T_C,p,MAX_2G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_20BW1S_OFDM1T_C=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//apmib_get(MIB_HW_TX_POWER_DIFF_40BW2S_20BW2S_C, (void *)buf1);
	//memcpy(pmib->dot11RFEntry.pwrdiff_40BW2S_20BW2S_C, buf1, MAX_2G_CHANNEL_NUM_MIB);
	hex_to_string(phw->pwrdiff_40BW2S_20BW2S_C,p,MAX_2G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_40BW2S_20BW2S_C=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//apmib_get(MIB_HW_TX_POWER_DIFF_40BW3S_20BW3S_C, (void *)buf1);
	//memcpy(pmib->dot11RFEntry.pwrdiff_40BW3S_20BW3S_C, buf1, MAX_2G_CHANNEL_NUM_MIB);
	hex_to_string(phw->pwrdiff_40BW3S_20BW3S_C,p,MAX_2G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_40BW3S_20BW3S_C=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//apmib_get(MIB_HW_TX_POWER_DIFF_20BW1S_OFDM1T_D, (void *)buf1);
	//memcpy(pmib->dot11RFEntry.pwrdiff_20BW1S_OFDM1T_D, buf1, MAX_2G_CHANNEL_NUM_MIB);
	hex_to_string(phw->pwrdiff_20BW1S_OFDM1T_D,p,MAX_2G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_20BW1S_OFDM1T_D=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//apmib_get(MIB_HW_TX_POWER_DIFF_40BW2S_20BW2S_D, (void *)buf1);
	//memcpy(pmib->dot11RFEntry.pwrdiff_40BW2S_20BW2S_D, buf1, MAX_2G_CHANNEL_NUM_MIB);
	hex_to_string(phw->pwrdiff_40BW2S_20BW2S_D,p,MAX_2G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_40BW2S_20BW2S_D=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//apmib_get(MIB_HW_TX_POWER_DIFF_40BW3S_20BW3S_D, (void *)buf1);
	//memcpy(pmib->dot11RFEntry.pwrdiff_40BW3S_20BW3S_D, buf1, MAX_2G_CHANNEL_NUM_MIB);
	hex_to_string(phw->pwrdiff_40BW3S_20BW3S_D,p,MAX_2G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrdiff_40BW3S_20BW3S_D=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//apmib_get(MIB_HW_TX_POWER_5G_HT40_1S_C, (void *)buf1);
	//memcpy(pmib->dot11RFEntry.pwrlevel5GHT40_1S_C, buf1, MAX_5G_CHANNEL_NUM_MIB);
	hex_to_string(phw->pwrlevel5GHT40_1S_C,p,MAX_5G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrlevel5GHT40_1S_C=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//apmib_get(MIB_HW_TX_POWER_5G_HT40_1S_D, (void *)buf1);
	//memcpy(pmib->dot11RFEntry.pwrlevel5GHT40_1S_D, buf1, MAX_5G_CHANNEL_NUM_MIB);
	hex_to_string(phw->pwrlevel5GHT40_1S_D,p,MAX_5G_CHANNEL_NUM_MIB);
	sprintf(tmpbuff,"iwpriv %s set_mib pwrlevel5GHT40_1S_D=%s",interface,p);
	system(tmpbuff);
	printf("%s\n",tmpbuff);

	//printf("pmib->dot11RFEntry.pwrlevel5GHT40_1S_A=%s\n",pmib->dot11RFEntry.pwrlevel5GHT40_1S_A);
	//printf("pmib->dot11RFEntry.pwrlevel5GHT40_1S_B=%s\n",pmib->dot11RFEntry.pwrlevel5GHT40_1S_B);
	//printf("pmib->dot11RFEntry.pwrlevel5GHT40_1S_C=%s\n",pmib->dot11RFEntry.pwrlevel5GHT40_1S_C);
	//printf("pmib->dot11RFEntry.pwrlevel5GHT40_1S_D=%s\n",pmib->dot11RFEntry.pwrlevel5GHT40_1S_D);
#endif
	return 0;
}


int set_wifi_mac(char* buf)
{
	unsigned int offset, wifi_count;
	unsigned char tmpbuff[64];
	unsigned char mactmp[ETH_ALEN+1];
	int idx;

	/* wifi mac */
	memset(mactmp,0,ETH_ALEN+1);
	idx = 0;
	offset = HW_WLAN_SETTING_OFFSET + sizeof(struct hw_wlan_setting) * idx;
	if(!memcmp(mactmp,(unsigned char *)(buf+offset),ETH_ALEN)) {
		printf("wlan0's MAC Adddress is not calibrated, dismissed!!");
	} else {
		memcpy(mactmp,(unsigned char *)(buf+offset),ETH_ALEN);	
		sprintf(tmpbuff,"ifconfig wlan0 hw ether %02x%02x%02x%02x%02x%02x",mactmp[0],mactmp[1],mactmp[2],mactmp[3],mactmp[4],mactmp[5]);
		//printf("cmd=%s\n",tmpbuff);
		system(tmpbuff);
	}
//#if defined(HAVE_WIFI_MBSSID)
#if 0
	for(wifi_count=0;wifi_count<7;wifi_count++){
		memset(mactmp,0,ETH_ALEN+1);
		if(!memcmp(mactmp,(unsigned char *)(buf+offset+ETH_ALEN*(wifi_count+1)),ETH_ALEN)) {
			printf("wlan0-%d's MAC Adddress is not calibrated, dismissed!!",wifi_count+1);
		} else {
			memcpy(mactmp,(unsigned char *)(buf+offset+ETH_ALEN*(wifi_count+1)),ETH_ALEN);	
			sprintf(tmpbuff,"ifconfig wlan0-%d hw ether %02x%02x%02x%02x%02x%02x",wifi_count+1,mactmp[0],mactmp[1],mactmp[2],mactmp[3],mactmp[4],mactmp[5]);
			//printf("cmd=%s\n",tmpbuff);
			system(tmpbuff);
		}
	}
#endif
#ifdef HAVE_RTK_DUAL_BAND_SUPPORT
	memset(mactmp,0,ETH_ALEN+1);
	idx = 1;
	offset = HW_WLAN_SETTING_OFFSET + sizeof(struct hw_wlan_setting) * idx;
	if(!memcmp(mactmp,(unsigned char *)(buf+offset),ETH_ALEN)) {
		printf("wlan1's MAC Adddress is not calibrated, dismissed!!");
	} else {
		memcpy(mactmp,(unsigned char *)(buf+offset),ETH_ALEN);	
		sprintf(tmpbuff,"ifconfig wlan1 hw ether %02x%02x%02x%02x%02x%02x",mactmp[0],mactmp[1],mactmp[2],mactmp[3],mactmp[4],mactmp[5]);
		//printf("cmd=%s\n",tmpbuff);
		system(tmpbuff);
	}
//#if defined(HAVE_WIFI_MBSSID)
#if 0
	for(wifi_count=0;wifi_count<7;wifi_count++){
		memset(mactmp,0,ETH_ALEN+1);
		if(!memcmp(mactmp,(unsigned char *)(buf+offset+ETH_ALEN*(wifi_count+1)),ETH_ALEN)) {
			printf("wlan1-%d's MAC Adddress is not calibrated, dismissed!!",wifi_count+1);
		} else {
			memcpy(mactmp,(unsigned char *)(buf+offset+ETH_ALEN*(wifi_count+1)),ETH_ALEN);   
			sprintf(tmpbuff,"ifconfig wlan1-%d hw ether %02x%02x%02x%02x%02x%02x",wifi_count+1,mactmp[0],mactmp[1],mactmp[2],mactmp[3],mactmp[4],mactmp[5]);
			//printf("cmd=%s\n",tmpbuff);
			system(tmpbuff);
		}
	}
#endif	//MBSSID
#endif	//HAVE_RTK_DUAL_BAND_SUPPORT
}

int set_ethernet_mac(char* buf)
{
	unsigned int offset;
    unsigned char tmpbuff[64];
    unsigned char mactmp[ETH_ALEN+1];
    int idx;

    memset(mactmp,0,ETH_ALEN+1);
    /* ethernet mac */
    
    idx = 0;
    offset = HW_SETTING_ETHMAC_OFFSET + idx*ETH_ALEN;

	if(memcmp(mactmp,(unsigned char *)(buf+offset),ETH_ALEN)) {
		memcpy(mactmp,(unsigned char *)(buf+offset),ETH_ALEN);
		sprintf(tmpbuff,"ifconfig br-lan hw ether %02x%02x%02x%02x%02x%02x",mactmp[0],mactmp[1],mactmp[2],mactmp[3],mactmp[4],mactmp[5]);
		system(tmpbuff);
		sprintf(tmpbuff,"ifconfig eth0 hw ether %02x%02x%02x%02x%02x%02x",mactmp[0],mactmp[1],mactmp[2],mactmp[3],mactmp[4],mactmp[5]);
		system(tmpbuff);
	} else {
		printf("br-lan/eth0's MAC Adddress is not calibrated, aborded!!\n");
	}
	memset(mactmp,0,ETH_ALEN+1);
    idx = 1;
    offset = HW_SETTING_ETHMAC_OFFSET + idx*ETH_ALEN;
	if(memcmp(mactmp,(unsigned char *)(buf+offset),ETH_ALEN)) {
	    memcpy(mactmp,(unsigned char *)(buf+offset),ETH_ALEN);
    	sprintf(tmpbuff,"ifconfig eth1 hw ether %02x%02x%02x%02x%02x%02x",mactmp[0],mactmp[1],mactmp[2],mactmp[3],mactmp[4],mactmp[5]);
		system(tmpbuff);
	} else {
		printf("eth1's MAC Adddress is not calibrated, aborded!!\n");
	}
}

int main(int argc, char *argv[])
{
	PARAM_HEADER_T header;
	char *buf = NULL;
	int hw_len = 0;
	HW_WLAN_SETTING_Tp phw;
	
	int ch,cmd = 0;
	while((ch = getopt(argc,argv,"we")) != -1)
	{
		switch(ch)
		{
		case 'w':
			cmd = CMD_SET_WIFI;
		break;
		case 'e':
			cmd = CMD_SET_ETHERNET;
		break;
		default:
			printf("cmd not support\n");
		}	
	}


#ifdef CONFIG_MTD_NAND
	if(cmd == CMD_SET_ETHERNET){
		if(access("/hw_setting",0) != 0)
		{
			system("mkdir /hw_setting;chmod 777 /hw_setting");
		}
		system("mount -t yaffs2 -o tags-ecc-off -o inband-tags /dev/mtdblock1 /hw_setting");
	}
#endif
	read_hw_setting_offset();	//mark_hw	
	hw_len = read_hw_setting_length();
#if 0
	if(hw_len < NUM_WLAN_INTERFACE*sizeof(HW_WLAN_SETTING_T)+HW_WLAN_SETTING_OFFSET){
		syslog(LOG_ERR,"Wlan HW setting length invalid!\n");
		return -1;
	}
#endif
	buf = malloc(hw_len);
	if(!buf){
		syslog(LOG_ERR,"Can't allocate memory for reading wlan HW setting!\n");
		return -1;
	}
	if(read_hw_setting(buf) < 0){
		free(buf);
		return -1;
	}
	
	if(cmd == CMD_SET_ETHERNET)
		set_ethernet_mac(buf);
	else if(cmd == CMD_SET_WIFI){
		phw = (HW_WLAN_SETTING_Tp)(buf+HW_WLAN_SETTING_OFFSET);
		set_led_type(phw);
#ifdef HAVE_RTK_EFUSE
		syslog(LOG_WARNING,"Efuse enabled. Calibration aborded!\n");
#else
		set_wifi_mac(buf);
		set_tx_calibration(phw,"wlan0");
#ifdef HAVE_RTK_DUAL_BAND_SUPPORT
		phw = (HW_WLAN_SETTING_Tp)(buf+HW_WLAN_SETTING_OFFSET+sizeof(HW_WLAN_SETTING_T));
		set_tx_calibration(phw,"wlan1");
#endif
#endif
	}
	free(buf);

	
#ifdef CONFIG_MTD_NAND
	if(cmd == CMD_SET_WIFI){
		system("umount /hw_setting");
		system("rm -rf /hw_setting");
	}
#endif
	return 0;	
}

