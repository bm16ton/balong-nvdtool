#ifndef _NVIO_C
#define _NVIO_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifndef WIN32
#include <sys/stat.h>
#include <sys/types.h>
#else
#include <windows.h>
#include <direct.h>
#include "printf.h"
#endif

#include "nvfile.h"
#include "nvio.h"
#include "nvio.h"
#include "nvid.h"
#include "nvcrc.h"
#include "sha2.h"

// Maximum permissible cell size
#define max_item_len 10000

//File header storage
struct nvfile_header nvhd;
// File Catalog
struct nv_file flist[100];
// cell directory
struct nv_item* itemlist;

#ifdef MODEM
// flag for direct work with the nvram file instead of the kernel interface
extern int32_t kernelflag;
#endif

//******************************************************
// Getting the offset to the beginning of a file by file number
//******************************************************
uint32_t fileoff(int fid) {

int i;
for (i=0;i<nvhd.file_num;i++) {
  if (flist[i].id == fid) return flist[i].offset;
}
printf("\n - Ошибка структуры файла - компоненты #%i не существует\n",fid);
exit(1);
}

//******************************************************
// Getting index by file number
//******************************************************
int32_t fileidx(int fid) {

int i;

for (i=0;i<nvhd.file_num;i++) {
  if (flist[i].id == fid) return i;
}
return -1;
}


//******************************************************
// Getting the offset to the beginning of a cell by its index
//******************************************************
uint32_t itemoff_idx(int idx) {

return itemlist[idx].off+fileoff(itemlist[idx].file_id);
}


//******************************************************
//* Getting the cell index by its id
//*  return -1 - cell not found
//******************************************************
int32_t itemidx(int item) {

int i;

for(i=0;i<nvhd.item_count;i++) {
  if (itemlist[i].id == item) return i;
}
return -1;
}

//******************************************************
// Getting the offset to the beginning of a cell by its number
//******************************************************
int32_t itemoff (int item) {

int idx=itemidx(item);
if (item == -1) return -1;
return itemoff_idx(idx);
}

//******************************************************
// Getting the cell size by its number
//******************************************************
int32_t itemlen (int item) {

int idx=itemidx(item);
if (idx == -1) return -1;
return itemlist[idx].len;
}


//***********************
//* Dump memory area *
//***********************

void fdump(char buffer[],unsigned int len,unsigned int base, FILE* dump) {
unsigned int i,j;
char ch;

for (i=0;i<len;i+=16) {
  fprintf(dump,"%08x: ",(base+i));
  for (j=0;j<16;j++){
   if ((i+j) < len) fprintf(dump,"%02x ",buffer[i+j]&0xff);
   else fprintf(dump,"   ");
  }
  fprintf(dump," *");
  for (j=0;j<16;j++) {
   if ((i+j) < len) {
    ch=buffer[i+j];
    if ((ch < 0x20)|(ch > 0x80)) ch='.';
    fputc(ch,dump);
   }
   else fputc(' ',dump);
  }
  fprintf(dump,"*\n");
}
}

//**********************************************
//* Outputting header information
//**********************************************
void print_hd_info() {

printf("\n NVRAM file version:       %i (0x%x)",nvhd.version,nvhd.version);
printf("\n Modem number:             %i",nvhd.modem_num);
printf("\n Device ID:				 %s",nvhd.product_version);
printf("\n CRC type:                  ");
switch (crcmode) {
  case 0:
    printf("No");
    break;

  case 1:
    printf("Block type 1");
    break;

  case 3:
    printf("Block type 3");
    break;

  case 2:
    printf("Individual, type 8");
    break;

  default:
    printf("Unsupported in this version of the utility, typ %x",nvhd.crcflag);
    break;
}

}


//**********************************************
//* Вывод списка компонентных файлов
//**********************************************
void print_filelist() {

int i;

printf("\n\n --- File Catalog ----\n");
printf("\n ID  position size ---name---\n--------------------------------------");

for(i=0;i<nvhd.file_num;i++) {
  printf("\n %2i  %08x  %6i  %s",flist[i].id,flist[i].offset,flist[i].size,flist[i].name);
}

printf("\n * Total files: %i\n",nvhd.file_num);
}

