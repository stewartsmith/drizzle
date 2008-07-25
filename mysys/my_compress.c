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

/* Written by Sinisa Milivojevic <sinisa@mysql.com> */

#include <my_global.h>
#include <my_sys.h>
#include <m_string.h>
#include <zlib.h>

/*
   This replaces the packet with a compressed packet

   SYNOPSIS
     my_compress()
     packet	Data to compress. This is is replaced with the compressed data.
     len	Length of data to compress at 'packet'
     complen	out: 0 if packet was not compressed

   RETURN
     1   error. 'len' is not changed'
     0   ok.  In this case 'len' contains the size of the compressed packet
*/

bool my_compress(uchar *packet, size_t *len, size_t *complen)
{
  if (*len < MIN_COMPRESS_LENGTH)
  {
    *complen=0;
  }
  else
  {
    uchar *compbuf=my_compress_alloc(packet,len,complen);
    if (!compbuf)
      return(*complen ? 0 : 1);
    memcpy(packet,compbuf,*len);
    my_free(compbuf,MYF(MY_WME));
  }
  return(0);
}


uchar *my_compress_alloc(const uchar *packet, size_t *len, size_t *complen)
{
  uchar *compbuf;
  uLongf tmp_complen;
  int res;
  *complen=  *len * 120 / 100 + 12;

  if (!(compbuf= (uchar *) my_malloc(*complen, MYF(MY_WME))))
    return 0;					/* Not enough memory */

  tmp_complen= *complen;
  res= compress((Bytef*) compbuf, &tmp_complen, (Bytef*) packet, (uLong) *len);
  *complen=    tmp_complen;

  if (res != Z_OK)
  {
    my_free(compbuf, MYF(MY_WME));
    return 0;
  }

  if (*complen >= *len)
  {
    *complen= 0;
    my_free(compbuf, MYF(MY_WME));
    return 0;
  }
  /* Store length of compressed packet in *len */
  swap_variables(size_t, *len, *complen);
  return compbuf;
}


/*
  Uncompress packet

   SYNOPSIS
     my_uncompress()
     packet	Compressed data. This is is replaced with the orignal data.
     len	Length of compressed data
     complen	Length of the packet buffer (must be enough for the original
	        data)

   RETURN
     1   error
     0   ok.  In this case 'complen' contains the updated size of the
              real data.
*/

bool my_uncompress(uchar *packet, size_t len, size_t *complen)
{
  uLongf tmp_complen;

  if (*complen)					/* If compressed */
  {
    uchar *compbuf= (uchar *) my_malloc(*complen,MYF(MY_WME));
    int error;
    if (!compbuf)
      return(1);				/* Not enough memory */

    tmp_complen= *complen;
    error= uncompress((Bytef*) compbuf, &tmp_complen, (Bytef*) packet,
                      (uLong) len);
    *complen= tmp_complen;
    if (error != Z_OK)
    {						/* Probably wrong packet */
      my_free(compbuf, MYF(MY_WME));
      return(1);
    }
    memcpy(packet, compbuf, *complen);
    my_free(compbuf, MYF(MY_WME));
  }
  else
    *complen= len;
  return(0);
}
