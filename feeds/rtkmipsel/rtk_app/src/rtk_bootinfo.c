#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#define TAG_LEN                                 4
#define RTK_BOOTINFO_SIGN "hwbt"
#define RTK_MAX_VALID_BOOTCNT 16

#define BOOT_NORMAL_MODE 0
#define BOOT_DUALIMAGE_TOGGLE_MODE 1
#define BOOT_DEFAULT_MAXCNT 3
/* Config file header */

typedef struct bootinfo_header {
        unsigned char tag[TAG_LEN] ;  // Tag + version
        unsigned int len ;
} BOOTINFO_HEADER_T, *BOOTINFO_HEADER_Tp;

typedef union bootinfo_data{
                unsigned int    val;
        struct {
                        unsigned char   bootbank;
                        unsigned char   bootmaxcnt;
                        unsigned char   bootcnt;
                        unsigned char   bootmode;
        } field;
} BOOTINFO_DATA_T, *BOOTINFO_DATA_P;

typedef struct bootinfo {
        BOOTINFO_HEADER_T header;
        BOOTINFO_DATA_T data;
}BOOTINFO_T, *BOOTINFO_P;


#define FLASH_DEVICE_NAME		("/dev/mtdblock0")

#define BOOTBANK_TMP_FILE		("/tmp/bootbank")
#define BOOTMODE_TMP_FILE		("/tmp/bootmode")


#ifdef CONFIG_RTL_8198C
static unsigned int FLASH_BOOTINFO_OFFSET=0x2a000;
#else 
#ifdef CONFIG_RTL_8197F //97F , is possible to use 64k erase flash , hence don't reuse 2a000 as bootinfo offset , use 30000 as new one
static unsigned int FLASH_BOOTINFO_OFFSET=0x30000;
#else //9XD/8881A/96E
static unsigned int FLASH_BOOTINFO_OFFSET=0xc000;
#endif
#endif

static BOOTINFO_T bootinfo_ram;

static int flash_read(char *buf, int offset, int len)
{
        int fh;
        int ok=1;

        fh = open(FLASH_DEVICE_NAME, O_RDWR );

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

        fh = open(FLASH_DEVICE_NAME, O_RDWR);

        if ( fh == -1 )
        {
                printf("open file error\n");
                return 0;
        }

        lseek(fh, offset, SEEK_SET);

        if ( write(fh, buf, len) != len)
                ok = 0;

        close(fh);

        return ok;
}

static int tmp_bootinfo_file_write(BOOTINFO_P bootinfo_ram_p)
{
        int fh, ok=1;
	char buf[8];	

        fh = open(BOOTBANK_TMP_FILE, O_RDWR | O_CREAT);

        if ( fh == -1 )
        {
                printf("open file error\n");
                return 0;
        }	

	sprintf(buf, "%d",bootinfo_ram_p->data.field.bootbank);			
       write(fh, buf, strlen(buf)+1);
        close(fh);	
		
        fh = open(BOOTMODE_TMP_FILE, O_RDWR | O_CREAT);

        if ( fh == -1 )
        {
                printf("open file error\n");
                return 0;
        }

	sprintf(buf, "%d",bootinfo_ram_p->data.field.bootmode);	
       write(fh, buf, strlen(buf)+1);        

	close(fh);	
        return ok;
}


void rtk_read_bootinfo_from_flash(BOOTINFO_P bootinfo_ram_p)
{

	unsigned int bootinfo_offset = FLASH_BOOTINFO_OFFSET;

        memset((char *)bootinfo_ram_p,0,sizeof(BOOTINFO_T));

        //flash(spi...etc)  can be read directly
        //memcpy((char *)bootinfo_ram_p,(char *)bootinfo_addr,sizeof(BOOTINFO_T));
        flash_read(bootinfo_ram_p,bootinfo_offset,sizeof(BOOTINFO_T));

}

void rtk_write_bootinfo_to_flash(BOOTINFO_P bootinfo_ram_p)
{
	unsigned int bootinfo_offset = FLASH_BOOTINFO_OFFSET;
	
	flash_write(bootinfo_ram_p,bootinfo_offset,sizeof(BOOTINFO_T));
}