//**********************************************
//* Вывод списка ячеек nvram
//**********************************************
void print_itemlist() {

int i;
uint32_t badflag=0;
printf("\n\n --- Cell catalog ----\n");
printf("\n NVID   FID  position size priority name\n-----------------------------------------------");

for (i=0;i<nvhd.item_count;i++) {
   printf("\n %-5i  %2i   %08x  %4i    %x   %s",
 	itemlist[i].id,
 	itemlist[i].file_id,
 	itemoff_idx(i),
 	itemlist[i].len,
 	itemlist[i].priority,
 	find_desc(itemlist[i].id));


  if ((crcmode ==2) && !verify_item_crc(itemlist[i].id)) {
    printf("\n ! ** Cell CRC error.\n");
    badflag++;
  }
}
printf("\n\n * Total cells: %i\n",nvhd.item_count);
if ((crcmode == 2) && (badflag != 0)) printf(" ! Cells with error CRC: %i\n",badflag);

}

//**********************************************
//* Extracting component files
//**********************************************
void extract_files() {

int i;
char filename[100];
char* buf;
FILE* out;

#ifndef WIN32
mkdir ("component",0777);
#else
_mkdir("component");
#endif
for(i=0;i<nvhd.file_num;i++) {
 fseek(nvf,flist[i].offset,SEEK_SET);
 printf("\n Extracting a file %s",flist[i].name);
 sprintf(filename,"component/%s",flist[i].name);
 out=fopen(filename,"wb");
 buf=malloc(flist[i].size);
 fread(buf,1,flist[i].size,nvf);
 fwrite(buf,1,flist[i].size,out);
 free(buf);
 fclose(out);
}
printf("\n");
}

//**********************************************
//* Retrieving cells by component number
//**********************************************
void extract_comp_items(int32_t fn) {


char dirname[100];
int i,fidx;

fidx=fileidx(fn);
if (fidx == -1) {
  printf("\n Components %i не exists\n",fn);
  return;
}

printf("\n Extracting Component Cells %i (%s)\n\n",fn,flist[fidx].name);
sprintf(dirname,"COMP%i/",fn);
#ifndef WIN32
mkdir(dirname,0777);
#else
_mkdir(dirname);
#endif

for(i=0;i<nvhd.item_count;i++) {
  if (itemlist[i].file_id != fn) continue;
  printf("\r Cell %i",itemlist[i].id);
  item_to_file(itemlist[i].id,dirname);
}
printf("\n\n");

}

//**********************************************
//* Дамп ячейки nvram
//**********************************************
void dump_item(uint32_t item) {

int len;
char buf[max_item_len];
char* desc;
int32_t idx,fidx;
int32_t fn;

idx=itemidx(item);
if (idx == -1) {
    printf("\n - Cell %i not found\n",item);
    return;
}

fseek(nvf,itemoff_idx(idx),SEEK_SET);
fread(buf,itemlist[idx].len,1,nvf);
len=itemlist[idx].len;
fn=itemlist[idx].file_id;
fidx=fileidx(fn);

printf("\n -- cell # %i: %i byte -- Component  %i (%s) -- ",item,len,fn,flist[fidx].name);
desc=find_desc(item);
if (strlen(desc) != 0) printf(" Name: %s --",desc);
printf("-----\n");
fdump(buf,len,0,stdout);
if ((crcmode == 2) && !verify_item_crc(item)) printf("\n ! Cell CRC error");
printf("\n");
}


//**********************************************
//* Loading a cell into the buffer
//**********************************************
int load_item(int item, char* buf) {

int idx;

idx=itemidx(item);
if (idx == -1) return -1; // not found
fseek(nvf,itemoff_idx(idx),SEEK_SET);
fread(buf,itemlist[idx].len,1,nvf);
return itemlist[idx].len;
}



//**********************************************
//* Writing a cell from a buffer
//**********************************************
int save_item(int item, char* buf) {

int idx;
int res;

idx=itemidx(item);
if (idx == -1) return 0; // not found

fseek(nvf,itemoff_idx(idx),SEEK_SET);
res=fwrite(buf,itemlist[idx].len,1,nvf);
if (res != 1) {
    printf("\nWriting a cell from a buffer %i",item);
    perror("");
    return 0;
}
restore_item_crc(item); // restoring CRC cells
return 1; // ok
}


//**********************************************
//* Extracting a single cell to a file
//**********************************************
void item_to_file(int item, char* prefix) {

char filename[100];
char buf[max_item_len];
FILE* out;
int len;

sprintf(filename,"%s%05i.nvm",prefix,item);
len=load_item(item,buf);
if (len == -1) {
  printf(" -  not found\n");
  exit(1);
}
out=fopen(filename,"wb");
fwrite(buf,1,len,out);
fclose(out);
if ((crcmode == 2) && !verify_item_crc(item)) printf(" - error CRC\n");
}

//**********************************************
//* Извлечение всех ячеек в файл
//**********************************************
void extract_all_item() {

int i;

#ifndef WIN32
mkdir ("item",0777);
#else
_mkdir("item");
#endif
printf("\n");
for(i=0;i<nvhd.item_count;i++) {
  printf("\r Cell %i length %i",itemlist[i].id,itemlist[i].len);
  item_to_file(itemlist[i].id,"item/");
}
printf("\n\n");
}

