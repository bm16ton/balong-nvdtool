#include <stdio.h>
#include <stdint.h>
#ifndef WIN32
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#else
#include <windows.h>
#include "getopt.h"
#include "printf.h"
#include "buildno.h"
#endif

#include "nvfile.h"
#include "nvio.h"
#include "nvid.h"
#include "nvcrc.h"

// File with open nvram image
FILE* nvf=0;

int mflag=-1;
int moff=0;
int mdata[128];
int midx;

int aflag=-1;
int aoff=0;
char adata[128];

// Offset to CRC field in file
uint32_t crcoff;

#ifdef MODEM
// flag for direct work with the nvram file instead of the kernel interface
int32_t kernelflag=0;
int ecall(char* name);
#endif

int test_crc();
void recalc_crc();

//****************************************************************
//* Checking the validity of working with CRC
//****************************************************************
void check_crcmode() {

if (crcmode == -1) {
  printf("\n Unsupported CRC type - nvram is read-only\n");
  exit(0);
}
}

//****************************************************************
//*   Parsing parameters of the -m switch
//****************************************************************
void parse_mflag(char* arg) {

char buf[1024];
char* sptr, *offptr;
int endflag=0;

if (strlen(arg)>1024) {
    printf("\nThe -m switch argument is too long\n");
    exit(0);
}
strcpy(buf,arg);

// check for the presence of the byte +
sptr=strchr(buf,'+');

if (sptr != 0) {
  // there is a displacement
  offptr=sptr+1;
  *sptr=0;
  sscanf(buf,"%i",&mflag);
  // looking for the first colon
  sptr=strchr(sptr+1,':');
  if (sptr == 0) {
    printf("\n - the -m switch does not specify a single replacement byte\n");
    exit(1);
  }
  *sptr=0;
  sscanf(offptr,"%x",&moff);
}

else {
  // no offset
  // looking for the first colon
  sptr=strchr(buf,':');
  if (sptr == 0) {
    printf("\n - the -m switch does not specify a single replacement byte\n");
    exit(1);
  }
  *sptr=0;
  sscanf(buf,"%i",&mflag);
}
sptr++; // to the first byte;
do {
  offptr=sptr;
  sptr=strchr(sptr,':'); // looking for another colon
  if (sptr != 0) *sptr=0;
  else endflag=1;
  sscanf(offptr,"%x",&mdata[midx]);
  if (mdata[midx] > 0xff) {
    printf("\n Incorrect byte 0x%x in the -m switch",mdata[midx]);
    exit(1);
  }
  midx++;
  sptr++;
} while (!endflag);
}


//****************************************************************
//*   Parsing the parameters of the -a switch
//****************************************************************
void parse_aflag(char* arg) {

char buf[1024];
char* sptr, *offptr;

if (strlen(arg)>1024) {
    printf("\nThe -a switch argument is too long\n");
    exit(0);
}
strcpy(buf,arg);

// check for the presence of the byte +
sptr=strchr(buf,'+');

if (sptr != 0) {
  // there is a displacement
  offptr=sptr+1;
  *sptr=0;
  sscanf(buf,"%i",&aflag);
  // looking for the first colon
  sptr=strchr(sptr+1,':');
  if (sptr == 0) {
    printf("\n - the -a switch does not specify a single replacement byte\n");
    exit(1);
  }
  *sptr=0;
  sscanf(offptr,"%x",&aoff);
}

else {
  // no offset
  // looking for the first colon
  sptr=strchr(buf,':');
  if (sptr == 0) {
    printf("\n - the -a switch does not specify a single replacement byte\n");
    exit(1);
  }
  *sptr=0;
  sscanf(buf,"%i",&aflag);
}
sptr++; //to the first byte;
strncpy(adata,sptr,128);
}

//****************************************************************
//*  Print utility title
//****************************************************************
void utilheader() {

printf("\n Utility for editing NVRAM images of chipset devices\n"
       " Hisilicon Balong, V1.0.%i (c) forth32, 2016, GNU GPLv3",BUILDNO);
#ifdef WIN32
printf("\n Port for Windows 32bit (c) rust3028, 2017");
#endif
printf("\n-------------------------------------------------------------------\n");
}