static int read_bootinfo_offset(void)
{
        FILE *offset_proc;   
		
        offset_proc = fopen ( "/proc/flash/bootoffset", "r" );
		
        if ( offset_proc != NULL )
        {
                 char buf[16];
                 unsigned int offset_setting_off=0;

                 fgets(buf, sizeof(buf), offset_proc);        /* eat line */
                 sscanf(buf, "%x",&offset_setting_off);
				 
                if(offset_setting_off == 0)
                        offset_setting_off = FLASH_BOOTINFO_OFFSET;

                 FLASH_BOOTINFO_OFFSET = offset_setting_off;
                 fclose(offset_proc);               
                 //printf("read_hw_setting_offset = %x \n",HW_SETTING_OFFSET);
        }
	else
		return 1; //not bootinfo mode
	
	 return 0;	
}

// bootinfo setbank 0/1 , 
// bootinfo setbootcnt 0/1 , 
int main(int argc, char *argv[])
{
   char *cmd =NULL;  
   unsigned char val=0;
   BOOTINFO_P bootinfo_ram_p=&bootinfo_ram;

  //check bootinfo proc exit and read bootoffset
  if(read_bootinfo_offset())
  	return 0;

  //decide command
  if(argc==3)
  {  	
       cmd = argv[1];  

	  if(!strcmp(cmd, "setbootcnt"))
	 {
			//read bootinfo from mtd0
		rtk_read_bootinfo_from_flash(bootinfo_ram_p);	
		
		 val = (unsigned char)(atoi(argv[2]));

		 bootinfo_ram_p->data.field.bootcnt = val ;

		 rtk_write_bootinfo_to_flash(bootinfo_ram_p);	
	 }
	 else if(!strcmp(cmd, "setbootbank"))  
	 {

		//read bootinfo from mtd0
		rtk_read_bootinfo_from_flash(bootinfo_ram_p);	
		
		 val = (unsigned char)(atoi(argv[2]));

		 bootinfo_ram_p->data.field.bootbank = val ;

		 rtk_write_bootinfo_to_flash(bootinfo_ram_p);	

	 }
	  else if(!strcmp(cmd, "setbootmode"))  
	 {

		//read bootinfo from mtd0
		rtk_read_bootinfo_from_flash(bootinfo_ram_p);	
		
		 val = (unsigned char)(atoi(argv[2]));

		 bootinfo_ram_p->data.field.bootmode = val ;

		 rtk_write_bootinfo_to_flash(bootinfo_ram_p);	

	 }
	 else if(!strcmp(cmd, "setbootmaxcnt"))  
	 {

		//read bootinfo from mtd0
		rtk_read_bootinfo_from_flash(bootinfo_ram_p);	
		
		 val = (unsigned char)(atoi(argv[2]));

		 bootinfo_ram_p->data.field.bootmaxcnt = val ;

		 rtk_write_bootinfo_to_flash(bootinfo_ram_p);	

	 }

  }  
  if(argc==2)
  {  	
  	cmd = argv[1]; 
	if(!strcmp(cmd, "update"))
	 {
			//read bootinfo from mtd0
		rtk_read_bootinfo_from_flash(bootinfo_ram_p);	
		tmp_bootinfo_file_write(bootinfo_ram_p);	

		//optional , reset bootcnt
		bootinfo_ram_p->data.field.bootcnt = 0 ;
   	       rtk_write_bootinfo_to_flash(bootinfo_ram_p);	
		
	 }
	  else if(!strcmp(cmd, "getbootmode"))
	 {
			//read bootinfo from mtd0
		rtk_read_bootinfo_from_flash(bootinfo_ram_p);	

		printf("%d", bootinfo_ram_p->data.field.bootmode);
	 }
	 else if(!strcmp(cmd, "getbootbank"))
	 {
			//read bootinfo from mtd0
		rtk_read_bootinfo_from_flash(bootinfo_ram_p);	

		printf("%d", bootinfo_ram_p->data.field.bootbank);
	 }
	 else if(!strcmp(cmd, "getbootmaxcnt"))
	 {
			//read bootinfo from mtd0
		rtk_read_bootinfo_from_flash(bootinfo_ram_p);	

		printf("%d", bootinfo_ram_p->data.field.bootmaxcnt);
	 }
	
    }

}