//**********************************************
//* Import all cells from a specified directory
//**********************************************
void mass_import(char* dir) {

int i;
char filename[200];
uint32_t fsize;
FILE* in;
char ibuf[max_item_len];
int idx;

printf("\n Importing cells:\n\n");
for (i=0;i<65536;i++) {
  sprintf(filename,"%s/%05i.nvm",dir,i);
  in=fopen(filename,"rb");
  if (in == 0) continue; // We don't have such a file
  // load the file into the buffer and at the same time determine its size
  fsize=fread(ibuf,1,10000,in);
  fclose(in);
  // checking the cell parameters in the image
  idx=itemidx(i);
  if (idx == -1) {
    printf("\r Cell %i not found in nvram image\n",i);
    continue;
  }
  if (itemlist[idx].len != fsize) {
    printf("\r Cell %i: cell size (%i) does not match the file size (%i)\n",i,itemlist[idx].len,fsize);
    continue;
  }
  // импорт ячейки
  printf("\r Cell %i: ",i);
  fseek(nvf,itemoff_idx(idx),SEEK_SET);
  fwrite(ibuf,itemlist[idx].len,1,nvf);
  restore_item_crc(i);
  printf("OK");
}
}


//**********************************************
//* Поск оем и симлок - кодов
//*  1 - oem
//*  2 - simlock
//**********************************************
void brute(int flag) {

char buf[128];
int item=50502;
int code;
SHA2_CTX ctx;
char scode[50];

uint8_t hash1[32],hash2[32];

if (flag == 2) item=50503;
if (itemlen(item) != 128) {
  printf("\n - Cell %i not found or damaged\n",item);
  return;
}
load_item(item,buf);
memcpy(hash1,buf,32);
memcpy(hash2,buf+64,32);

printf("\n");
for(code=0;code<99999999;code++) {
  if  (code % 1000000 == 0) {
    printf("\r* Search %s-code: %i%%... ",(flag == 1)?"OEM":"Simlock",code/1000000);
    fflush(stdout);
  }
  // хеш нашего кода
  SHA256Init(&ctx);
  sprintf(scode,"%08i",code);
  SHA256Update(&ctx,scode,8);
  SHA256Final(scode,&ctx);
  // хеш суммы
  SHA256Init(&ctx);
  SHA256Update(&ctx,scode,32);
  SHA256Update(&ctx,hash2,32);
  SHA256Final(scode,&ctx);
  // сравниваем хеш с лежащим в nvram
  if (memcmp(scode,hash1,32) == 0) {
    printf("\r Found %s-the code: %08i\n",(flag == 1)?"OEM":"Simlock",code);
    return;
  }
}
printf("\n%s-code not found\n",(flag == 1)?"OEM":"Simlock");
}

//************************************************
//* Запись нового IMEI
//************************************************
void write_imei(char* imei) {
unsigned char binimei[16];
int i,f,j;
char cbuf[7];
int csum;

if (strlen(imei) != 15) {
  printf("\n Incorrect IMEI length");
  return;
}

for (i=0;i<15;i++) {
  if ((imei[i] >= '0') && (imei[i] <='9')) {
    binimei[i] = imei[i]-'0';
    continue;
  }
  printf("\n Incorrect character in IMEI string - %c\n",imei[i]);
  return;
}
binimei[15]=0;
// Проверяем контрольную сумму IMEI
j=0;
for (i=1;i<14;i+=2) cbuf[j++]=binimei[i]*2;
csum=0;
for (i=0;i<7;i++) {
  f=(int)cbuf[i]/10;
  csum = csum + f + (int)((cbuf[i]*10) - f*100)/10;
}
for (i=0;i<13;i+=2) csum += binimei[i];

if ((((int)csum/10)*10) == csum) csum=0;
else csum=( (int)csum/10 + 1)*10 - csum;
if (binimei[14] != csum) {
  printf("\n IMEI has an incorrect checksum !\n Correct IMEI: ");
  for (i=0;i<14;i++) printf("%1i",binimei[i]);
  printf("%1i",csum);
  binimei[14]=csum;
  printf("\n IMEI corrected");
}

if (save_item(0,binimei)) printf("\n IMEI successfully written\n");
else printf("\n Cell search error 0");

}

//************************************************
//* Запись нового серийника
//************************************************
void  write_serial(char* serial) {

//char* sptr;

// заменяем нули на 0xff
//sptr=strchr(serial,0);
//if (sptr != 0) *sptr=0xff;

if (save_item(6,serial)) printf("\n Serial number successfully recorded\n");
else printf("\n Cell search error 6");

}