//****************************************************************
//* Печать help-блока
//****************************************************************
void utilhelp(char* utilname) {

utilheader();
printf("\nCommand line format:\n\n\
%s [switches] <NVRAM image file name>\n\n\
  The following keys are valid:\n\n\
-l - display NVRAM image map\n\
-u - print unique identifiers and settings\n\
-e - extract all cells \n\
-x item - extract cell item to a file \n\
-d item - dump the item cell (d* - all cells)\n\
-r item:file - replace the item cell with the contents of the file file\n\n\
-m item[+off]:nn[:nn...] - replace the bytes in item with the bytes specified in the command\n\
-a item[+off]:text - write a text string to item\n\
         * if +off is not specified, the replacement starts from a zero byte. The offset is specified in hex\n\n\
-i imei write new IMEI\n\
-s serial- write a new serial number\n\
-c - extract all component files \n\
-k n - extract all cells related to component n into the COMPn directory \n\
-w dir - import cell contents from directory files dir/\n\
-b oem|simlock|all - select OEM, SIMLOCK or both codes\n"
#ifdef MODEM
"-f - reload the modified nvram into the modem memory\n"
#endif
"\n",utilname);
}


//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

int main(int argc,char* argv[]) {

int i;
int opt;
int res;
uint32_t pos;
char buf[10000];
int len;
char rfilename[200];
char* sptr;
FILE* in;
char imei[16];
char serial[20];
char nvfilename[100];

int xflag=-1;
int lflag=0;
int cflag=0;
int eflag=0;
int dflag=-1;
int rflag=-1;
int bflag=0;
int uflag=0;
int iflag=0;
int sflag=0;
int kflag=-1;
char wflag[200]={0};

while ((opt = getopt(argc, argv, "hlucex:d:r:m:b:i:s:a:k:w:f")) != -1) {
  switch (opt) {
   case 'h':
    utilhelp(argv[0]);
    return 0;

   case 'f':
#ifndef MODEM
     printf("\n The -f switch is not valid on this platform\n");
     return 0;
#else
     kernelflag=1;
     break;
#endif

   case 'b':
     switch (optarg[0]) {
       case 'o':
       case 'O':
	  bflag=1;
	  break;

       case 's':
       case 'S':
	  bflag=2;
	  break;

       case 'a':
       case 'A':
	  bflag=3;
	  break;

       default:
	  printf("\n incorrect value for -b switch\n");
          return 0;
     }
     break;

   case 'l':
     lflag=1;
     break;

   case 'u':
     uflag=1;
     break;

   case 'e':
     eflag=1;
     break;

   case 'c':
     cflag=1;
     break;

   case 'w':
     strcpy(wflag,optarg);
     break;

   case 'i':
     iflag=1;
     memset(imei,0,16);
     strncpy(imei,optarg,15);
     break;

   case 's':
     sflag=1;
     memset(serial,0xFF,20);
     strncpy(serial,optarg,strlen(optarg) < 16 ? strlen(optarg) : 16);
     break;

   case 'x':
     sscanf(optarg,"%i",&xflag);
     break;

   case 'k':
     sscanf(optarg,"%i",&kflag);
     break;

   case 'd':
     if (*optarg == '*') dflag=-2;
     else sscanf(optarg,"%i",&dflag);
     break;

   case 'r':
     strcpy(buf,optarg);
     sptr=strchr(buf,':');
     if (sptr == 0) {
       printf("\n - The file name is not specified in the -r switch\n");
       return 0;
     }
     *sptr=0;
     sscanf(buf,"%i",&rflag);
     strcpy(rfilename,sptr+1);
     break;

   case 'm':
     parse_mflag(optarg);
     break;

   case 'a':
     parse_aflag(optarg);
     break;

   case '?':
   case ':':
     return 0;
  }
}


if (optind>=argc) {
#ifndef MODEM
    printf("\n - Not an nvram image file, use the -h switch for a hint\n");
    return 0;
#else
    strcpy(nvfilename,"/mnvm2:0/nv.bin");
#endif
}
else {
    strcpy(nvfilename,argv[optind]);
#ifdef MODEM
    // when working inside a modem, we automatically set the direct access flag when specifying an input file
    if (kernelflag==1) {
        printf("\n The -f switch is not applicable to explicitly specifying the file name");
        kernelflag=0;
    }
#endif
}


//------------ Разбор nv-файла и выделение из него управлющих структур -----------------------

// открываем образ nvram
nvf=fopen(nvfilename,"r+b");
if (nvf == 0) {
  printf("\n File %s not found\n",argv[optind]);
  return 0;
}

// читаем заголовок образа
res=fread(&nvhd,1,sizeof(nvhd),nvf);
if (res != sizeof(nvhd)) {
  printf("\n - Error reading file %s\n",argv[optind]);
  return 0;
}
if (nvhd.magicnum != FILE_MAGIC_NUM) {
  printf("\n - File %s is not an NVRAM image\n",argv[optind]);
  return 0;
}

// Определяем тип CRC
switch (nvhd.crcflag) {
  case 0:
    crcmode=0;
    break;

  case 1:
    crcmode=1;
    break;

  case 3:
    crcmode=3;
    break;

  case 8:
    crcmode=2;
    break;

  default:
    crcmode=-1;
    break;

}

//----- Reading the file directory

fseek(nvf,nvhd.file_offset,SEEK_SET);
pos=nvhd.ctrl_size;

for(i=0;i<nvhd.file_num;i++) {
 // extract the information available in the file
 fread(&flist[i],1,36,nvf);
 // calculate the offset to the file data
 flist[i].offset=pos;
 pos+=flist[i].size;
}

// we get the offset to the CRC field
crcoff=pos;

//----- Reading the cell directory
fseek(nvf,nvhd.item_offset,SEEK_SET);
itemlist=malloc(nvhd.item_size);
fread(itemlist,1,nvhd.item_size,nvf);

// output of maps and parameters
if (lflag) {
  utilheader();
  print_hd_info();
  print_filelist();
  print_itemlist();
  return 0;
}

// Displaying unique parameters
if (uflag) {
  print_data();
  return 0;
}

// extracting files and cells
if (cflag) extract_files();
if (eflag) extract_all_item();
if (kflag != -1) extract_comp_items(kflag);
if (xflag != -1) {
  printf("\n Cell is retrieved %i\n",xflag);
  item_to_file(xflag,"");
}

// View cell dump
if (dflag != -1) {
  if (dflag != -2) dump_item(dflag); // one cell
  else
  // all cells
     for(i=0;i<nvhd.item_count;i++)  dump_item(itemlist[i].id);
}

// Bulk import of cells (switch -w)
if (strlen(wflag) != 0) {
check_crcmode();
mass_import(wflag);
}

// Replacing cells
if (rflag != -1) {
  check_crcmode();
  len=itemlen(rflag);
  if (len == -1) {
    printf("\n - Cell %i not found\n",rflag);
    return 0;
  }
  in=fopen(rfilename,"rb");
  if (in == 0) {
    printf("\n - Error opening file %s\n",rfilename);
    return 0;
  }
  fseek(in,0,SEEK_END);
  if (ftell(in) != len) {
    printf("\n File size (%u) does not match cell size (%i)\n",(uint32_t)ftell(in),len);
    return 0;
  }
  fseek(in,0,SEEK_SET);
  fread(buf,1,len,in);
  save_item(rflag,buf);
  printf("\nCell %i successfully written\n",rflag);
}

// direct cell editing
if (mflag != -1) {
  check_crcmode();
  len=load_item(mflag,buf);
  if (len == -1) {
    printf("\n - Cell %i not found\n",mflag);
    return 0;
  }
  if ((midx+moff) > len) {
    printf("\n - cell length exceeded %i\n",mflag);
    return 0;
  }

  for(i=0;i<midx;i++) buf[moff+i]=mdata[i];
  save_item(mflag,buf);
  printf("\n cell %i edited\n",mflag);

}

// Writing text strings to a cell
if (aflag != -1) {
 check_crcmode();
 len=load_item(aflag,buf);
  if (len == -1) {
    printf("\n Cell %i not found\n",mflag);
    return 0;
  }
  if ((strlen(adata)+aoff) > len) {
    printf("\n - cell length exceeded %i\n",mflag);
    return 0;
  }
  memcpy(buf+aoff,adata,strlen(adata));
  save_item(aflag,buf);
  printf("\n Cell %i edited\n",aflag);
}

// selection of blocking codes
if (bflag) {
  if (nvhd.version>121) {
    printf("\nCode calculation is not supported on this platform");
  }
  else {
   utilheader();
   switch (bflag) {
    case 1:
      brute(1);
      return 0;

    case 2:
      brute(2);
      return 0;

    case 3:
      brute(1);
      brute(2);
      return 0;
  }
 }
}

// IMEI entry
if (iflag) {
  check_crcmode();
  write_imei(imei);
}

// serial record
if (sflag) {
  check_crcmode();
  write_serial(serial);
}

// recalculation of the KS array
if (crcmode != -1) recalc_crc();
fclose(nvf);

#ifdef MODEM
  if (kernelflag) system("ecall bsp_nvm_reload");
#endif
printf("\n");

}
