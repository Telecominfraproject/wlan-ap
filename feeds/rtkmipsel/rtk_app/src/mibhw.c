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
#include "mibtbl.h"
#include "apmib.h"
#ifndef _DEBUG_
#include <syslog.h>
#else
#define syslog(x,fmt,args...) printf(fmt,## args)
#endif

#define PIN_LEN					8
#define HW_WLAN_SETTING_OFFSET	13

#define SIGNATURE_LEN			4

/* Config file header */
#if 0
#ifdef RTK_HW_OFFSET
#define HW_SETTING_OFFSET		RTK_HW_OFFSET
#else
#define HW_SETTING_OFFSET		0x6000
#endif
#endif

typedef enum {
	MIB_READ = 1,
	MIB_WRITE,
	MIB_SHOW,
	MIB_DEFAULT
} FLASH_OP_T;

static unsigned int hw_setting_off=0x6000;

static void read_hw_setting_offset(void) 
{
	FILE *hwpart_proc;
	hwpart_proc = fopen ( "/proc/flash/hwpart", "r" );
	if ( hwpart_proc != NULL )
	{
		char buf[16];

		fgets(buf, sizeof(buf), hwpart_proc);        /* eat line */
		sscanf(buf, "%x",&hw_setting_off);
#ifndef CONFIG_MTD_NAND
		if(hw_setting_off == 0)
			hw_setting_off = HW_SETTING_OFFSET;
#endif
		fclose(hwpart_proc);
		//printf("read_hw_setting_offset = %x \n",hw_setting_off);
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

static int flash_write(char *buf, int offset, int len)
{
	int fh;
	int ok=1;

#ifdef CONFIG_MTD_NAND
	fh = open(FLASH_DEVICE_NAME, O_RDWR|O_CREAT);
#else
	fh = open(FLASH_DEVICE_NAME, O_RDWR);
#endif

	if ( fh == -1 )
		return 0;

	lseek(fh, offset, SEEK_SET);

	if ( write(fh, buf, len) != len)
		ok = 0;

	close(fh);
	sync();

	return ok;
}

static int reset_hw_setting(void)
{
	PARAM_HEADER_Tp header;
	unsigned char *buf = NULL, checksum;;

	buf = calloc(1, sizeof(PARAM_HEADER_T)+sizeof(HW_SETTING_T));

	if(buf) {
		//printf("Flash offset:%lx, Write with Tag:%s\n",hw_setting_off,HW_SETTING_HEADER_TAG);
		header = (PARAM_HEADER_Tp)buf;
		sprintf((char *)header->signature, "%s%02d", HW_SETTING_HEADER_TAG, HW_SETTING_VER);
		header->len = sizeof(HW_SETTING_T);

		flash_write(buf, hw_setting_off, header->len+sizeof(PARAM_HEADER_T));
		printf("Reset HW settings done\n");
		free(buf);
	} else {
		printf("Allocate memory for reset HW settings failed\n");
	}

	return 0;	
}

static int load_hw_setting(char *buf)
{
	PARAM_HEADER_T header;
    
	if(flash_read(&header,hw_setting_off,sizeof(PARAM_HEADER_T))==0){
		printf("Read wlan hw setting header failed\n");
		return -1;
	}
	//printf("Tag:%s, match with %s; Length:%d\n",header.signature,HW_SETTING_HEADER_TAG,header.len);
	if(memcmp(header.signature, HW_SETTING_HEADER_TAG, TAG_LEN)){
		printf("Invild wlan hw setting signature!\n");
		return -1;
	}
	if(flash_read(buf,hw_setting_off+sizeof(PARAM_HEADER_T),header.len)==0){
		printf("Read wlan hw setting to memory failed\n");
		
		return -1;
	}
	return 0;
	
}

static int read_hw_setting_length()
{
	PARAM_HEADER_T header;
	int len, reason=0;
	unsigned short target_signature[SIGNATURE_LEN+1];

	if(flash_read(&header,hw_setting_off,sizeof(PARAM_HEADER_T))==0){
		printf("Read wlan hw setting header failed\n");
		return -1;
	}

	sprintf(target_signature,"%s%02d",HW_SETTING_HEADER_TAG,HW_SETTING_VER);
	if((reason = memcmp(header.signature, target_signature, SIGNATURE_LEN)) ||
		(reason = ((header.len < sizeof(HW_SETTING_T))? 2:0)) ) {
		printf("Invild wlan hw setting signature %s, reset!\n",header.signature);
		printf("Reset reason: %s\n", (reason==2)? "Length is too short!":"Signature mismatch!");
		reset_hw_setting();
		flash_read(&header,hw_setting_off,sizeof(PARAM_HEADER_T));
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

static int _is_hex(char c)
{
    return (((c >= '0') && (c <= '9')) ||
            ((c >= 'A') && (c <= 'F')) ||
            ((c >= 'a') && (c <= 'f')));
}

static int string_to_hex(char *string, unsigned char *key, int len)
{
	char tmpBuf[4];
	int idx, ii=0;
	for (idx=0; idx<len; idx+=2) {
		tmpBuf[0] = string[idx];
		tmpBuf[1] = string[idx+1];
		tmpBuf[2] = 0;
		if ( !_is_hex(tmpBuf[0]) || !_is_hex(tmpBuf[1]))
			return 0;

		key[ii++] = (unsigned char) strtol(tmpBuf, (char**)NULL, 16);
	}
	return 1;
}

static void convert_lower(char *str)
{
	int i,len = strlen(str);

	for (i=0; i<len; i++)
		str[i] = tolower(str[i]);
}

typedef enum {
        HW_MIB_AREA=1,
        HW_MIB_WLAN_AREA,
        DEF_MIB_AREA,
        DEF_MIB_WLAN_AREA,
        MIB_AREA,
        MIB_WLAN_AREA
} CONFIG_AREA_T;

static CONFIG_AREA_T config_area;

#define DEC_FORMAT	("%d")
#define BYTE5_FORMAT	("%02x%02x%02x%02x%02x")
#define BYTE6_FORMAT	("%02x%02x%02x%02x%02x%02x")
#define BYTE13_FORMAT	("%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x")
#define STR_FORMAT	("%s")

mib_table_entry_T hwmib_wlan_table[]={
#ifdef MIB_TLV
#define MIBDEF(_ctype,  _cname, _crepeat, _mib_name, _mib_type, _mib_parents_ctype, _default_value, _next_tbl ) \
                {_MIBHWID_NAME(_mib_name), _mib_type, _OFFSET_SIZE_FIELD(_mib_parents_ctype, _cname), _UNIT_SIZE(_ctype), _default_value, _next_tbl},
#else
#define MIBDEF(_ctype,  _cname, _crepeat, _mib_name, _mib_type, _mib_parents_ctype, _default_value, _next_tbl ) \
                        {_MIBHWID_NAME(_mib_name), _mib_type, FIELD_OFFSET(_mib_parents_ctype, _cname), FIELD_SIZE(_mib_parents_ctype, _cname)},
#endif
#define MIB_HW_WLAN_IMPORT
#include "mibdef.h"
#undef MIB_HW_WLAN_IMPORT

#undef MIBDEF
{0}
};

mib_table_entry_T hwmib_table[]={
#ifdef MIB_TLV
#define MIBDEF(_ctype,  _cname, _crepeat, _mib_name, _mib_type, _mib_parents_ctype, _default_value, _next_tbl ) \
                {_MIBHWID_NAME(_mib_name), _mib_type, _OFFSET_SIZE_FIELD(_mib_parents_ctype, _cname), _UNIT_SIZE(_ctype), _default_value, _next_tbl},
#else
#define MIBDEF(_ctype,  _cname, _crepeat, _mib_name, _mib_type, _mib_parents_ctype, _default_value, _next_tbl ) \
                        {_MIBHWID_NAME(_mib_name), _mib_type, FIELD_OFFSET(_mib_parents_ctype, _cname), FIELD_SIZE(_mib_parents_ctype, _cname)},
#endif
#define MIB_HW_IMPORT
#include "mibdef.h"
#undef MIB_HW_IMPORT

#undef MIBDEF
{0}
};

static int search_tbl(int id, mib_table_entry_T *pTbl, int *idx)
{
	int i;
	for (i=0; pTbl[i].id; i++) {
		if ( pTbl[i].id == id ) {
			*idx = i;
			return id;
		}
	}
	return 0;
}

int hwmib_get(int id, void *value, unsigned char *mibBuf, unsigned char wlan_index)
{
	int i, index;
	void *pMibTbl;
	mib_table_entry_T *pTbl;
	unsigned char ch;
	unsigned short wd;
	unsigned long dwd;
#ifdef MIB_TLV
	//unsigned int offset;
	unsigned int num;
#endif

	if ( search_tbl(id, hwmib_table, &i) ) {
		pMibTbl = (void *)mibBuf;
		pTbl = hwmib_table;
	}
	else if ( search_tbl(id, hwmib_wlan_table, &i) ) {
		pMibTbl = (void *)&(((HW_SETTING_Tp)mibBuf)->wlan[wlan_index]);
		pTbl = hwmib_wlan_table;
	} else {
		printf("HW MIB index:%d not found\n",id);
		return 0;
	}

	switch (pTbl[i].type) {
	case BYTE_T:
//		*((int *)value) =(int)(*((unsigned char *)(((long)pMibTbl) + pTbl[i].offset)));
		memcpy((char *)&ch, ((char *)pMibTbl) + pTbl[i].offset, 1);
		*((int *)value) = (int)ch;
		break;

	case BYTE6_T:
		memcpy( (unsigned char *)value, (unsigned char *)(((long)pMibTbl) + pTbl[i].offset), 6);
		break;

	case BYTE_ARRAY_T:
		memcpy( (unsigned char *)value, (unsigned char *)(((long)pMibTbl) + pTbl[i].offset), pTbl[i].size);
		break;

	case STRING_T:
		strcpy( (char *)value, (const char *)(((long)pMibTbl) + pTbl[i].offset) );
		break;

	default:
		break;
	}

	return 1;
}

static void getMIB(unsigned char *mibBuf, char *name, int id, TYPE_T type, int num, int array_separate, char **val, unsigned char wlan_idx)
{
	unsigned char array_val[2048];
	void *value;
	unsigned char tmpBuf[2048]={0}, *format=NULL, *buf, tmp1[400];
	int int_val, i;
	int index=1, tbl=0;
	char mibName[100]={0};

	if (num ==0)
		goto ret;

	strcat(mibName, name);

getval:
	buf = &tmpBuf[strlen((char *)tmpBuf)];
	switch (type) {
		case BYTE_T:
			value = (void *)&int_val;
			format = (unsigned char *)DEC_FORMAT;
			break;
		case BYTE6_T:
			value = (void *)array_val;
			format = (unsigned char *)BYTE6_FORMAT;
			break;
		case STRING_T:
			value = (void *)array_val;
			format = (unsigned char *)STR_FORMAT;
			break;
		case BYTE_ARRAY_T:
			value = (void *)array_val;
			break;
		default:
			printf("invalid mib!\n"); return;
	}

	if ( !hwmib_get(id, value, mibBuf, wlan_idx)) {
		printf("Get MIB failed!\n");
		return;
	}

	if (type == BYTE_T || type == WORD_T)
		sprintf((char *)buf, (char *)format, int_val);
	else if ( type == STRING_T ) {
		sprintf((char *)buf, (char *)format, value);
		
		if (type == STRING_T ) {
			unsigned char tmpBuf1[1024];
			int srcIdx, dstIdx;
			for (srcIdx=0, dstIdx=0; buf[srcIdx]; srcIdx++, dstIdx++) {
				if ( buf[srcIdx] == '"' || buf[srcIdx] == '\\' || buf[srcIdx] == '$' || buf[srcIdx] == '`' || buf[srcIdx] == ' ' )
					tmpBuf1[dstIdx++] = '\\';

				tmpBuf1[dstIdx] = buf[srcIdx];
			}
			if (dstIdx != srcIdx) {
				memcpy(buf, tmpBuf1, dstIdx);
				buf[dstIdx] ='\0';
			}
		}
	}
	else if (type == BYTE6_T ) {
		sprintf((char *)buf, (char *)format, array_val[0],array_val[1],array_val[2],
			array_val[3],array_val[4],array_val[5],array_val[6]);
		convert_lower((char *)buf);
	}
	else if(type == BYTE_ARRAY_T ) {
		int max_chan_num=MAX_2G_CHANNEL_NUM_MIB;
		int chan;

		if((id >= MIB_HW_TX_POWER_CCK_A &&  id <=MIB_HW_TX_POWER_DIFF_OFDM)
#if defined(CONFIG_WLAN_HAL_8814AE) || defined(HAVE_RTK_4T4R_AC_SUPPORT)
			||(id >= MIB_HW_TX_POWER_CCK_C &&  id <=MIB_HW_TX_POWER_CCK_D)
			||(id >= MIB_HW_TX_POWER_HT40_1S_C &&  id <=MIB_HW_TX_POWER_HT40_1S_D)
#endif
		)
			max_chan_num = MAX_2G_CHANNEL_NUM_MIB;
		else if((id >= MIB_HW_TX_POWER_5G_HT40_1S_A &&  id <=MIB_HW_TX_POWER_DIFF_5G_OFDM)
#if defined(CONFIG_WLAN_HAL_8814AE) || defined(HAVE_RTK_4T4R_AC_SUPPORT)
			||(id >= MIB_HW_TX_POWER_5G_HT40_1S_C &&  id <=MIB_HW_TX_POWER_5G_HT40_1S_D)
#endif
		)
			max_chan_num = MAX_5G_CHANNEL_NUM_MIB;
		else if(id == MIB_L2TP_PAYLOAD)
			max_chan_num = MAX_L2TP_BUFF_LEN;
		
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(HAVE_RTK_AC_SUPPORT) || defined(HAVE_RTK_4T4R_AC_SUPPORT)
		if(((id >= MIB_HW_TX_POWER_DIFF_20BW1S_OFDM1T_A) && (id <= MIB_HW_TX_POWER_DIFF_OFDM4T_CCK4T_A)) 
			|| ((id >= MIB_HW_TX_POWER_DIFF_20BW1S_OFDM1T_B) && (id <= MIB_HW_TX_POWER_DIFF_OFDM4T_CCK4T_B)) )
			max_chan_num = MAX_2G_CHANNEL_NUM_MIB;
		
		if(((id >= MIB_HW_TX_POWER_DIFF_5G_20BW1S_OFDM1T_A) && (id <= MIB_HW_TX_POWER_DIFF_5G_80BW4S_160BW4S_A)) 
			|| ((id >= MIB_HW_TX_POWER_DIFF_5G_20BW1S_OFDM1T_B) && (id <= MIB_HW_TX_POWER_DIFF_5G_80BW4S_160BW4S_B)) )
			max_chan_num = MAX_5G_DIFF_NUM;
#endif

#if defined(CONFIG_WLAN_HAL_8814AE) || defined(HAVE_RTK_4T4R_AC_SUPPORT)
		if(((id >= MIB_HW_TX_POWER_DIFF_20BW1S_OFDM1T_C) && (id <= MIB_HW_TX_POWER_DIFF_OFDM4T_CCK4T_C))
			|| ((id >= MIB_HW_TX_POWER_DIFF_20BW1S_OFDM1T_D) && (id <= MIB_HW_TX_POWER_DIFF_OFDM4T_CCK4T_D)) )
			max_chan_num = MAX_2G_CHANNEL_NUM_MIB;

		if(((id >= MIB_HW_TX_POWER_DIFF_5G_20BW1S_OFDM1T_C) && (id <= MIB_HW_TX_POWER_DIFF_5G_80BW4S_160BW4S_C))
			|| ((id >= MIB_HW_TX_POWER_DIFF_5G_20BW1S_OFDM1T_D) && (id <= MIB_HW_TX_POWER_DIFF_5G_80BW4S_160BW4S_D)) )
			max_chan_num = MAX_5G_DIFF_NUM;
#endif

		
		if(val == NULL || val[0] == NULL){
			for(i=0;i< max_chan_num;i++){
				sprintf((char *)tmp1, "%02x", array_val[i]);
				strcat((char *)buf, (char *)tmp1);
			}
			convert_lower((char *)buf);
		}
		else{
			chan = atoi(val[0]);
			if(chan < 1 || chan > max_chan_num){
				printf("invalid channel number\n");
				return;
			}
			sprintf((char *)buf, "%d", *(((unsigned char *)value)+chan-1) );
		}
	}

	if (--num > 0) {
		if (!array_separate)
			strcat((char *)tmpBuf, " ");
		else {
			if (tbl){
				if(type == STRING_T)
					printf("%s%d=\"%s\"\n", mibName, index-1, tmpBuf);
				else
					printf("%s%d=%s\n", mibName, index-1, tmpBuf);
			}
			else{
				if(type == STRING_T)
					printf("%s=\"%s\"\n", mibName, tmpBuf);
				else
					printf("%s=%s\n", mibName, tmpBuf);
			}
			tmpBuf[0] = '\0';
		}
		goto getval;
	}
ret:
	if (tbl) {
		if(type == STRING_T)
			printf("%s%d=\"%s\"\n", mibName, index-1, tmpBuf);
		else
			printf("%s%d=%s\n", mibName, index-1, tmpBuf);
	}
	else{
		if(type == STRING_T)
			printf("%s=\"%s\"\n", mibName, tmpBuf);
		else
			printf("%s=%s\n", mibName, tmpBuf);
	}
}

static void dumpAllHW(unsigned char *buf)
{
	int idx=0, num, wlan_idx=0;
	mib_table_entry_T *pTbl=NULL;
	config_area=0;

next_tbl:
	if (++config_area > HW_MIB_WLAN_AREA)
		return;
	if (config_area == HW_MIB_AREA)
		pTbl = hwmib_table;
	else if (config_area == HW_MIB_WLAN_AREA)
		pTbl = hwmib_wlan_table;

next_wlan:
	while (pTbl[idx].id) {
		num = 1;
		if (num >0) {
#ifdef MIB_TLV
			if(pTbl[idx].type == TABLE_LIST_T) // ignore table root entry. keith
			{
				idx++;
				continue;
			}
#endif // #ifdef MIB_TLV
			if ( config_area == HW_MIB_AREA || config_area == HW_MIB_WLAN_AREA)
			{
				printf("HW_");
				if (config_area == HW_MIB_WLAN_AREA) {
					printf("WLAN%d_", wlan_idx);
				}
			}
			getMIB(buf, pTbl[idx].name, pTbl[idx].id, pTbl[idx].type, num, 1 , NULL, wlan_idx);
		}
    	idx++;
    }
    idx = 0;

	if (config_area == HW_MIB_WLAN_AREA ) {
		if (++wlan_idx < NUM_WLAN_INTERFACE)
			goto next_wlan;
		else
			wlan_idx = 0;
	}
	goto next_tbl;
}

int hwmib_set(int id, void *value, unsigned char *mibBuf, unsigned char wlan_idx)
{
	int i, ret=1;
	void *pMibTbl;
	mib_table_entry_T *pTbl;
	unsigned char ch;
	unsigned short wd;
	unsigned long dwd;
	unsigned char* tmp;
	int max_chan_num=MAX_2G_CHANNEL_NUM_MIB;
#ifdef MIB_TLV
	//unsigned int offset=0;
	unsigned int mib_num_id=0;
	unsigned int num;
	unsigned int id_orig;
	#if defined(MIB_MOD_TBL_ENTRY)
	unsigned int mod_tbl=0;
	#endif
#endif

	if ( search_tbl(id, hwmib_table, &i) ) {
		pMibTbl = (void *)mibBuf;
		pTbl = hwmib_table;
	}
	else if ( search_tbl(id, hwmib_wlan_table, &i) ) {
		pMibTbl = (void *)&(((HW_SETTING_Tp)mibBuf)->wlan[wlan_idx]);
		pTbl = hwmib_wlan_table;
	}
	else {
		printf("id not found\n");
		ret=0;
        goto hwmib_set_errorout;
	}

	switch (pTbl[i].type) {
	case BYTE_T:
//		*((unsigned char *)(((long)pMibTbl) + pTbl[i].offset)) = (unsigned char)(*((int *)value));
		ch = (unsigned char)(*((int *)value));
		memcpy( ((char *)pMibTbl) + pTbl[i].offset, &ch, 1);
		break;

	case STRING_T:
		if ( strlen(value)+1 > pTbl[i].size )
		{
			return 0;
		}
		if (value==NULL || strlen(value)==0)
			*((char *)(((long)pMibTbl) + pTbl[i].offset)) = '\0';
		else
			strcpy((char *)(((long)pMibTbl) + pTbl[i].offset), (char *)value);
		break;

	case BYTE6_T:
		memcpy((unsigned char *)(((long)pMibTbl) + pTbl[i].offset), (unsigned char *)value, 6);
		break;

	case BYTE_ARRAY_T:
		tmp = (unsigned char *) value;
		
		{
			if((id >= MIB_HW_TX_POWER_CCK_A &&  id <=MIB_HW_TX_POWER_DIFF_OFDM)
#if defined(CONFIG_WLAN_HAL_8814AE) || defined(HAVE_RTK_4T4R_AC_SUPPORT)
				||(id >= MIB_HW_TX_POWER_CCK_C &&  id <=MIB_HW_TX_POWER_CCK_D)
				||(id >= MIB_HW_TX_POWER_HT40_1S_C &&  id <=MIB_HW_TX_POWER_HT40_1S_D)
#endif
			)
				max_chan_num = MAX_2G_CHANNEL_NUM_MIB;
			else if((id >= MIB_HW_TX_POWER_5G_HT40_1S_A &&  id <=MIB_HW_TX_POWER_DIFF_5G_OFDM)
#if defined(CONFIG_WLAN_HAL_8814AE) || defined(HAVE_RTK_4T4R_AC_SUPPORT)
					||(id >= MIB_HW_TX_POWER_5G_HT40_1S_C &&  id <=MIB_HW_TX_POWER_5G_HT40_1S_D)
#endif
				)
				max_chan_num = MAX_5G_CHANNEL_NUM_MIB;

#if defined(CONFIG_RTL_8812_SUPPORT) || defined(HAVE_RTK_AC_SUPPORT) || defined(HAVE_RTK_4T4R_AC_SUPPORT)
			if(((id >= MIB_HW_TX_POWER_DIFF_20BW1S_OFDM1T_A) && (id <= MIB_HW_TX_POWER_DIFF_OFDM4T_CCK4T_A)) 
				|| ((id >= MIB_HW_TX_POWER_DIFF_20BW1S_OFDM1T_B) && (id <= MIB_HW_TX_POWER_DIFF_OFDM4T_CCK4T_B)) )
				max_chan_num = MAX_2G_CHANNEL_NUM_MIB;

			if(((id >= MIB_HW_TX_POWER_DIFF_5G_20BW1S_OFDM1T_A) && (id <= MIB_HW_TX_POWER_DIFF_5G_80BW4S_160BW4S_A)) 
				|| ((id >= MIB_HW_TX_POWER_DIFF_5G_20BW1S_OFDM1T_B) && (id <= MIB_HW_TX_POWER_DIFF_5G_80BW4S_160BW4S_B)) )
				max_chan_num = MAX_5G_DIFF_NUM;
#endif

			if(tmp[0]==2){
				if(tmp[3] == 0xff){ // set one channel value
					memcpy((unsigned char *)(((long)pMibTbl) + pTbl[i].offset + (long)tmp[1] -1), (unsigned char *)(tmp+2), 1);
				}
			}else{
					memcpy((unsigned char *)(((long)pMibTbl) + pTbl[i].offset), (unsigned char *)(value+1), max_chan_num);
				}		
		}
		break;
	
	default:
		break;
	}

hwmib_set_errorout:
	return ret;
}

int hwmib_update(unsigned char *buf)
{
	int i, len;
#ifndef MIB_TLV
	int j, k;
#endif
	unsigned char checksum;
	unsigned char *data;
    PARAM_HEADER_T header;
#ifdef MIB_TLV
	unsigned char *pfile = NULL;
	unsigned char *mib_tlv_data = NULL;
	unsigned int tlv_content_len = 0;
	unsigned int mib_tlv_max_len = 0;
#endif

	int write_hw_tlv = 0;
	//	always Write HW setting uncompressed		
	//		if(compress_hw_setting == 0)
	//		write_hw_tlv = 0;
	if(flash_read(&header,hw_setting_off,sizeof(PARAM_HEADER_T))==0){
		printf("Read wlan hw setting header failed\n");
		return -1;
	}
	len=header.len;

	if ( flash_write((char *)buf, hw_setting_off+sizeof(header), header.len)==0 ) {
		printf("write hs MIB failed!\n");
#if CONFIG_APMIB_SHARED_MEMORY == 1
		apmib_sem_unlock();
#endif
#ifdef MIB_TLV
	if(write_hw_tlv)	{

		if(mib_tlv_data)
			free(mib_tlv_data);
		
		if(pfile)
			free(pfile);
	}
#endif
		return 0;
	}

#ifdef MIB_TLV
	if(mib_tlv_data)
		free(mib_tlv_data);
	if(pfile)
		free(pfile);
#endif	

	return 1;
}


static void setMIB(unsigned char *mibBuf, char *name, int id, TYPE_T type, int len, int valNum, char **val, unsigned char wlan_idx)
{
	unsigned char key[200];
	void *value=NULL;
	int int_val, i;	
	int entryNum;
	int max_chan_num=0, tx_power_cnt=0;

	switch (type) {
		case BYTE_T:
			int_val = atoi(val[0]);
			value = (void *)&int_val;
			break;

		case BYTE6_T:
			if ( strlen(val[0])!=12 || !string_to_hex(val[0], key, 12)) {
				printf("invalid value!\n");
				return;
			}
			value = (void *)key;
			break;

		case BYTE_ARRAY_T:
			if(!(id >= MIB_HW_TX_POWER_CCK_A &&  id <=MIB_HW_TX_POWER_DIFF_OFDM) &&
				!(id >= MIB_HW_TX_POWER_5G_HT40_1S_A &&  id <=MIB_HW_TX_POWER_DIFF_5G_OFDM)
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(HAVE_RTK_AC_SUPPORT) || defined(HAVE_RTK_4T4R_AC_SUPPORT)
				&& !(id >= MIB_HW_TX_POWER_DIFF_20BW1S_OFDM1T_A &&  id <=MIB_HW_TX_POWER_DIFF_5G_80BW4S_160BW4S_B)
#endif
#if defined(CONFIG_WLAN_HAL_8814AE) || defined(HAVE_RTK_4T4R_AC_SUPPORT)
				&& !(id >= MIB_HW_TX_POWER_DIFF_20BW1S_OFDM1T_C &&  id <=MIB_HW_TX_POWER_5G_HT40_1S_D)
#endif
				){
				printf("invalid mib!\n");
				return;
			}

			if((id >= MIB_HW_TX_POWER_CCK_A &&  id <=MIB_HW_TX_POWER_DIFF_OFDM)
#if defined(CONFIG_WLAN_HAL_8814AE) || defined(HAVE_RTK_4T4R_AC_SUPPORT)
				||(id >= MIB_HW_TX_POWER_CCK_C &&  id <=MIB_HW_TX_POWER_CCK_D)
				||(id >= MIB_HW_TX_POWER_HT40_1S_C &&  id <=MIB_HW_TX_POWER_HT40_1S_D)
#endif
			)
				max_chan_num = MAX_2G_CHANNEL_NUM_MIB;
			else if((id >= MIB_HW_TX_POWER_5G_HT40_1S_A &&  id <=MIB_HW_TX_POWER_DIFF_5G_OFDM)
#if defined(CONFIG_WLAN_HAL_8814AE) || defined(HAVE_RTK_4T4R_AC_SUPPORT)
				||(id >= MIB_HW_TX_POWER_5G_HT40_1S_C &&  id <=MIB_HW_TX_POWER_5G_HT40_1S_D)
#endif
			)
				max_chan_num = MAX_5G_CHANNEL_NUM_MIB;
			
#if defined(CONFIG_RTL_8812_SUPPORT) || defined(HAVE_RTK_AC_SUPPORT) || defined(HAVE_RTK_4T4R_AC_SUPPORT)
			if(((id >= MIB_HW_TX_POWER_DIFF_20BW1S_OFDM1T_A) && (id <= MIB_HW_TX_POWER_DIFF_OFDM4T_CCK4T_A)) 
				|| ((id >= MIB_HW_TX_POWER_DIFF_20BW1S_OFDM1T_B) && (id <= MIB_HW_TX_POWER_DIFF_OFDM4T_CCK4T_B)) )
				max_chan_num = MAX_2G_CHANNEL_NUM_MIB;

			if(((id >= MIB_HW_TX_POWER_DIFF_5G_20BW1S_OFDM1T_A) && (id <= MIB_HW_TX_POWER_DIFF_5G_80BW4S_160BW4S_A)) 
				|| ((id >= MIB_HW_TX_POWER_DIFF_5G_20BW1S_OFDM1T_B) && (id <= MIB_HW_TX_POWER_DIFF_5G_80BW4S_160BW4S_B)) )
				max_chan_num = MAX_5G_DIFF_NUM;
#endif

#if defined(CONFIG_WLAN_HAL_8814AE) || defined(HAVE_RTK_4T4R_AC_SUPPORT)
			if(((id >= MIB_HW_TX_POWER_DIFF_20BW1S_OFDM1T_C) && (id <= MIB_HW_TX_POWER_DIFF_OFDM4T_CCK4T_C))
				|| ((id >= MIB_HW_TX_POWER_DIFF_20BW1S_OFDM1T_D) && (id <= MIB_HW_TX_POWER_DIFF_OFDM4T_CCK4T_D)) )
				max_chan_num = MAX_2G_CHANNEL_NUM_MIB;

			if(((id >= MIB_HW_TX_POWER_DIFF_5G_20BW1S_OFDM1T_C) && (id <= MIB_HW_TX_POWER_DIFF_5G_80BW4S_160BW4S_C))
				|| ((id >= MIB_HW_TX_POWER_DIFF_5G_20BW1S_OFDM1T_D) && (id <= MIB_HW_TX_POWER_DIFF_5G_80BW4S_160BW4S_D)) )
				max_chan_num = MAX_5G_DIFF_NUM;
#endif

			for (i=0; i<max_chan_num ; i++) {
				if(val[i] == NULL) break;
				if ( !sscanf(val[i], "%d", &int_val) ) {
					printf("invalid value!\n");
					return;
				}
				key[i+1] = (unsigned char)int_val;
				tx_power_cnt ++;
			}

			if(tx_power_cnt != 1 && tx_power_cnt !=2 && tx_power_cnt != max_chan_num){
				unsigned char key_tmp[200];
				memcpy(key_tmp, key+1, tx_power_cnt);		
				hwmib_get(id, key+1, mibBuf, wlan_idx);
				memcpy(key+1, key_tmp, tx_power_cnt);
			}

			if(tx_power_cnt == 1){
				for(i=1 ; i <= max_chan_num; i++) {
					key[i] = key[1];
				}
			} else if(tx_power_cnt ==2) {
				//key[1] is channel number to set
				//key[2] is tx power value
		                //key[3] is tx power key for check set mode
				if(key[1] < 1 || key[1] > max_chan_num){
					if((key[1]<1) || ((id >= MIB_HW_TX_POWER_CCK_A && id <=MIB_HW_TX_POWER_DIFF_OFDM)) ||
						 ((id >= MIB_HW_TX_POWER_5G_HT40_1S_A &&  id <=MIB_HW_TX_POWER_DIFF_5G_OFDM) && (key[1]>216))){
						printf("invalid channel number\n");
						return;
					}
				}
				key[3] = 0xff ;
			}
			key[0] = tx_power_cnt;
			value = (void *)key;
			break;

		case STRING_T:
			if ( strlen(val[0]) > len) {
				printf("string value too long!\n");
				return;
			}
			value = (void *)val[0];
			break;
		default:
			printf("invalid mib!\n");
			return;
	}

	if ( !hwmib_set(id, value, mibBuf, wlan_idx))
		printf("set MIB failed!\n");
	//printf("setMIB end...\n");
    hwmib_update(mibBuf);
}

static int searchMIB(char *token, unsigned int *wlan_idx)
{
	int idx = 0;
	char tmpBuf[100];
	int desired_config=0;

	if (!memcmp(token, "HW_", 3)) {
		config_area = HW_MIB_AREA;
		if (!memcmp(&token[3], "WLAN", 4) && token[8] == '_') {
			*wlan_idx = token[7] - '0';
			if (*wlan_idx >= NUM_WLAN_INTERFACE) {
				printf("WLAN%d is not supported\n",*wlan_idx);
				return -1;
			}
			strcpy(tmpBuf, &token[9]);
			desired_config = config_area+1;
		}
		else
			strcpy(tmpBuf, &token[3]);
	} else {
		printf("Invalid HW MIB name:%s\n",token);
		return -1;
	}

	while (hwmib_table[idx].id) {
		if ( !strcmp(hwmib_table[idx].name, tmpBuf)) {
			if (desired_config && config_area != desired_config)
				return -1;
			return idx;
		}
		idx++;
	}
	idx=0;
	while (hwmib_wlan_table[idx].id) {
		if ( !strcmp(hwmib_wlan_table[idx].name, tmpBuf)) {
			config_area++;
			if (desired_config && config_area != desired_config)
				return -1;
			return idx;
		}
		idx++;
	}
	return -1;
}

int main(int argc, char *argv[])
{
	PARAM_HEADER_T header;
	unsigned int mibIdx=0, valNum=0;
	char *buf = NULL, mib[100]={0}, valueArray[170][100], *value[170], *ptr;
	int hw_len = 0, action=0, argNum=1, wlan_idx=0;
	HW_WLAN_SETTING_Tp phw;

	read_hw_setting_offset();	//mark_hw

	argNum=1;
	if ( argc > 1 ) {
		if ( !strcmp(argv[argNum], "sethw") || !strcmp(argv[argNum], "set") ) {
			action = MIB_WRITE;
			if (++argNum < argc) {
				if (argc > 4 && !memcmp(argv[argNum], "wlan", 4)) {
					wlan_idx = atoi(&argv[argNum++][4]);
					if (wlan_idx >= NUM_WLAN_INTERFACE) {
						printf("invalid wlan interface index number!\n");
						return 0;
					}
				}
				sscanf(argv[argNum], "%s", mib);
				while (++argNum < argc) {
					sscanf(argv[argNum], "%s", valueArray[valNum]);
					value[valNum] = valueArray[valNum];
					valNum++;
				}
				value[valNum]= NULL;
			}
		} else if ( !strcmp(argv[argNum], "gethw") || !strcmp(argv[argNum], "get") ) {
			action = MIB_READ;
            if (++argNum < argc) {
				if (argc > 3 && !memcmp(argv[argNum], "wlan", 4)) {
					wlan_idx = atoi(&argv[argNum++][4]);
					if (wlan_idx >= NUM_WLAN_INTERFACE) {
						printf("invalid wlan interface index number!\n");
						return 0;
					}
				}
				sscanf(argv[argNum], "%s", mib);
				while (++argNum < argc) {
					sscanf(argv[argNum], "%s", valueArray[valNum]);
					value[valNum] = valueArray[valNum];
					valNum++;
				}
				value[valNum]= NULL;
			}
        } else if ( !strcmp(argv[argNum], "allhw") ) {
			action = MIB_SHOW;
		} else if ( !strcmp(argv[argNum], "resethw") || !strcmp(argv[argNum], "reset") ) {
			action = MIB_DEFAULT;
		}
	}

	if(action < MIB_DEFAULT) {
		hw_len = read_hw_setting_length();
#if 0
		if(hw_len < NUM_WLAN_INTERFACE*sizeof(HW_WLAN_SETTING_T)+HW_WLAN_SETTING_OFFSET){
			printf("Wlan HW setting length invalid!\n");
			return -1;
		}
#endif
		if(hw_len > 0) {
			buf = malloc(hw_len);
			if(!buf){
				printf("Can't allocate memory for loading wlan HW setting!\n");
				return -1;
			}

			if(load_hw_setting(buf) < 0) {
				free(buf);
				return -1;
			}
		        
			switch(action) {
		        case MIB_SHOW:
					dumpAllHW(buf);
		            break;
				case MIB_WRITE:
				{
					unsigned int mibIdx=0;
					mib_table_entry_T *pTbl=NULL;
				        
					mibIdx = searchMIB(mib, &wlan_idx);

					if ( mibIdx == -1 ) {
						//showHelp();
						//showAllMibName();
						printf("MIB not found!\n");
						goto error_return;
					}

					if ( valNum < 1) {
						//showHelp();
						printf("Invalid input value!\n");
						goto error_return;
					}

					if (config_area == HW_MIB_AREA)
						pTbl = hwmib_table;
					else if (config_area == HW_MIB_WLAN_AREA)
						pTbl = hwmib_wlan_table;
					else {
						printf("Unknown HW setting area!\n");
			            goto error_return;
					}
					setMIB(buf, mib, pTbl[mibIdx].id, pTbl[mibIdx].type, pTbl[mibIdx].size, valNum, value, wlan_idx);
		            break;
				}
		        case MIB_READ:
				{
					unsigned int mibIdx=0, num=1;
					mib_table_entry_T *pTbl=NULL;

					mibIdx = searchMIB(mib,&wlan_idx);
					if ( mibIdx == -1 ) {
						//showHelp();
						//showAllMibName();
						printf("MIB not found!\n");
						return -1;
					}

					if (config_area == HW_MIB_AREA)
						pTbl = hwmib_table;
					else if (config_area == HW_MIB_WLAN_AREA)
						pTbl = hwmib_wlan_table;
					else {
						printf("Unknown HW setting area!\n");
			            goto error_return;
					}

					getMIB(buf, mib, pTbl[mibIdx].id, pTbl[mibIdx].type, num, 1 ,value, wlan_idx);
		            break;
				}
				default:
		        	printf("Unknown MIB operation!\n");
		        	break;
			}

			free(buf);
		} else {
			printf("Unknown HW settings header!\n");
		}
	} else {
		reset_hw_setting();
	}

error_return:
	return 0;	
}

