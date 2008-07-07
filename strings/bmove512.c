/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*  File   : bmove512.c
    Author : Michael Widenius;
    Defines: bmove512()

    bmove512(dst, src, len) moves exactly "len" bytes from the source "src"
    to the destination "dst".  "src" and "dst" must be alligned on long
    boundory and len must be a mutliple of 512 byte. If len is not a
    multiple of 512 byte len/512*512+1 bytes is copyed.
    bmove512 is moustly used to copy IO_BLOCKS.  bmove512 should be the
    fastest way to move a mutiple of 512 byte.
*/

#include <my_global.h>
#include "m_string.h"

#ifndef bmove512

#define LONG ulonglong

void bmove512(uchar *to, const uchar *from, register size_t length)
{
  register LONG *f,*t,*end= (LONG*) ((char*) from+length);

  f= (LONG*) from;
  t= (LONG*) to;

#if defined(m88k) || defined(sparc) 
  do {
    t[0]=f[0];	    t[1]=f[1];	    t[2]=f[2];	    t[3]=f[3];
    t[4]=f[4];	    t[5]=f[5];	    t[6]=f[6];	    t[7]=f[7];
    t[8]=f[8];	    t[9]=f[9];	    t[10]=f[10];    t[11]=f[11];
    t[12]=f[12];    t[13]=f[13];    t[14]=f[14];    t[15]=f[15];
    t[16]=f[16];    t[17]=f[17];    t[18]=f[18];    t[19]=f[19];
    t[20]=f[20];    t[21]=f[21];    t[22]=f[22];    t[23]=f[23];
    t[24]=f[24];    t[25]=f[25];    t[26]=f[26];    t[27]=f[27];
    t[28]=f[28];    t[29]=f[29];    t[30]=f[30];    t[31]=f[31];
    t[32]=f[32];    t[33]=f[33];    t[34]=f[34];    t[35]=f[35];
    t[36]=f[36];    t[37]=f[37];    t[38]=f[38];    t[39]=f[39];
    t[40]=f[40];    t[41]=f[41];    t[42]=f[42];    t[43]=f[43];
    t[44]=f[44];    t[45]=f[45];    t[46]=f[46];    t[47]=f[47];
    t[48]=f[48];    t[49]=f[49];    t[50]=f[50];    t[51]=f[51];
    t[52]=f[52];    t[53]=f[53];    t[54]=f[54];    t[55]=f[55];
    t[56]=f[56];    t[57]=f[57];    t[58]=f[58];    t[59]=f[59];
    t[60]=f[60];    t[61]=f[61];    t[62]=f[62];    t[63]=f[63];
    t+=64; f+=64;
  } while (f < end);
#else
  do {
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;    *t++ = *f++;
  } while (f < end);
#endif
  return;
} /* bmove512 */

#endif /* bmove512 */
