#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>

typedef int (__cdecl *SAN_DEC)(uint8_t *in, uint8_t *out, int mode, int bit_size);
typedef int (__cdecl *SAN_DEC_PULCOD2_INIT)(int a, int b);

#define wav_add_long(a) out[i++] = a & 0xff;\
out[i++] = (a >> 8) & 0xff;\
out[i++] = (a >> 16) & 0xff;\
out[i++] = (a >> 24) & 0xff;\
	
int wave_header(uint8_t out[], int data_size, int freq)
{
	int file_size = data_size + 44 - 8;
	int i = 0;
	
	out[i++] = 'R';
	out[i++] = 'I';
	out[i++] = 'F';
	out[i++] = 'F';
	
	wav_add_long(file_size);
	
	out[i++] = 'W';
	out[i++] = 'A';
	out[i++] = 'V';
	out[i++] = 'E';
	
	out[i++] = 'f';
	out[i++] = 'm';
	out[i++] = 't';
	out[i++] = ' ';
	
	out[i++] = 16;
	out[i++] = 0;
	out[i++] = 0;
	out[i++] = 0;
	
	out[i++] = 1; // uncompressed
	out[i++] = 0;
	
	out[i++] = 1; // mono
	out[i++] = 0;

	wav_add_long(freq); // sample rate
	
	int byte_rate = freq*16*1/8;
	wav_add_long(byte_rate); // bps
	
	out[i++] = 2; // block aligment
	out[i++] = 0;
	
	out[i++] = 16; // bits per sample
	out[i++] = 0;

	out[i++] = 'd';
	out[i++] = 'a';
	out[i++] = 't';
	out[i++] = 'a';
	
	wav_add_long(data_size);

	return i;
}

int main(int argc, const char *argv[])
{
	
   if(argc < 2)
   {
      printf("sandec 1.1 \n\
Copyright (C) 2008 Robert Mazur (rm@nateo.pl)\n\
Converts Olympus Digital Voice Recorder files to WAV files.\n\
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n\
Ussage: sandec [filename].raw\n");
      return -1;
   }
	
   HINSTANCE hLibrary = LoadLibrary("san_dec.dll");
   if (hLibrary == NULL) 
   {
      printf("Unable to load san_dec.dll!\n");
      return -1;
   }

   SAN_DEC san_dec;
   san_dec  = (SAN_DEC)GetProcAddress(hLibrary, "san_dec");
   
   SAN_DEC_PULCOD2_INIT san_dec_pulcod2_init;
   san_dec_pulcod2_init  = (SAN_DEC_PULCOD2_INIT)GetProcAddress(hLibrary, "san_dec_pulcod2_init");
  
   if(san_dec == NULL)
   {
      printf("Unable to find 'san_dec' proc!\n");
      return -1;
   }
   
   if(san_dec_pulcod2_init == NULL)
   {
      printf("Unable to find 'san_dec_pulcod2_init' proc!\n");
      return -1;
   }
   
   int ret, i;
      
   int fd, fd2; 
   uint8_t in[8*1024], out[32*1024];
   
   int len = 0, out_len = 0, data_size=0, quality = 0, freq=16000, pulcod_size=0, max_size=0;
   int bit_size = 16;
   int mode = 1;   
   
   fd = open(argv[1],O_RDONLY|O_BINARY);
   if(fd<0)
   {
      printf("Unable to open '%s'!\n",argv[1]);
      return -1;
   }
 
  // 5 odvr\01
  // 13 fileinfo
   
   lseek(fd, 5+12, SEEK_SET);
   
   ret = read(fd, &quality, 1);   
   if(ret < 0) return -1;
   
   
   switch(quality)
   {
       case  0: freq = 10600; mode = 0; break;
       case  1: freq =  5750; mode = 0; break;
       case  2: freq = 16000; mode = 0; break;
       case  3: mode = 0; break;
       case  4: break;
       case  5: freq = 12000; pulcod_size = 36; mode = 2; max_size = 4032; break;
       case  6: freq =  8000; pulcod_size = 64; mode = 2; max_size = 7168; break;
       case  7: freq = 16000; pulcod_size = 24; mode = 2; max_size = 2688; break;
       case  8: freq = 16000; mode = 1; break;
       case  9: mode = 1; break;
       case 10: mode = 1; break;
   }
   
   if(pulcod_size)
   {
    ret = san_dec_pulcod2_init(freq, pulcod_size);
    if(ret < 0)
    {
      printf("Pulcod2_init (%d,%d) failed!\n", freq, pulcod_size);
      return -1;
    }
   }
   
   char filename[512];
   
   strncpy(filename, argv[1], 512);
   filename[511]=0;
   
   int str_len = strlen(filename);
   if(str_len < 4) str_len = 4;
   
   filename[str_len-0] = 0;
   filename[str_len-1] = 'v';
   filename[str_len-2] = 'a';
   filename[str_len-3] = 'w';
   filename[str_len-4] = '.';
   
   fd2 = open(filename, O_CREAT|O_WRONLY|O_BINARY|O_EXCL);
   if(fd2<0)
   {
      printf("Unable to open '%s'!\n", filename);
      return -1;
   }
   
   ret = wave_header(out, 0, freq);
 
   ret = write(fd2, out, ret);
  
   if(ret<0)
   {
      printf("Wave write error!\n");
      return -1;
   }
  
 
   do{
       
   len = 0;
   
   ret = read(fd, &len, 2);
   
   if(ret < 0) break;
   
   if(read(fd, in, len) <= 0) break;
   
   for(i=0; i<32*1024; i++) out[i] = 0;
 
   if(pulcod_size)
   {   
    int i;
    
    for(i = 2; i < 512; i += 256)
    {
     int k = 0;
     int count = 0;
     do
     {
        
      if(in[i+k] == 0x80)
      {
       int zero = 1, j;
        for(j = 1; j < 9; j++)
         if(in[i+k+j] != 0) zero = 0;
        if(zero) //silence to end
            break;
      }
     
      ret = san_dec(in+i+k, out+out_len, mode, bit_size);
     
      if(ret >= 0)
      {
       out_len += ret*2;
       k += 9;
      }
     
     }while(++count < 28);
    }
    if(out_len < max_size)
     out_len = max_size;
           
   }
   else // ! pulcod2
   {
    ret = san_dec(in+2, out+out_len, mode, bit_size);
     if(ret <= 0)
    break;
    out_len += ret*2;
   }
   
   ret = write(fd2, out, out_len);
   
   if(ret<0)
   {
      printf("Wave write error!\n");
      return -1;
   }
   
   data_size += out_len;
   out_len = 0;
   
   }while(len > 0);
       
   close(fd); 
   
   ret = lseek(fd2, 0, SEEK_SET);
   
   if(ret<0)
   {
      printf("Wave seek error!\n");
      return -1;
   }
   
   ret = wave_header(out, data_size, freq);
   
   ret = write(fd2, out, ret);
  
   if(ret<0)
   {
      printf("Wave write error!\n");
      return -1;
   }
   
   ret = close(fd2);
   
   if(ret<0)
   {
      printf("Wave close error!\n");
   }
   
   FreeLibrary(hLibrary);
   
   return 0;
}
