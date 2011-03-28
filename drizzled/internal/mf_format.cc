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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <config.h>

#include <drizzled/internal/my_sys.h>

#include <fcntl.h>

#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#include <algorithm>

#include <drizzled/internal/m_string.h>

using namespace std;

namespace drizzled
{
namespace internal
{

static size_t strlength(const char *str);

/*
  Formats a filename with possible replace of directory of extension
  Function can handle the case where 'to' == 'name'
  For a description of the flag values, consult my_sys.h
  The arguments should be in unix format.
*/

char * fn_format(char * to, const char *name, const char *dir,
		    const char *extension, uint32_t flag)
{
  char dev[FN_REFLEN], buff[FN_REFLEN], *pos;
  const char *startpos = name;
  const char *ext;
  size_t length;
  size_t dev_length;

  /* Copy and skip directory */
  name+=(length=dirname_part(dev, startpos, &dev_length));
  if (length == 0 || (flag & MY_REPLACE_DIR))
  {
    /* Use given directory */
    convert_dirname(dev,dir,NULL);		/* Fix to this OS */
  }
  else if ((flag & MY_RELATIVE_PATH) && !test_if_hard_path(dev))
  {
    /* Put 'dir' before the given path */
    strncpy(buff,dev,sizeof(buff)-1);
    pos=convert_dirname(dev,dir,NULL);
    strncpy(pos,buff,sizeof(buff)-1- (int) (pos-dev));
  }

  if (flag & MY_UNPACK_FILENAME)
    (void) unpack_dirname(dev,dev);		/* Replace ~/.. with dir */

  if (!(flag & MY_APPEND_EXT) &&
      (pos= (char*) strchr(name,FN_EXTCHAR)) != NULL)
  {
    if ((flag & MY_REPLACE_EXT) == 0)		/* If we should keep old ext */
    {
      length=strlength(name);			/* Use old extension */
      ext = "";
    }
    else
    {
      length= (size_t) (pos-(char*) name);	/* Change extension */
      ext= extension;
    }
  }
  else
  {
    length=strlength(name);			/* No ext, use the now one */
    ext=extension;
  }

  if (strlen(dev)+length+strlen(ext) >= FN_REFLEN || length >= FN_LEN )
  {
    /* To long path, return original or NULL */
    size_t tmp_length;
    if (flag & MY_SAFE_PATH)
      return NULL;
    tmp_length= min(strlength(startpos), (size_t)(FN_REFLEN-1));
    strncpy(to,startpos,tmp_length);
    to[tmp_length]= '\0';
  }
  else
  {
    if (to == startpos)
    {
      memmove(buff, name, length); /* Save name for last copy */
      name=buff;
    }
    char *tmp= strcpy(to, dev) + strlen(dev);
    pos= strncpy(tmp,name,length) + length;
    (void) strcpy(pos,ext);			/* Don't convert extension */
  }
  /*
    If MY_RETURN_REAL_PATH and MY_RESOLVE_SYMLINK is given, only do
    realpath if the file is a symbolic link
  */
  if (flag & MY_RETURN_REAL_PATH)
  {
    struct stat stat_buff;
    char rp_buff[PATH_MAX];
    if ((!flag & MY_RESOLVE_SYMLINKS) || 
       (!lstat(to,&stat_buff) && S_ISLNK(stat_buff.st_mode)))
    {
      if (!realpath(to,rp_buff))
        my_load_path(rp_buff, to, NULL);
      rp_buff[FN_REFLEN-1]= '\0';
      strcpy(to,rp_buff);
    }
  }
  else if (flag & MY_RESOLVE_SYMLINKS)
  {
    strcpy(buff,to);
    ssize_t sym_link_size= readlink(buff,to,FN_REFLEN-1);
    if (sym_link_size >= 0)
      to[sym_link_size]= '\0';
  }
  return(to);
} /* fn_format */


/*
  strlength(const string str)
  Return length of string with end-space:s not counted.
*/

static size_t strlength(const char *str)
{
  const char* found= str;
  const char* pos= str;

  while (*pos)
  {
    if (*pos != ' ')
    {
      while (*++pos && *pos != ' ') {};
      if (!*pos)
      {
	found=pos;			/* String ends here */
	break;
      }
    }
    found=pos;
    while (*++pos == ' ') {};
  }
  return((size_t) (found - str));
} /* strlength */

} /* namespace internal */
} /* namespace drizzled */