//************************************************
//*  Печать идентификаторов и настроек
//************************************************
void print_data() {

char buf[2560];
char* bptr;
int i,badflag=0;

// CRC управяющей структуры
uint32_t crc_ctrl=0;

// NV 53525 - информации о продукте
struct {
     uint32_t index;          // +00 Hardware version number (large version number 1 + large version number 2), distinguish between different products * /
     uint32_t hwIdSub;        // +04 hardware sub-version number, distinguish between different versions of the product * /
     uint8_t name [32];       // +08 internal product name * /
     uint8_t namePlus [32];   // +28 Internal product name PLUS * /
     uint8_t hwVer [32];      // +48 Hardware version name * /
     uint8_t dloadId [32];    // +68 the name used in the upgrade * /
     uint8_t productId [32];  // +88 External Product Name * /
 } prodinfo;

// информация о wifi
int wicount=0;       // число wifi-записей
// char wissid[36][4];  // имена сетей
// char wikey[68][4];   // ключи
char wissid[5][36];  // имена сетей
char wikey[5][68];   // ключи

// вывод информации о продукте
i=load_item(53525,(char*)&prodinfo);
if ((i == sizeof(prodinfo))&&(prodinfo.index != 0)) {
  printf("\n Product Code: %i %i",prodinfo.index,prodinfo.hwIdSub);
  printf("\n Product Name: %s",prodinfo.name);
  printf("\n HW version  : %s",prodinfo.hwVer);
  printf("\n Dload ID    : %s",prodinfo.dloadId);
  printf("\n Product ID  : %s\n",prodinfo.productId);
}

// вывод IMEI
load_item(0,buf);
printf("\n IMEI        : ");
for (i=0;i<15;i++) printf("%c",buf[i]+'0');
// вывод серийника
load_item(6,buf);
printf("\n Serial      : %16.16s",buf);
// IP-адрес:
load_item(44,buf);
printf("\n IPaddr      : %s",buf);

// mac
load_item(50014,buf);
if (buf[0] != 0) printf("\n Ethernet MAC: %s",buf);

// вывод wifi-параметров
#ifndef WIN32
bzero(buf,256);
bzero(wissid,36*4);
bzero(wikey,68*4);
#else
memset(buf,0,256);
memset(wissid,0,36*4);
memset(wikey,0,68*4);
#endif
load_item(9111,buf);
if (buf[0] != 0) {
  // ssid
  bptr=buf;
  wicount=0;
  while (*bptr != 0) {
    strcpy(wissid[wicount],bptr);
    wicount++;
    bptr+=0x21;
  }

  // WPA key
   load_item(9110,buf);
   for (i=0;i<wicount;i++) {
      strncpy(wikey[i],buf+462+i*65,65);
   }

  printf("\n\n *** Options Wifi ***\n\n #  SSID                             KEY\n\
----------------------------------------------------------------------------------\n");
  for(i=0;i<wicount;i++) printf("\n %1i  %-32.32s %-32.32s",i,(char*)&wissid[i],(char*)&wikey[i]);

}

// Checking crc
printf("\n");
// Selecting the header CRC
if (crcmode == 2) {
  if ((nvhd.ctrl_size-nvhd.item_offset-nvhd.item_size) != 4)
                       printf("\n CTRL CRC   : отсутствует поле CRC в заголовке - формат файла неверен");
  else {
   // missing CRC field in header - file format is incorrect
   fseek(nvf,nvhd.ctrl_size-4,SEEK_SET);
   fread(&crc_ctrl,4,1,nvf);
   if (crc_ctrl == calc_ctrl_crc())
     printf("\n CTRL CRC    : OK");
   else
     printf("\n CTRL CRC    : Error!");
  }
}
else printf("\n CTRL CRC    : Absent");

// Проверяем CRC области данных
switch (crcmode) {
  case 0:
    printf("\n DATA CRC    : Absent");
    break;

  case 1:
  case 3:
    if (test_crc() == 0) printf("\n DATA CRC    : OK");
    else printf("\n DATA CRC    : Error!");
    break;

  case 2:
    for (i=0;i<nvhd.item_count;i++) {
     if ((crcmode ==2) && !verify_item_crc(itemlist[i].id))  badflag++;
    }
    if (badflag != 0) printf("\n DATA CRC    : Cells with error CRC: %i",badflag);
    else printf("\n DATA CRC    : OK");
    break;

  default:
    printf("\n DATA CRC    : Unsupported type");

}

printf("\n");
}



#endif // define _NVIO_C
