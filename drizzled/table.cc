/* Copyright (C) 2000-2006 MySQL AB

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


/* Some general useful functions */

#include <config.h>

#include <drizzled/server_includes.h>
#include <drizzled/error.h>
#include <drizzled/gettext.h>

#include "tmp_table.h"
#include "sj_tmp_table.h"

#include <string>

using namespace std;

/* INFORMATION_SCHEMA name */
LEX_STRING INFORMATION_SCHEMA_NAME= {C_STRING_WITH_LEN("information_schema")};

/* Keyword for parsing virtual column functions */
LEX_STRING parse_vcol_keyword= { C_STRING_WITH_LEN("PARSE_VCOL_EXPR ") };

/* Functions defined in this file */

void open_table_error(TABLE_SHARE *share, int error, int db_errno,
                      myf errortype, int errarg);
static int open_binary_frm(Session *session, TABLE_SHARE *share,
                           unsigned char *head, File file);
static void fix_type_pointers(const char ***array, TYPELIB *point_to_type,
			      uint32_t types, char **names);
static uint32_t find_field(Field **fields, unsigned char *record, uint32_t start, uint32_t length);

/*************************************************************************/

/* Get column name from column hash */

static unsigned char *get_field_name(Field **buff, size_t *length,
                             bool not_used __attribute__((unused)))
{
  *length= (uint) strlen((*buff)->field_name);
  return (unsigned char*) (*buff)->field_name;
}


/*
  Returns pointer to '.frm' extension of the file name.

  SYNOPSIS
    fn_rext()
    name       file name

  DESCRIPTION
    Checks file name part starting with the rightmost '.' character,
    and returns it if it is equal to '.frm'. 

  TODO
    It is a good idea to get rid of this function modifying the code
    to garantee that the functions presently calling fn_rext() always
    get arguments in the same format: either with '.frm' or without '.frm'.

  RETURN VALUES
    Pointer to the '.frm' extension. If there is no extension,
    or extension is not '.frm', pointer at the end of file name.
*/

char *fn_rext(char *name)
{
  char *res= strrchr(name, '.');
  if (res && !strcmp(res, reg_ext))
    return res;
  return name + strlen(name);
}

TABLE_CATEGORY get_table_category(const LEX_STRING *db, const LEX_STRING *name)
{
  assert(db != NULL);
  assert(name != NULL);

  if ((db->length == INFORMATION_SCHEMA_NAME.length) &&
      (my_strcasecmp(system_charset_info,
                    INFORMATION_SCHEMA_NAME.str,
                    db->str) == 0))
  {
    return TABLE_CATEGORY_INFORMATION;
  }

  return TABLE_CATEGORY_USER;
}


/*
  Allocate a setup TABLE_SHARE structure

  SYNOPSIS
    alloc_table_share()
    TableList		Take database and table name from there
    key			Table cache key (db \0 table_name \0...)
    key_length		Length of key

  RETURN
    0  Error (out of memory)
    #  Share
*/

TABLE_SHARE *alloc_table_share(TableList *table_list, char *key,
                               uint32_t key_length)
{
  MEM_ROOT mem_root;
  TABLE_SHARE *share;
  char *key_buff, *path_buff;
  char path[FN_REFLEN];
  uint32_t path_length;

  path_length= build_table_filename(path, sizeof(path) - 1,
                                    table_list->db,
                                    table_list->table_name, "", 0);
  init_sql_alloc(&mem_root, TABLE_ALLOC_BLOCK_SIZE, 0);
  if (multi_alloc_root(&mem_root,
                       &share, sizeof(*share),
                       &key_buff, key_length,
                       &path_buff, path_length + 1,
                       NULL))
  {
    memset(share, 0, sizeof(*share));

    share->set_table_cache_key(key_buff, key, key_length);

    share->path.str= path_buff;
    share->path.length= path_length;
    my_stpcpy(share->path.str, path);
    share->normalized_path.str=    share->path.str;
    share->normalized_path.length= path_length;

    share->version=       refresh_version;

    /*
      This constant is used to mark that no table map version has been
      assigned.  No arithmetic is done on the value: it will be
      overwritten with a value taken from DRIZZLE_BIN_LOG.
    */
    share->table_map_version= UINT64_MAX;

    /*
      Since alloc_table_share() can be called without any locking (for
      example, ha_create_table... functions), we do not assign a table
      map id here.  Instead we assign a value that is not used
      elsewhere, and then assign a table map id inside open_table()
      under the protection of the LOCK_open mutex.
    */
    share->table_map_id= UINT32_MAX;
    share->cached_row_logging_check= -1;

    memcpy(&share->mem_root, &mem_root, sizeof(mem_root));
    pthread_mutex_init(&share->mutex, MY_MUTEX_INIT_FAST);
    pthread_cond_init(&share->cond, NULL);
  }
  return(share);
}


/*
  Initialize share for temporary tables

  SYNOPSIS
    init_tmp_table_share()
    session         thread handle
    share	Share to fill
    key		Table_cache_key, as generated from create_table_def_key.
		must start with db name.    
    key_length	Length of key
    table_name	Table name
    path	Path to file (possible in lower case) without .frm

  NOTES
    This is different from alloc_table_share() because temporary tables
    don't have to be shared between threads or put into the table def
    cache, so we can do some things notable simpler and faster

    If table is not put in session->temporary_tables (happens only when
    one uses OPEN TEMPORARY) then one can specify 'db' as key and
    use key_length= 0 as neither table_cache_key or key_length will be used).
*/

void init_tmp_table_share(Session *session, TABLE_SHARE *share, const char *key,
                          uint32_t key_length, const char *table_name,
                          const char *path)
{

  memset(share, 0, sizeof(*share));
  init_sql_alloc(&share->mem_root, TABLE_ALLOC_BLOCK_SIZE, 0);
  share->table_category=         TABLE_CATEGORY_TEMPORARY;
  share->tmp_table=              INTERNAL_TMP_TABLE;
  share->db.str=                 (char*) key;
  share->db.length=		 strlen(key);
  share->table_cache_key.str=    (char*) key;
  share->table_cache_key.length= key_length;
  share->table_name.str=         (char*) table_name;
  share->table_name.length=      strlen(table_name);
  share->path.str=               (char*) path;
  share->normalized_path.str=    (char*) path;
  share->path.length= share->normalized_path.length= strlen(path);
  share->frm_version= 		 FRM_VER_TRUE_VARCHAR;
  /*
    Temporary tables are not replicated, but we set up these fields
    anyway to be able to catch errors.
   */
  share->table_map_version= ~(uint64_t)0;
  share->cached_row_logging_check= -1;

  /*
    table_map_id is also used for MERGE tables to suppress repeated
    compatibility checks.
  */
  share->table_map_id= (ulong) session->query_id;

  return;
}


/*
  Free table share and memory used by it

  SYNOPSIS
    free_table_share()
    share		Table share

  NOTES
    share->mutex must be locked when we come here if it's not a temp table
*/

void free_table_share(TABLE_SHARE *share)
{
  MEM_ROOT mem_root;
  assert(share->ref_count == 0);

  /*
    If someone is waiting for this to be deleted, inform it about this.
    Don't do a delete until we know that no one is refering to this anymore.
  */
  if (share->tmp_table == NO_TMP_TABLE)
  {
    /* share->mutex is locked in release_table_share() */
    while (share->waiting_on_cond)
    {
      pthread_cond_broadcast(&share->cond);
      pthread_cond_wait(&share->cond, &share->mutex);
    }
    /* No thread refers to this anymore */
    pthread_mutex_unlock(&share->mutex);
    pthread_mutex_destroy(&share->mutex);
    pthread_cond_destroy(&share->cond);
  }
  hash_free(&share->name_hash);
  
  plugin_unlock(NULL, share->db_plugin);
  share->db_plugin= NULL;

  /* We must copy mem_root from share because share is allocated through it */
  memcpy(&mem_root, &share->mem_root, sizeof(mem_root));
  free_root(&mem_root, MYF(0));                 // Free's share
  return;
}

/*
  Read table definition from a binary / text based .frm file
  
  SYNOPSIS
  open_table_def()
  session		Thread handler
  share		Fill this with table definition
  db_flags	Bit mask of the following flags: OPEN_VIEW

  NOTES
    This function is called when the table definition is not cached in
    table_def_cache
    The data is returned in 'share', which is alloced by
    alloc_table_share().. The code assumes that share is initialized.

  RETURN VALUES
   0	ok
   1	Error (see open_table_error)
   2    Error (see open_table_error)
   3    Wrong data in .frm file
   4    Error (see open_table_error)
   5    Error (see open_table_error: charset unavailable)
   6    Unknown .frm version
*/

int open_table_def(Session *session, TABLE_SHARE *share, uint32_t db_flags  __attribute__((unused)))
{
  int error, table_type;
  bool error_given;
  File file;
  unsigned char head[64], *disk_buff;
  string	path("");

  MEM_ROOT **root_ptr, *old_root;

  error= 1;
  error_given= 0;
  disk_buff= NULL;

  path.reserve(FN_REFLEN);
  path.append(share->normalized_path.str);
  path.append(reg_ext);
  if ((file= open(path.c_str(), O_RDONLY)) < 0)
  {
    /*
      We don't try to open 5.0 unencoded name, if
      - non-encoded name contains '@' signs, 
        because '@' can be misinterpreted.
        It is not clear if '@' is escape character in 5.1,
        or a normal character in 5.0.
        
      - non-encoded db or table name contain "#mysql50#" prefix.
        This kind of tables must have been opened only by the
        open() above.
    */
    if (strchr(share->table_name.str, '@') ||
        !strncmp(share->db.str, MYSQL50_TABLE_NAME_PREFIX,
                 MYSQL50_TABLE_NAME_PREFIX_LENGTH) ||
        !strncmp(share->table_name.str, MYSQL50_TABLE_NAME_PREFIX,
                 MYSQL50_TABLE_NAME_PREFIX_LENGTH))
      goto err_not_open;

    /* Try unencoded 5.0 name */
    size_t length;
    char unpacked_path[FN_REFLEN];
    path.clear();
    path.append(mysql_data_home);
    path.append("/");
    path.append(share->db.str);
    path.append("/");
    path.append(share->table_name.str);
    path.append(reg_ext);
    length= unpack_filename(unpacked_path, path.c_str()) - reg_ext_length;
    /*
      The following is a safety test and should never fail
      as the old file name should never be longer than the new one.
    */
    assert(length <= share->normalized_path.length);
    /*
      If the old and the new names have the same length,
      then table name does not have tricky characters,
      so no need to check the old file name.
    */
    if (length == share->normalized_path.length ||
        ((file= open(unpacked_path, O_RDONLY)) < 0))
      goto err_not_open;

    /* Unencoded 5.0 table name found */
    unpacked_path[length]= '\0'; // Remove .frm extension
    my_stpcpy(share->normalized_path.str, unpacked_path);
    share->normalized_path.length= length;
  }

  error= 4;
  if (my_read(file, head, 64, MYF(MY_NABP)))
    goto err;

  if (head[0] == (unsigned char) 254 && head[1] == 1)
  {
    if (head[2] == FRM_VER || head[2] == FRM_VER+1 ||
        (head[2] >= FRM_VER+3 && head[2] <= FRM_VER+4))
    {
      table_type= 1;
    }
    else
    {
      error= 6;                                 // Unkown .frm version
      goto err;
    }
  }
  else
    goto err;

  /* No handling of text based files yet */
  if (table_type == 1)
  {
    root_ptr= (MEM_ROOT **)pthread_getspecific(THR_MALLOC);
    old_root= *root_ptr;
    *root_ptr= &share->mem_root;
    error= open_binary_frm(session, share, head, file);
    *root_ptr= old_root;
    error_given= 1;
  }
  else
    assert(1);

  share->table_category= get_table_category(& share->db, & share->table_name);

  if (!error)
    session->status_var.opened_shares++;

err:
  my_close(file, MYF(MY_WME));

err_not_open:
  if (error && !error_given)
  {
    share->error= error;
    open_table_error(share, error, (share->open_errno= my_errno), 0);
  }

  return(error);
}


/*
  Read data from a binary .frm file from MySQL 3.23 - 5.0 into TABLE_SHARE
*/

static int open_binary_frm(Session *session, TABLE_SHARE *share, unsigned char *head,
                           File file)
{
  int error, errarg= 0;
  uint32_t new_frm_ver, field_pack_length, new_field_pack_flag;
  uint32_t interval_count, interval_parts, read_length, int_length;
  uint32_t db_create_options, keys, key_parts, n_length;
  uint32_t key_info_length, com_length, null_bit_pos=0;
  uint32_t vcol_screen_length;
  uint32_t extra_rec_buf_length;
  uint32_t i,j;
  bool use_hash;
  unsigned char forminfo[288];
  char *keynames, *names, *comment_pos, *vcol_screen_pos;
  unsigned char *record;
  unsigned char *disk_buff, *strpos, *null_flags=NULL, *null_pos=NULL;
  ulong pos, record_offset, *rec_per_key, rec_buff_length;
  handler *handler_file= 0;
  KEY	*keyinfo;
  KEY_PART_INFO *key_part;
  Field  **field_ptr, *reg_field;
  const char **interval_array;
  enum legacy_db_type legacy_db_type;
  my_bitmap_map *bitmaps;
  unsigned char *buff= 0;
  unsigned char *field_extra_info= 0;

  new_field_pack_flag= head[27];
  new_frm_ver= (head[2] - FRM_VER);
  field_pack_length= new_frm_ver < 2 ? 11 : 17;
  disk_buff= 0;

  error= 3;
  if (!(pos=get_form_pos(file,head,(TYPELIB*) 0)))
    goto err;                                   /* purecov: inspected */
  my_seek(file,pos,MY_SEEK_SET,MYF(0));
  if (my_read(file,forminfo,288,MYF(MY_NABP)))
    goto err;

  share->frm_version= head[2];
  /*
    Check if .frm file created by MySQL 5.0. In this case we want to
    display CHAR fields as CHAR and not as VARCHAR.
    We do it this way as we want to keep the old frm version to enable
    MySQL 4.1 to read these files.
  */
  if (share->frm_version == FRM_VER_TRUE_VARCHAR -1 && head[33] == 5)
    share->frm_version= FRM_VER_TRUE_VARCHAR;

  legacy_db_type= DB_TYPE_FIRST_DYNAMIC;
  assert(share->db_plugin == NULL);
  /*
    if the storage engine is dynamic, no point in resolving it by its
    dynamically allocated legacy_db_type. We will resolve it later by name.
  */
  if (legacy_db_type > DB_TYPE_UNKNOWN && 
      legacy_db_type < DB_TYPE_FIRST_DYNAMIC)
    share->db_plugin= ha_lock_engine(NULL, 
                                     ha_checktype(session, legacy_db_type, 0, 0));
  share->db_create_options= db_create_options= uint2korr(head+30);
  share->db_options_in_use= share->db_create_options;
  share->mysql_version= uint4korr(head+51);
  share->null_field_first= 0;
  if (!head[32])				// New frm file in 3.23
  {
    share->avg_row_length= uint4korr(head+34);
    share->transactional= (ha_choice) (head[39] & 3);
    share->page_checksum= (ha_choice) ((head[39] >> 2) & 3);
    share->row_type= (row_type) head[40];
    share->block_size= uint4korr(head+43);
    share->table_charset= get_charset((uint) head[38],MYF(0));
    share->null_field_first= 1;
  }
  if (!share->table_charset)
  {
    /* unknown charset in head[38] or pre-3.23 frm */
    if (use_mb(default_charset_info))
    {
      /* Warn that we may be changing the size of character columns */
      sql_print_warning(_("'%s' had no or invalid character set, "
                        "and default character set is multi-byte, "
                        "so character column sizes may have changed"),
                        share->path.str);
    }
    share->table_charset= default_charset_info;
  }
  share->db_record_offset= 1;
  if (db_create_options & HA_OPTION_LONG_BLOB_PTR)
    share->blob_ptr_size= portable_sizeof_char_ptr;
  /* Set temporarily a good value for db_low_byte_first */
  share->db_low_byte_first= true;
  error=4;
  share->max_rows= uint4korr(head+18);
  share->min_rows= uint4korr(head+22);

  /* Read keyinformation */
  key_info_length= (uint) uint2korr(head+28);
  my_seek(file,(ulong) uint2korr(head+6),MY_SEEK_SET,MYF(0));
  if (read_string(file,(unsigned char**) &disk_buff,key_info_length))
    goto err;                                   /* purecov: inspected */
  if (disk_buff[0] & 0x80)
  {
    share->keys=      keys=      (disk_buff[1] << 7) | (disk_buff[0] & 0x7f);
    share->key_parts= key_parts= uint2korr(disk_buff+2);
  }
  else
  {
    share->keys=      keys=      disk_buff[0];
    share->key_parts= key_parts= disk_buff[1];
  }
  share->keys_for_keyread.init(0);
  share->keys_in_use.init(keys);

  n_length=keys*sizeof(KEY)+key_parts*sizeof(KEY_PART_INFO);
  if (!(keyinfo = (KEY*) alloc_root(&share->mem_root,
				    n_length + uint2korr(disk_buff+4))))
    goto err;                                   /* purecov: inspected */
  memset(keyinfo, 0, n_length);
  share->key_info= keyinfo;
  key_part= reinterpret_cast<KEY_PART_INFO*> (keyinfo+keys);
  strpos=disk_buff+6;

  if (!(rec_per_key= (ulong*) alloc_root(&share->mem_root,
					 sizeof(ulong*)*key_parts)))
    goto err;

  for (i=0 ; i < keys ; i++, keyinfo++)
  {
    keyinfo->table= 0;                           // Updated in open_frm
    if (new_frm_ver >= 3)
    {
      keyinfo->flags=	   (uint) uint2korr(strpos) ^ HA_NOSAME;
      keyinfo->key_length= (uint) uint2korr(strpos+2);
      keyinfo->key_parts=  (uint) strpos[4];
      keyinfo->algorithm=  (enum ha_key_alg) strpos[5];
      keyinfo->block_size= uint2korr(strpos+6);
      strpos+=8;
    }

    keyinfo->key_part=	 key_part;
    keyinfo->rec_per_key= rec_per_key;
    for (j=keyinfo->key_parts ; j-- ; key_part++)
    {
      *rec_per_key++=0;
      key_part->fieldnr=	(uint16_t) (uint2korr(strpos) & FIELD_NR_MASK);
      key_part->offset= (uint) uint2korr(strpos+2)-1;
      key_part->key_type=	(uint) uint2korr(strpos+5);
      // key_part->field=	(Field*) 0;	// Will be fixed later
      if (new_frm_ver >= 1)
      {
	key_part->key_part_flag= *(strpos+4);
	key_part->length=	(uint) uint2korr(strpos+7);
	strpos+=9;
      }
      else
      {
	key_part->length=	*(strpos+4);
	key_part->key_part_flag=0;
	if (key_part->length > 128)
	{
	  key_part->length&=127;		/* purecov: inspected */
	  key_part->key_part_flag=HA_REVERSE_SORT; /* purecov: inspected */
	}
	strpos+=7;
      }
      key_part->store_length=key_part->length;
    }
  }
  keynames=(char*) key_part;
  strpos+= (my_stpcpy(keynames, (char *) strpos) - keynames)+1;

  //reading index comments
  for (keyinfo= share->key_info, i=0; i < keys; i++, keyinfo++)
  {
    if (keyinfo->flags & HA_USES_COMMENT)
    {
      keyinfo->comment.length= uint2korr(strpos);
      keyinfo->comment.str= strmake_root(&share->mem_root, (char*) strpos+2,
                                         keyinfo->comment.length);
      strpos+= 2 + keyinfo->comment.length;
    } 
    assert(test(keyinfo->flags & HA_USES_COMMENT) == 
               (keyinfo->comment.length > 0));
  }

  share->reclength = uint2korr((head+16));
  share->stored_rec_length= share->reclength;

  record_offset= (ulong) (uint2korr(head+6)+
                          ((uint2korr(head+14) == 0xffff ?
                            uint4korr(head+47) : uint2korr(head+14))));
 
  if ((n_length= uint4korr(head+55)))
  {
    /* Read extra data segment */
    unsigned char *next_chunk, *buff_end;
    if (!(next_chunk= buff= (unsigned char*) my_malloc(n_length, MYF(MY_WME))))
      goto err;
    if (pread(file, buff, n_length, record_offset + share->reclength) == 0)
    {
      goto err;
    }
    share->connect_string.length= uint2korr(buff);
    if (!(share->connect_string.str= strmake_root(&share->mem_root,
                                                  (char*) next_chunk + 2,
                                                  share->connect_string.
                                                  length)))
    {
      goto err;
    }
    next_chunk+= share->connect_string.length + 2;
    buff_end= buff + n_length;
    if (next_chunk + 2 < buff_end)
    {
      uint32_t str_db_type_length= uint2korr(next_chunk);
      LEX_STRING name;
      name.str= (char*) next_chunk + 2;
      name.length= str_db_type_length;

      plugin_ref tmp_plugin= ha_resolve_by_name(session, &name);
      if (tmp_plugin != NULL && !plugin_equals(tmp_plugin, share->db_plugin))
      {
        if (legacy_db_type > DB_TYPE_UNKNOWN &&
            legacy_db_type < DB_TYPE_FIRST_DYNAMIC &&
            legacy_db_type != ha_legacy_type(
                plugin_data(tmp_plugin, handlerton *)))
        {
          /* bad file, legacy_db_type did not match the name */
          free(buff);
          goto err;
        }
        /*
          tmp_plugin is locked with a local lock.
          we unlock the old value of share->db_plugin before
          replacing it with a globally locked version of tmp_plugin
        */
        plugin_unlock(NULL, share->db_plugin);
        share->db_plugin= my_plugin_lock(NULL, &tmp_plugin);
      }
      else if (!tmp_plugin)
      {
        /* purecov: begin inspected */
        error= 8;
        my_error(ER_UNKNOWN_STORAGE_ENGINE, MYF(0), name.str);
        free(buff);
        goto err;
        /* purecov: end */
      }
      next_chunk+= str_db_type_length + 2;
    }
    if (share->mysql_version >= 50110)
    {
      /* New auto_partitioned indicator introduced in 5.1.11 */
      next_chunk++;
    }
    if (forminfo[46] == (unsigned char)255)
    {
      //reading long table comment
      if (next_chunk + 2 > buff_end)
      {
          free(buff);
          goto err;
      }
      share->comment.length = uint2korr(next_chunk);
      if (! (share->comment.str= strmake_root(&share->mem_root,
                               (char*)next_chunk + 2, share->comment.length)))
      {
          free(buff);
          goto err;
      }
      next_chunk+= 2 + share->comment.length;
    }
    assert(next_chunk <= buff_end);
    if (share->mysql_version >= DRIZZLE_VERSION_TABLESPACE_IN_FRM_CGE)
    {
      /*
       New frm format in mysql_version 5.2.5 (originally in
       mysql-5.1.22-ndb-6.2.5)
       New column properties added:
       COLUMN_FORMAT DYNAMIC|FIXED and STORAGE DISK|MEMORY
       TABLESPACE name is now stored in frm
      */
      if (next_chunk >= buff_end)
      {
        if (share->mysql_version >= DRIZZLE_VERSION_TABLESPACE_IN_FRM)
        {
          goto err;
        }
      }
      else
      {
        const uint32_t format_section_header_size= 8;
        uint32_t format_section_len= uint2korr(next_chunk+0);

        field_extra_info= next_chunk + format_section_header_size + 1;
        next_chunk+= format_section_len;
      }
    }
    assert (next_chunk <= buff_end);
    if (next_chunk > buff_end)
    {
      goto err;
    }
  }
  share->key_block_size= uint2korr(head+62);

  error=4;
  extra_rec_buf_length= uint2korr(head+59);
  rec_buff_length= ALIGN_SIZE(share->reclength + 1 + extra_rec_buf_length);
  share->rec_buff_length= rec_buff_length;
  if (!(record= (unsigned char *) alloc_root(&share->mem_root,
                                     rec_buff_length)))
    goto err;                                   /* purecov: inspected */
  share->default_values= record;
  if (pread(file, record, (size_t) share->reclength, record_offset) == 0)
    goto err;                                   /* purecov: inspected */

  my_seek(file,pos+288,MY_SEEK_SET,MYF(0));

  share->fields= uint2korr(forminfo+258);
  pos= uint2korr(forminfo+260);			/* Length of all screens */
  n_length= uint2korr(forminfo+268);
  interval_count= uint2korr(forminfo+270);
  interval_parts= uint2korr(forminfo+272);
  int_length= uint2korr(forminfo+274);
  share->null_fields= uint2korr(forminfo+282);
  com_length= uint2korr(forminfo+284);
  vcol_screen_length= uint2korr(forminfo+286);
  share->vfields= 0;
  share->stored_fields= share->fields;
  if (forminfo[46] != (unsigned char)255)
  {
    share->comment.length=  (int) (forminfo[46]);
    share->comment.str= strmake_root(&share->mem_root, (char*) forminfo+47,
                                     share->comment.length);
  }


  if (!(field_ptr = (Field **)
	alloc_root(&share->mem_root,
		   (uint) ((share->fields+1)*sizeof(Field*)+
			   interval_count*sizeof(TYPELIB)+
			   (share->fields+interval_parts+
			    keys+3)*sizeof(char *)+
			   (n_length+int_length+com_length+
			       vcol_screen_length)))))
    goto err;                                   /* purecov: inspected */

  share->field= field_ptr;
  read_length=(uint) (share->fields * field_pack_length +
		      pos+ (uint) (n_length+int_length+com_length+
		                   vcol_screen_length));
  if (read_string(file,(unsigned char**) &disk_buff,read_length))
    goto err;                                   /* purecov: inspected */
  strpos= disk_buff+pos;

  share->intervals= (TYPELIB*) (field_ptr+share->fields+1);
  interval_array= (const char **) (share->intervals+interval_count);
  names= (char*) (interval_array+share->fields+interval_parts+keys+3);
  if (!interval_count)
    share->intervals= 0;			// For better debugging
  memcpy(names, strpos+(share->fields*field_pack_length),
	 (uint) (n_length+int_length));
  comment_pos= names+(n_length+int_length);
  memcpy(comment_pos, disk_buff+read_length-com_length-vcol_screen_length, 
         com_length);
  vcol_screen_pos= names+(n_length+int_length+com_length);
  memcpy(vcol_screen_pos, disk_buff+read_length-vcol_screen_length, 
         vcol_screen_length);

  fix_type_pointers(&interval_array, &share->fieldnames, 1, &names);
  if (share->fieldnames.count != share->fields)
    goto err;
  fix_type_pointers(&interval_array, share->intervals, interval_count,
		    &names);

  {
    /* Set ENUM and SET lengths */
    TYPELIB *interval;
    for (interval= share->intervals;
         interval < share->intervals + interval_count;
         interval++)
    {
      uint32_t count= (uint) (interval->count + 1) * sizeof(uint);
      if (!(interval->type_lengths= (uint32_t *) alloc_root(&share->mem_root,
                                                        count)))
        goto err;
      for (count= 0; count < interval->count; count++)
      {
        char *val= (char*) interval->type_names[count];
        interval->type_lengths[count]= strlen(val);
      }
      interval->type_lengths[count]= 0;
    }
  }

  if (keynames)
    fix_type_pointers(&interval_array, &share->keynames, 1, &keynames);

 /* Allocate handler */
  if (!(handler_file= get_new_handler(share, session->mem_root,
                                      share->db_type())))
    goto err;

  record= share->default_values-1;              /* Fieldstart = 1 */
  if (share->null_field_first)
  {
    null_flags= null_pos= (unsigned char*) record+1;
    null_bit_pos= (db_create_options & HA_OPTION_PACK_RECORD) ? 0 : 1;
    /*
      null_bytes below is only correct under the condition that
      there are no bit fields.  Correct values is set below after the
      table struct is initialized
    */
    share->null_bytes= (share->null_fields + null_bit_pos + 7) / 8;
  }

  use_hash= share->fields >= MAX_FIELDS_BEFORE_HASH;
  if (use_hash)
    use_hash= !hash_init(&share->name_hash,
			 system_charset_info,
			 share->fields,0,0,
			 (hash_get_key) get_field_name,0,0);

  for (i=0 ; i < share->fields; i++, strpos+=field_pack_length, field_ptr++)
  {
    uint32_t pack_flag, interval_nr, unireg_type, recpos, field_length;
    uint32_t vcol_info_length=0;
    enum_field_types field_type;
    enum column_format_type column_format= COLUMN_FORMAT_TYPE_DEFAULT;
    const CHARSET_INFO *charset= NULL;
    LEX_STRING comment;
    virtual_column_info *vcol_info= NULL;
    bool fld_is_stored= true;

    if (field_extra_info)
    {
      char tmp= field_extra_info[i];
      column_format= (enum column_format_type)
                    ((tmp >> COLUMN_FORMAT_SHIFT) & COLUMN_FORMAT_MASK);
    }
    if (new_frm_ver >= 3)
    {
      /* new frm file in 4.1 */
      field_length= uint2korr(strpos+3);
      recpos=	    uint3korr(strpos+5);
      pack_flag=    uint2korr(strpos+8);
      unireg_type=  (uint) strpos[10];
      interval_nr=  (uint) strpos[12];
      uint32_t comment_length=uint2korr(strpos+15);
      field_type=(enum_field_types) (uint) strpos[13];

      {
        if (!strpos[14])
          charset= &my_charset_bin;
        else if (!(charset=get_charset((uint) strpos[14], MYF(0))))
        {
          error= 5; // Unknown or unavailable charset
          errarg= (int) strpos[14];
          goto err;
        }
      }
      if (field_type == DRIZZLE_TYPE_VIRTUAL)
      {
        assert(interval_nr); // Expect non-null expression
        /* 
          The interval_id byte in the .frm file stores the length of the
          expression statement for a virtual column.
        */
        vcol_info_length= interval_nr;
        interval_nr= 0;
      }
      if (!comment_length)
      {
	comment.str= (char*) "";
	comment.length=0;
      }
      else
      {
	comment.str=    (char*) comment_pos;
	comment.length= comment_length;
	comment_pos+=   comment_length;
      }
      if (vcol_info_length)
      {
        /*
          Get virtual column data stored in the .frm file as follows:
          byte 1      = 1 (always 1 to allow for future extensions)
          byte 2      = sql_type
          byte 3      = flags (as of now, 0 - no flags, 1 - field is physically stored)
          byte 4-...  = virtual column expression (text data)
        */
        vcol_info= new virtual_column_info();
        if ((uint)vcol_screen_pos[0] != 1)
        {
          error= 4;
          goto err;
        }
        field_type= (enum_field_types) (unsigned char) vcol_screen_pos[1];
        fld_is_stored= (bool) (uint) vcol_screen_pos[2];
        vcol_info->expr_str.str= (char *)memdup_root(&share->mem_root,
                                                     vcol_screen_pos+
                                                       (uint)FRM_VCOL_HEADER_SIZE,
                                                     vcol_info_length-
                                                       (uint)FRM_VCOL_HEADER_SIZE);
        vcol_info->expr_str.length= vcol_info_length-(uint)FRM_VCOL_HEADER_SIZE;
        vcol_screen_pos+= vcol_info_length;
        share->vfields++;
      }
    }
    else
    {
      field_length= (uint) strpos[3];
      recpos=	    uint2korr(strpos+4),
      pack_flag=    uint2korr(strpos+6);
      pack_flag&=   ~FIELDFLAG_NO_DEFAULT;     // Safety for old files
      unireg_type=  (uint) strpos[8];
      interval_nr=  (uint) strpos[10];

      /* old frm file */
      field_type= (enum_field_types) f_packtype(pack_flag);
      if (f_is_binary(pack_flag))
      {
        /*
          Try to choose the best 4.1 type:
          - for 4.0 "CHAR(N) BINARY" or "VARCHAR(N) BINARY" 
            try to find a binary collation for character set.
          - for other types (e.g. BLOB) just use my_charset_bin. 
        */
        if (!f_is_blob(pack_flag))
        {
          // 3.23 or 4.0 string
          if (!(charset= get_charset_by_csname(share->table_charset->csname,
                                               MY_CS_BINSORT, MYF(0))))
            charset= &my_charset_bin;
        }
        else
          charset= &my_charset_bin;
      }
      else
        charset= share->table_charset;
      memset(&comment, 0, sizeof(comment));
    }

    if (interval_nr && charset->mbminlen > 1)
    {
      /* Unescape UCS2 intervals from HEX notation */
      TYPELIB *interval= share->intervals + interval_nr - 1;
      unhex_type2(interval);
    }

    *field_ptr= reg_field=
      make_field(share, record+recpos,
		 (uint32_t) field_length,
		 null_pos, null_bit_pos,
		 pack_flag,
		 field_type,
		 charset,
		 (Field::utype) MTYP_TYPENR(unireg_type),
		 (interval_nr ?
		  share->intervals+interval_nr-1 :
		  (TYPELIB*) 0),
		 share->fieldnames.type_names[i]);
    if (!reg_field)				// Not supported field type
    {
      error= 4;
      goto err;			/* purecov: inspected */
    }

    reg_field->flags|= ((uint)column_format << COLUMN_FORMAT_FLAGS);
    reg_field->field_index= i;
    reg_field->comment=comment;
    reg_field->vcol_info= vcol_info;
    reg_field->is_stored= fld_is_stored;
    if (!(reg_field->flags & NOT_NULL_FLAG))
    {
      if (!(null_bit_pos= (null_bit_pos + 1) & 7))
        null_pos++;
    }
    if (f_no_default(pack_flag))
      reg_field->flags|= NO_DEFAULT_VALUE_FLAG;

    if (reg_field->unireg_check == Field::NEXT_NUMBER)
      share->found_next_number_field= field_ptr;
    if (share->timestamp_field == reg_field)
      share->timestamp_field_offset= i;

    if (use_hash)
      (void) my_hash_insert(&share->name_hash,
                            (unsigned char*) field_ptr); // never fail
    if (!reg_field->is_stored)
    {
      share->stored_fields--;
      if (share->stored_rec_length>=recpos)
        share->stored_rec_length= recpos-1;
    }
  }
  *field_ptr=0;					// End marker
  /* Sanity checks: */
  assert(share->fields>=share->stored_fields);
  assert(share->reclength>=share->stored_rec_length);

  /* Fix key->name and key_part->field */
  if (key_parts)
  {
    uint32_t primary_key=(uint) (find_type((char*) primary_key_name,
				       &share->keynames, 3) - 1);
    int64_t ha_option= handler_file->ha_table_flags();
    keyinfo= share->key_info;
    key_part= keyinfo->key_part;

    for (uint32_t key=0 ; key < share->keys ; key++,keyinfo++)
    {
      uint32_t usable_parts= 0;
      keyinfo->name=(char*) share->keynames.type_names[key];

      if (primary_key >= MAX_KEY && (keyinfo->flags & HA_NOSAME))
      {
	/*
	  If the UNIQUE key doesn't have NULL columns and is not a part key
	  declare this as a primary key.
	*/
	primary_key=key;
	for (i=0 ; i < keyinfo->key_parts ;i++)
	{
	  uint32_t fieldnr= key_part[i].fieldnr;
	  if (!fieldnr ||
	      share->field[fieldnr-1]->null_ptr ||
	      share->field[fieldnr-1]->key_length() !=
	      key_part[i].length)
	  {
	    primary_key=MAX_KEY;		// Can't be used
	    break;
	  }
	}
      }

      for (i=0 ; i < keyinfo->key_parts ; key_part++,i++)
      {
        Field *field;
	if (new_field_pack_flag <= 1)
	  key_part->fieldnr= (uint16_t) find_field(share->field,
                                                 share->default_values,
                                                 (uint) key_part->offset,
                                                 (uint) key_part->length);
	if (!key_part->fieldnr)
        {
          error= 4;                             // Wrong file
          goto err;
        }
        field= key_part->field= share->field[key_part->fieldnr-1];
        key_part->type= field->key_type();
        if (field->null_ptr)
        {
          key_part->null_offset=(uint) ((unsigned char*) field->null_ptr -
                                        share->default_values);
          key_part->null_bit= field->null_bit;
          key_part->store_length+=HA_KEY_NULL_LENGTH;
          keyinfo->flags|=HA_NULL_PART_KEY;
          keyinfo->extra_length+= HA_KEY_NULL_LENGTH;
          keyinfo->key_length+= HA_KEY_NULL_LENGTH;
        }
        if (field->type() == DRIZZLE_TYPE_BLOB ||
            field->real_type() == DRIZZLE_TYPE_VARCHAR)
        {
          if (field->type() == DRIZZLE_TYPE_BLOB)
            key_part->key_part_flag|= HA_BLOB_PART;
          else
            key_part->key_part_flag|= HA_VAR_LENGTH_PART;
          keyinfo->extra_length+=HA_KEY_BLOB_LENGTH;
          key_part->store_length+=HA_KEY_BLOB_LENGTH;
          keyinfo->key_length+= HA_KEY_BLOB_LENGTH;
        }
        if (i == 0 && key != primary_key)
          field->flags |= (((keyinfo->flags & HA_NOSAME) &&
                           (keyinfo->key_parts == 1)) ?
                           UNIQUE_KEY_FLAG : MULTIPLE_KEY_FLAG);
        if (i == 0)
          field->key_start.set_bit(key);
        if (field->key_length() == key_part->length &&
            !(field->flags & BLOB_FLAG))
        {
          if (handler_file->index_flags(key, i, 0) & HA_KEYREAD_ONLY)
          {
            share->keys_for_keyread.set_bit(key);
            field->part_of_key.set_bit(key);
            field->part_of_key_not_clustered.set_bit(key);
          }
          if (handler_file->index_flags(key, i, 1) & HA_READ_ORDER)
            field->part_of_sortkey.set_bit(key);
        }
        if (!(key_part->key_part_flag & HA_REVERSE_SORT) &&
            usable_parts == i)
          usable_parts++;			// For FILESORT
        field->flags|= PART_KEY_FLAG;
        if (key == primary_key)
        {
          field->flags|= PRI_KEY_FLAG;
          /*
            If this field is part of the primary key and all keys contains
            the primary key, then we can use any key to find this column
          */
          if (ha_option & HA_PRIMARY_KEY_IN_READ_INDEX)
          {
            field->part_of_key= share->keys_in_use;
            if (field->part_of_sortkey.is_set(key))
              field->part_of_sortkey= share->keys_in_use;
          }
        }
        if (field->key_length() != key_part->length)
        {
          key_part->key_part_flag|= HA_PART_KEY_SEG;
        }
      }
      keyinfo->usable_key_parts= usable_parts; // Filesort

      set_if_bigger(share->max_key_length,keyinfo->key_length+
                    keyinfo->key_parts);
      share->total_key_length+= keyinfo->key_length;
      /*
        MERGE tables do not have unique indexes. But every key could be
        an unique index on the underlying MyISAM table. (Bug #10400)
      */
      if ((keyinfo->flags & HA_NOSAME) ||
          (ha_option & HA_ANY_INDEX_MAY_BE_UNIQUE))
        set_if_bigger(share->max_unique_length,keyinfo->key_length);
    }
    if (primary_key < MAX_KEY &&
	(share->keys_in_use.is_set(primary_key)))
    {
      share->primary_key= primary_key;
      /*
	If we are using an integer as the primary key then allow the user to
	refer to it as '_rowid'
      */
      if (share->key_info[primary_key].key_parts == 1)
      {
	Field *field= share->key_info[primary_key].key_part[0].field;
	if (field && field->result_type() == INT_RESULT)
        {
          /* note that fieldnr here (and rowid_field_offset) starts from 1 */
	  share->rowid_field_offset= (share->key_info[primary_key].key_part[0].
                                      fieldnr);
        }
      }
    }
    else
      share->primary_key = MAX_KEY; // we do not have a primary key
  }
  else
    share->primary_key= MAX_KEY;
  if (disk_buff)
    free(disk_buff);
  disk_buff= NULL;
  if (new_field_pack_flag <= 1)
  {
    /* Old file format with default as not null */
    uint32_t null_length= (share->null_fields+7)/8;
    memset(share->default_values + (null_flags - (unsigned char*) record), 
          null_length, 255);
  }

  if (share->found_next_number_field)
  {
    reg_field= *share->found_next_number_field;
    if ((int) (share->next_number_index= (uint)
	       find_ref_key(share->key_info, share->keys,
                            share->default_values, reg_field,
			    &share->next_number_key_offset,
                            &share->next_number_keypart)) < 0)
    {
      /* Wrong field definition */
      error= 4;
      goto err;
    }
    else
      reg_field->flags |= AUTO_INCREMENT_FLAG;
  }

  if (share->blob_fields)
  {
    Field **ptr;
    uint32_t k, *save;

    /* Store offsets to blob fields to find them fast */
    if (!(share->blob_field= save=
	  (uint*) alloc_root(&share->mem_root,
                             (uint) (share->blob_fields* sizeof(uint)))))
      goto err;
    for (k=0, ptr= share->field ; *ptr ; ptr++, k++)
    {
      if ((*ptr)->flags & BLOB_FLAG)
	(*save++)= k;
    }
  }

  /*
    the correct null_bytes can now be set, since bitfields have been taken
    into account
  */
  share->null_bytes= (null_pos - (unsigned char*) null_flags +
                      (null_bit_pos + 7) / 8);
  share->last_null_bit_pos= null_bit_pos;

  share->db_low_byte_first= handler_file->low_byte_first();
  share->column_bitmap_size= bitmap_buffer_size(share->fields);

  if (!(bitmaps= (my_bitmap_map*) alloc_root(&share->mem_root,
                                             share->column_bitmap_size)))
    goto err;
  bitmap_init(&share->all_set, bitmaps, share->fields, false);
  bitmap_set_all(&share->all_set);

  delete handler_file;
  if (buff)
    free(buff);
  return (0);

 err:
  if (buff)
    free(buff);
  share->error= error;
  share->open_errno= my_errno;
  share->errarg= errarg;
  if (disk_buff)
    free(disk_buff);
  delete handler_file;
  hash_free(&share->name_hash);

  open_table_error(share, error, share->open_errno, errarg);
  return(error);
} /* open_binary_frm */


/*
  Clear flag GET_FIXED_FIELDS_FLAG in all fields of the table.
  This routine is used for error handling purposes.

  SYNOPSIS
    clear_field_flag()
    table                Table object for which virtual columns are set-up

  RETURN VALUE
    NONE
*/
static void clear_field_flag(Table *table)
{
  Field **ptr;

  for (ptr= table->field; *ptr; ptr++)
    (*ptr)->flags&= (~GET_FIXED_FIELDS_FLAG);
}

/*
  The function uses the feature in fix_fields where the flag 
  GET_FIXED_FIELDS_FLAG is set for all fields in the item tree.
  This field must always be reset before returning from the function
  since it is used for other purposes as well.

  SYNOPSIS
    fix_fields_vcol_func()
    session                  The thread object
    func_item            The item tree reference of the virtual columnfunction
    table                The table object
    field_name           The name of the processed field

  RETURN VALUE
    true                 An error occurred, something was wrong with the
                         function.
    false                Ok, a partition field array was created
*/

bool fix_fields_vcol_func(Session *session,
                          Item* func_expr,
                          Table *table,
                          const char *field_name)
{
  uint dir_length, home_dir_length;
  bool result= true;
  TableList tables;
  TableList *save_table_list, *save_first_table, *save_last_table;
  int error;
  Name_resolution_context *context;
  const char *save_where;
  char* db_name;
  char db_name_string[FN_REFLEN];
  bool save_use_only_table_context;
  Field **ptr, *field;
  enum_mark_columns save_mark_used_columns= session->mark_used_columns;
  assert(func_expr);

  /*
    Set-up the TABLE_LIST object to be a list with a single table
    Set the object to zero to create NULL pointers and set alias
    and real name to table name and get database name from file name.
  */

  bzero((void*)&tables, sizeof(TableList));
  tables.alias= tables.table_name= (char*) table->s->table_name.str;
  tables.table= table;
  tables.next_local= NULL;
  tables.next_name_resolution_table= NULL;
  memcpy(db_name_string,
         table->s->normalized_path.str,
         table->s->normalized_path.length);
  dir_length= dirname_length(db_name_string);
  db_name_string[dir_length - 1]= 0;
  home_dir_length= dirname_length(db_name_string);
  db_name= &db_name_string[home_dir_length];
  tables.db= db_name;

  session->mark_used_columns= MARK_COLUMNS_NONE;

  context= session->lex->current_context();
  table->map= 1; //To ensure correct calculation of const item
  table->get_fields_in_item_tree= true;
  save_table_list= context->table_list;
  save_first_table= context->first_name_resolution_table;
  save_last_table= context->last_name_resolution_table;
  context->table_list= &tables;
  context->first_name_resolution_table= &tables;
  context->last_name_resolution_table= NULL;
  func_expr->walk(&Item::change_context_processor, 0, (unsigned char*) context);
  save_where= session->where;
  session->where= "virtual column function";

  /* Save the context before fixing the fields*/
  save_use_only_table_context= session->lex->use_only_table_context;
  session->lex->use_only_table_context= true;
  /* Fix fields referenced to by the virtual column function */
  error= func_expr->fix_fields(session, (Item**)0);
  /* Restore the original context*/
  session->lex->use_only_table_context= save_use_only_table_context;
  context->table_list= save_table_list;
  context->first_name_resolution_table= save_first_table;
  context->last_name_resolution_table= save_last_table;

  if (unlikely(error))
  {
    clear_field_flag(table);
    goto end;
  }
  session->where= save_where;
  /* 
    Walk through the Item tree checking if all items are valid 
   to be part of the virtual column
 */
  error= func_expr->walk(&Item::check_vcol_func_processor, 0, NULL);
  if (error)
  {
    my_error(ER_VIRTUAL_COLUMN_FUNCTION_IS_NOT_ALLOWED, MYF(0), field_name);
    clear_field_flag(table);
    goto end;
  }
  if (unlikely(func_expr->const_item()))
  {
    my_error(ER_CONST_EXPR_IN_VCOL, MYF(0));
    clear_field_flag(table);
    goto end;
  }
  /* Ensure that this virtual column is not based on another virtual field. */
  ptr= table->field;
  while ((field= *(ptr++))) 
  {
    if ((field->flags & GET_FIXED_FIELDS_FLAG) &&
        (field->vcol_info))
    {
      my_error(ER_VCOL_BASED_ON_VCOL, MYF(0));
      clear_field_flag(table);
      goto end;
    }
  }
  /*
    Cleanup the fields marked with flag GET_FIXED_FIELDS_FLAG
    when calling fix_fields.
  */
  clear_field_flag(table);
  result= false;

end:
  table->get_fields_in_item_tree= false;
  session->mark_used_columns= save_mark_used_columns;
  table->map= 0; //Restore old value
  return(result);
}

/*
  Unpack the definition of a virtual column

  SYNOPSIS
    unpack_vcol_info_from_frm()
    session                  Thread handler
    table                Table with the checked field
    field                Pointer to Field object
    open_mode            Open table mode needed to determine
                         which errors need to be generated in a failure
    error_reported       updated flag for the caller that no other error
                         messages are to be generated.

  RETURN VALUES
    true            Failure
    false           Success
*/
bool unpack_vcol_info_from_frm(Session *session,
                               Table *table,
                               Field *field,
                               LEX_STRING *vcol_expr,
                               open_table_mode open_mode,
                               bool *error_reported)
{
  assert(vcol_expr);

  /* 
    Step 1: Construct a statement for the parser.
    The parsed string needs to take the following format:
    "PARSE_VCOL_EXPR (<expr_string_from_frm>)"
  */
  char *vcol_expr_str;
  int str_len= 0;
  
  if (!(vcol_expr_str= (char*) alloc_root(&table->mem_root,
                                          vcol_expr->length + 
                                            parse_vcol_keyword.length + 3)))
  {
    return(true);
  }
  memcpy(vcol_expr_str,
         (char*) parse_vcol_keyword.str,
         parse_vcol_keyword.length);
  str_len= parse_vcol_keyword.length;
  memcpy(vcol_expr_str + str_len, "(", 1);
  str_len++;
  memcpy(vcol_expr_str + str_len, 
         (char*) vcol_expr->str, 
         vcol_expr->length);
  str_len+= vcol_expr->length;
  memcpy(vcol_expr_str + str_len, ")", 1);
  str_len++;
  memcpy(vcol_expr_str + str_len, "\0", 1);
  str_len++;
  Lex_input_stream lip(session, vcol_expr_str, str_len);

  /* 
    Step 2: Setup session for parsing.
    1) make Item objects be created in the memory allocated for the Table
       object (not TABLE_SHARE)
    2) ensure that created Item's are not put on to session->free_list 
       (which is associated with the parsed statement and hence cleared after 
       the parsing)
    3) setup a flag in the LEX structure to allow "PARSE_VCOL_EXPR" 
       to be parsed as a SQL command.
  */
  MEM_ROOT **root_ptr, *old_root;
  Item *backup_free_list= session->free_list;
  root_ptr= (MEM_ROOT **)pthread_getspecific(THR_MALLOC);
  old_root= *root_ptr;
  *root_ptr= &table->mem_root;
  session->free_list= NULL;
  session->lex->parse_vcol_expr= true;

  /* 
    Step 3: Use the parser to build an Item object from.
  */
  if (parse_sql(session, &lip))
  {
    goto parse_err;
  }
  /* From now on use vcol_info generated by the parser. */
  field->vcol_info= session->lex->vcol_info;

  /* Validate the Item tree. */
  if (fix_fields_vcol_func(session,
                           field->vcol_info->expr_item,
                           table,
                           field->field_name))
  {
    if (open_mode == OTM_CREATE)
    {
      /*
        During CREATE/ALTER TABLE it is ok to receive errors here.
        It is not ok if it happens during the opening of an frm
        file as part of a normal query.
      */
      *error_reported= true;
    }
    field->vcol_info= NULL;
    goto parse_err;
  }
  field->vcol_info->item_free_list= session->free_list;
  session->free_list= backup_free_list;
  *root_ptr= old_root;

  return(false);

parse_err:
  session->lex->parse_vcol_expr= false;
  session->free_items();
  *root_ptr= old_root;
  session->free_list= backup_free_list;
  return(true);
}


/*
  Open a table based on a TABLE_SHARE

  SYNOPSIS
    open_table_from_share()
    session			Thread handler
    share		Table definition
    alias       	Alias for table
    db_stat		open flags (for example HA_OPEN_KEYFILE|
    			HA_OPEN_RNDFILE..) can be 0 (example in
                        ha_example_table)
    prgflag   		READ_ALL etc..
    ha_open_flags	HA_OPEN_ABORT_IF_LOCKED etc..
    outparam       	result table
    open_mode           One of OTM_OPEN|OTM_CREATE|OTM_ALTER
                        if OTM_CREATE some errors are ignore
                        if OTM_ALTER HA_OPEN is not called

  RETURN VALUES
   0	ok
   1	Error (see open_table_error)
   2    Error (see open_table_error)
   3    Wrong data in .frm file
   4    Error (see open_table_error)
   5    Error (see open_table_error: charset unavailable)
   7    Table definition has changed in engine
*/

int open_table_from_share(Session *session, TABLE_SHARE *share, const char *alias,
                          uint32_t db_stat, uint32_t prgflag, uint32_t ha_open_flags,
                          Table *outparam, open_table_mode open_mode)
{
  int error;
  uint32_t records, i, bitmap_size;
  bool error_reported= false;
  unsigned char *record, *bitmaps;
  Field **field_ptr, **vfield_ptr;

  /* Parsing of partitioning information from .frm needs session->lex set up. */
  assert(session->lex->is_lex_started);

  error= 1;
  memset(outparam, 0, sizeof(*outparam));
  outparam->in_use= session;
  outparam->s= share;
  outparam->db_stat= db_stat;
  outparam->write_row_record= NULL;

  init_sql_alloc(&outparam->mem_root, TABLE_ALLOC_BLOCK_SIZE, 0);

  if (!(outparam->alias= my_strdup(alias, MYF(MY_WME))))
    goto err;
  outparam->quick_keys.init();
  outparam->covering_keys.init();
  outparam->keys_in_use_for_query.init();

  /* Allocate handler */
  outparam->file= 0;
  if (!(prgflag & OPEN_FRM_FILE_ONLY))
  {
    if (!(outparam->file= get_new_handler(share, &outparam->mem_root,
                                          share->db_type())))
      goto err;
  }
  else
  {
    assert(!db_stat);
  }

  error= 4;
  outparam->reginfo.lock_type= TL_UNLOCK;
  outparam->current_lock= F_UNLCK;
  records=0;
  if ((db_stat & HA_OPEN_KEYFILE) || (prgflag & DELAYED_OPEN))
    records=1;
  if (prgflag & (READ_ALL+EXTRA_RECORD))
    records++;

  if (!(record= (unsigned char*) alloc_root(&outparam->mem_root,
                                   share->rec_buff_length * records)))
    goto err;                                   /* purecov: inspected */

  if (records == 0)
  {
    /* We are probably in hard repair, and the buffers should not be used */
    outparam->record[0]= outparam->record[1]= share->default_values;
  }
  else
  {
    outparam->record[0]= record;
    if (records > 1)
      outparam->record[1]= record+ share->rec_buff_length;
    else
      outparam->record[1]= outparam->record[0];   // Safety
  }

#ifdef HAVE_purify
  /*
    We need this because when we read var-length rows, we are not updating
    bytes after end of varchar
  */
  if (records > 1)
  {
    memcpy(outparam->record[0], share->default_values, share->rec_buff_length);
    memcpy(outparam->record[1], share->default_values, share->null_bytes);
    if (records > 2)
      memcpy(outparam->record[1], share->default_values,
             share->rec_buff_length);
  }
#endif

  if (!(field_ptr = (Field **) alloc_root(&outparam->mem_root,
                                          (uint) ((share->fields+1)*
                                                  sizeof(Field*)))))
    goto err;                                   /* purecov: inspected */

  outparam->field= field_ptr;

  record= (unsigned char*) outparam->record[0]-1;	/* Fieldstart = 1 */
  if (share->null_field_first)
    outparam->null_flags= (unsigned char*) record+1;
  else
    outparam->null_flags= (unsigned char*) (record+ 1+ share->reclength -
                                    share->null_bytes);

  /* Setup copy of fields from share, but use the right alias and record */
  for (i=0 ; i < share->fields; i++, field_ptr++)
  {
    if (!((*field_ptr)= share->field[i]->clone(&outparam->mem_root, outparam)))
      goto err;
  }
  (*field_ptr)= 0;                              // End marker

  if (share->found_next_number_field)
    outparam->found_next_number_field=
      outparam->field[(uint) (share->found_next_number_field - share->field)];
  if (share->timestamp_field)
    outparam->timestamp_field= (Field_timestamp*) outparam->field[share->timestamp_field_offset];


  /* Fix key->name and key_part->field */
  if (share->key_parts)
  {
    KEY	*key_info, *key_info_end;
    KEY_PART_INFO *key_part;
    uint32_t n_length;
    n_length= share->keys*sizeof(KEY) + share->key_parts*sizeof(KEY_PART_INFO);
    if (!(key_info= (KEY*) alloc_root(&outparam->mem_root, n_length)))
      goto err;
    outparam->key_info= key_info;
    key_part= (reinterpret_cast<KEY_PART_INFO*> (key_info+share->keys));
    
    memcpy(key_info, share->key_info, sizeof(*key_info)*share->keys);
    memcpy(key_part, share->key_info[0].key_part, (sizeof(*key_part) *
                                                   share->key_parts));

    for (key_info_end= key_info + share->keys ;
         key_info < key_info_end ;
         key_info++)
    {
      KEY_PART_INFO *key_part_end;

      key_info->table= outparam;
      key_info->key_part= key_part;

      for (key_part_end= key_part+ key_info->key_parts ;
           key_part < key_part_end ;
           key_part++)
      {
        Field *field= key_part->field= outparam->field[key_part->fieldnr-1];

        if (field->key_length() != key_part->length &&
            !(field->flags & BLOB_FLAG))
        {
          /*
            We are using only a prefix of the column as a key:
            Create a new field for the key part that matches the index
          */
          field= key_part->field=field->new_field(&outparam->mem_root,
                                                  outparam, 0);
          field->field_length= key_part->length;
        }
      }
    }
  }

  /*
    Process virtual columns, if any.
  */
  if (not (vfield_ptr = (Field **) alloc_root(&outparam->mem_root,
                                              (uint) ((share->vfields+1)*
                                                      sizeof(Field*)))))
    goto err;

  outparam->vfield= vfield_ptr;
  
  for (field_ptr= outparam->field; *field_ptr; field_ptr++)
  {
    if ((*field_ptr)->vcol_info)
    {
      if (unpack_vcol_info_from_frm(session,
                                    outparam,
                                    *field_ptr,
                                    &(*field_ptr)->vcol_info->expr_str,
                                    open_mode,
                                    &error_reported))
      {
        error= 4; // in case no error is reported
        goto err;
      }
      *(vfield_ptr++)= *field_ptr;
    }
  }
  *vfield_ptr= NULL;                              // End marker
  /* Check virtual columns against table's storage engine. */
  if ((share->vfields && outparam->file) && 
        (not outparam->file->check_if_supported_virtual_columns()))
  {
    my_error(ER_UNSUPPORTED_ACTION_ON_VIRTUAL_COLUMN,
             MYF(0), 
             "Specified storage engine");
    error_reported= true;
    goto err;
  }

  /* Allocate bitmaps */

  bitmap_size= share->column_bitmap_size;
  if (!(bitmaps= (unsigned char*) alloc_root(&outparam->mem_root, bitmap_size*3)))
    goto err;
  bitmap_init(&outparam->def_read_set,
              (my_bitmap_map*) bitmaps, share->fields, false);
  bitmap_init(&outparam->def_write_set,
              (my_bitmap_map*) (bitmaps+bitmap_size), share->fields, false);
  bitmap_init(&outparam->tmp_set,
              (my_bitmap_map*) (bitmaps+bitmap_size*2), share->fields, false);
  outparam->default_column_bitmaps();

  /* The table struct is now initialized;  Open the table */
  error= 2;
  if (db_stat && open_mode != OTM_ALTER)
  {
    int ha_err;
    if ((ha_err= (outparam->file->
                  ha_open(outparam, share->normalized_path.str,
                          (db_stat & HA_READ_ONLY ? O_RDONLY : O_RDWR),
                          (db_stat & HA_OPEN_TEMPORARY ? HA_OPEN_TMP_TABLE :
                           (db_stat & HA_WAIT_IF_LOCKED) ?  HA_OPEN_WAIT_IF_LOCKED :
                           (db_stat & (HA_ABORT_IF_LOCKED | HA_GET_INFO)) ?
                          HA_OPEN_ABORT_IF_LOCKED :
                           HA_OPEN_IGNORE_IF_LOCKED) | ha_open_flags))))
    {
      /* Set a flag if the table is crashed and it can be auto. repaired */
      share->crashed= ((ha_err == HA_ERR_CRASHED_ON_USAGE) &&
                       outparam->file->auto_repair() &&
                       !(ha_open_flags & HA_OPEN_FOR_REPAIR));

      switch (ha_err)
      {
        case HA_ERR_NO_SUCH_TABLE:
	  /*
            The table did not exists in storage engine, use same error message
            as if the .frm file didn't exist
          */
	  error= 1;
	  my_errno= ENOENT;
          break;
        case EMFILE:
	  /*
            Too many files opened, use same error message as if the .frm
            file can't open
           */
	  error= 1;
	  my_errno= EMFILE;
          break;
        default:
          outparam->file->print_error(ha_err, MYF(0));
          error_reported= true;
          if (ha_err == HA_ERR_TABLE_DEF_CHANGED)
            error= 7;
          break;
      }
      goto err;                                 /* purecov: inspected */
    }
  }

#if defined(HAVE_purify) 
  memset(bitmaps, 0, bitmap_size*3);
#endif

  outparam->no_replicate= outparam->file;
  session->status_var.opened_tables++;

  return (0);

 err:
  if (!error_reported && !(prgflag & DONT_GIVE_ERROR))
    open_table_error(share, error, my_errno, 0);
  delete outparam->file;
  outparam->file= 0;				// For easier error checking
  outparam->db_stat=0;
  free_root(&outparam->mem_root, MYF(0));       // Safe to call on zeroed root
  free((char*) outparam->alias);
  return (error);
}


/*
  Free information allocated by openfrm

  SYNOPSIS
    closefrm()
    table		Table object to free
    free_share		Is 1 if we also want to free table_share
*/

int closefrm(register Table *table, bool free_share)
{
  int error=0;

  if (table->db_stat)
    error=table->file->close();
  free((char*) table->alias);
  table->alias= 0;
  if (table->field)
  {
    for (Field **ptr=table->field ; *ptr ; ptr++)
      delete *ptr;
    table->field= 0;
  }
  delete table->file;
  table->file= 0;				/* For easier errorchecking */
  if (free_share)
  {
    if (table->s->tmp_table == NO_TMP_TABLE)
      release_table_share(table->s, RELEASE_NORMAL);
    else
      free_table_share(table->s);
  }
  free_root(&table->mem_root, MYF(0));
  return(error);
}


/* Deallocate temporary blob storage */

void free_blobs(register Table *table)
{
  uint32_t *ptr, *end;
  for (ptr= table->getBlobField(), end=ptr + table->sizeBlobFields();
       ptr != end ;
       ptr++)
    ((Field_blob*) table->field[*ptr])->free();
}


	/* Find where a form starts */
	/* if formname is NULL then only formnames is read */

ulong get_form_pos(File file, unsigned char *head, TYPELIB *save_names)
{
  uint32_t a_length,names,length;
  unsigned char *pos,*buf;
  ulong ret_value=0;

  names=uint2korr(head+8);
  a_length=(names+2)*sizeof(char *);		/* Room for two extra */

  if (!save_names)
    a_length=0;
  else
    save_names->type_names=0;			/* Clear if error */

  if (names)
  {
    length=uint2korr(head+4);
    my_seek(file,64L,MY_SEEK_SET,MYF(0));
    if (!(buf= (unsigned char*) my_malloc((size_t) length+a_length+names*4,
				  MYF(MY_WME))) ||
	my_read(file, buf+a_length, (size_t) (length+names*4),
		MYF(MY_NABP)))
    {						/* purecov: inspected */
      if (buf)
        free(buf);
      return(0L);				/* purecov: inspected */
    }
    pos= buf+a_length+length;
    ret_value=uint4korr(pos);
  }
  if (! save_names)
  {
    if (names)
      free((unsigned char*) buf);
  }
  else if (!names)
    memset(save_names, 0, sizeof(save_names));
  else
  {
    char *str;
    str=(char *) (buf+a_length);
    fix_type_pointers((const char ***) &buf,save_names,1,&str);
  }
  return(ret_value);
}


/*
  Read string from a file with malloc

  NOTES:
    We add an \0 at end of the read string to make reading of C strings easier
*/

int read_string(File file, unsigned char**to, size_t length)
{

  if (*to)
    free(*to);
  if (!(*to= (unsigned char*) my_malloc(length+1,MYF(MY_WME))) ||
      my_read(file, *to, length,MYF(MY_NABP)))
  {
    if (*to)
      free(*to);
    *to= NULL;
    return(1);                           /* purecov: inspected */
  }
  *((char*) *to+length)= '\0';
  return (0);
} /* read_string */


	/* Add a new form to a form file */

ulong make_new_entry(File file, unsigned char *fileinfo, TYPELIB *formnames,
		     const char *newname)
{
  uint32_t i,bufflength,maxlength,n_length,length,names;
  ulong endpos,newpos;
  unsigned char buff[IO_SIZE];
  unsigned char *pos;

  length=(uint) strlen(newname)+1;
  n_length=uint2korr(fileinfo+4);
  maxlength=uint2korr(fileinfo+6);
  names=uint2korr(fileinfo+8);
  newpos=uint4korr(fileinfo+10);

  if (64+length+n_length+(names+1)*4 > maxlength)
  {						/* Expand file */
    newpos+=IO_SIZE;
    int4store(fileinfo+10,newpos);
    endpos=(ulong) my_seek(file,0L,MY_SEEK_END,MYF(0));/* Copy from file-end */
    bufflength= (uint) (endpos & (IO_SIZE-1));	/* IO_SIZE is a power of 2 */

    while (endpos > maxlength)
    {
      my_seek(file,(ulong) (endpos-bufflength),MY_SEEK_SET,MYF(0));
      if (my_read(file, buff, bufflength, MYF(MY_NABP+MY_WME)))
	return(0L);
      my_seek(file,(ulong) (endpos-bufflength+IO_SIZE),MY_SEEK_SET,
		   MYF(0));
      if ((my_write(file, buff,bufflength,MYF(MY_NABP+MY_WME))))
	return(0);
      endpos-=bufflength; bufflength=IO_SIZE;
    }
    memset(buff, 0, IO_SIZE);			/* Null new block */
    my_seek(file,(ulong) maxlength,MY_SEEK_SET,MYF(0));
    if (my_write(file,buff,bufflength,MYF(MY_NABP+MY_WME)))
	return(0L);
    maxlength+=IO_SIZE;				/* Fix old ref */
    int2store(fileinfo+6,maxlength);
    for (i=names, pos= (unsigned char*) *formnames->type_names+n_length-1; i-- ;
	 pos+=4)
    {
      endpos=uint4korr(pos)+IO_SIZE;
      int4store(pos,endpos);
    }
  }

  if (n_length == 1 )
  {						/* First name */
    length++;
    strxmov((char*) buff,"/",newname,"/",NULL);
  }
  else
    strxmov((char*) buff,newname,"/",NULL); /* purecov: inspected */
  my_seek(file,63L+(ulong) n_length,MY_SEEK_SET,MYF(0));
  if (my_write(file, buff, (size_t) length+1,MYF(MY_NABP+MY_WME)) ||
      (names && my_write(file,(unsigned char*) (*formnames->type_names+n_length-1),
			 names*4, MYF(MY_NABP+MY_WME))) ||
      my_write(file, fileinfo+10, 4,MYF(MY_NABP+MY_WME)))
    return(0L); /* purecov: inspected */

  int2store(fileinfo+8,names+1);
  int2store(fileinfo+4,n_length+length);
  assert(ftruncate(file, newpos)==0);/* Append file with '\0' */
  return(newpos);
} /* make_new_entry */


	/* error message when opening a form file */

void open_table_error(TABLE_SHARE *share, int error, int db_errno, int errarg)
{
  int err_no;
  char buff[FN_REFLEN];
  myf errortype= ME_ERROR+ME_WAITTANG;

  switch (error) {
  case 7:
  case 1:
    if (db_errno == ENOENT)
      my_error(ER_NO_SUCH_TABLE, MYF(0), share->db.str, share->table_name.str);
    else
    {
      strxmov(buff, share->normalized_path.str, reg_ext, NULL);
      my_error((db_errno == EMFILE) ? ER_CANT_OPEN_FILE : ER_FILE_NOT_FOUND,
               errortype, buff, db_errno);
    }
    break;
  case 2:
  {
    handler *file= 0;
    const char *datext= "";
    
    if (share->db_type() != NULL)
    {
      if ((file= get_new_handler(share, current_session->mem_root,
                                 share->db_type())))
      {
        if (!(datext= *file->bas_ext()))
          datext= "";
      }
    }
    err_no= (db_errno == ENOENT) ? ER_FILE_NOT_FOUND : (db_errno == EAGAIN) ?
      ER_FILE_USED : ER_CANT_OPEN_FILE;
    strxmov(buff, share->normalized_path.str, datext, NULL);
    my_error(err_no,errortype, buff, db_errno);
    delete file;
    break;
  }
  case 5:
  {
    const char *csname= get_charset_name((uint) errarg);
    char tmp[10];
    if (!csname || csname[0] =='?')
    {
      snprintf(tmp, sizeof(tmp), "#%d", errarg);
      csname= tmp;
    }
    my_printf_error(ER_UNKNOWN_COLLATION,
                    _("Unknown collation '%s' in table '%-.64s' definition"), 
                    MYF(0), csname, share->table_name.str);
    break;
  }
  case 6:
    strxmov(buff, share->normalized_path.str, reg_ext, NULL);
    my_printf_error(ER_NOT_FORM_FILE,
                    _("Table '%-.64s' was created with a different version "
                    "of MySQL and cannot be read"), 
                    MYF(0), buff);
    break;
  case 8:
    break;
  default:				/* Better wrong error than none */
  case 4:
    strxmov(buff, share->normalized_path.str, reg_ext, NULL);
    my_error(ER_NOT_FORM_FILE, errortype, buff, 0);
    break;
  }
  return;
} /* open_table_error */


	/*
	** fix a str_type to a array type
	** typeparts separated with some char. differents types are separated
	** with a '\0'
	*/

static void
fix_type_pointers(const char ***array, TYPELIB *point_to_type, uint32_t types,
		  char **names)
{
  char *type_name, *ptr;
  char chr;

  ptr= *names;
  while (types--)
  {
    point_to_type->name=0;
    point_to_type->type_names= *array;

    if ((chr= *ptr))			/* Test if empty type */
    {
      while ((type_name=strchr(ptr+1,chr)) != NULL)
      {
	*((*array)++) = ptr+1;
	*type_name= '\0';		/* End string */
	ptr=type_name;
      }
      ptr+=2;				/* Skip end mark and last 0 */
    }
    else
      ptr++;
    point_to_type->count= (uint) (*array - point_to_type->type_names);
    point_to_type++;
    *((*array)++)= NULL;		/* End of type */
  }
  *names=ptr;				/* Update end */
  return;
} /* fix_type_pointers */


TYPELIB *typelib(MEM_ROOT *mem_root, List<String> &strings)
{
  TYPELIB *result= (TYPELIB*) alloc_root(mem_root, sizeof(TYPELIB));
  if (!result)
    return 0;
  result->count=strings.elements;
  result->name="";
  uint32_t nbytes= (sizeof(char*) + sizeof(uint)) * (result->count + 1);
  if (!(result->type_names= (const char**) alloc_root(mem_root, nbytes)))
    return 0;
  result->type_lengths= (uint*) (result->type_names + result->count + 1);
  List_iterator<String> it(strings);
  String *tmp;
  for (uint32_t i=0; (tmp=it++) ; i++)
  {
    result->type_names[i]= tmp->ptr();
    result->type_lengths[i]= tmp->length();
  }
  result->type_names[result->count]= 0;		// End marker
  result->type_lengths[result->count]= 0;
  return result;
}


/*
 Search after a field with given start & length
 If an exact field isn't found, return longest field with starts
 at right position.
 
 NOTES
   This is needed because in some .frm fields 'fieldnr' was saved wrong

 RETURN
   0  error
   #  field number +1
*/

static uint32_t find_field(Field **fields, unsigned char *record, uint32_t start, uint32_t length)
{
  Field **field;
  uint32_t i, pos;

  pos= 0;
  for (field= fields, i=1 ; *field ; i++,field++)
  {
    if ((*field)->offset(record) == start)
    {
      if ((*field)->key_length() == length)
	return (i);
      if (!pos || fields[pos-1]->pack_length() <
	  (*field)->pack_length())
	pos= i;
    }
  }
  return (pos);
}


	/* Check that the integer is in the internal */

int set_zone(register int nr, int min_zone, int max_zone)
{
  if (nr<=min_zone)
    return (min_zone);
  if (nr>=max_zone)
    return (max_zone);
  return (nr);
} /* set_zone */

	/* Adjust number to next larger disk buffer */

ulong next_io_size(register ulong pos)
{
  register ulong offset;
  if ((offset= pos & (IO_SIZE-1)))
    return pos-offset+IO_SIZE;
  return pos;
} /* next_io_size */


/*
  Store an SQL quoted string.

  SYNOPSIS  
    append_unescaped()
    res		result String
    pos		string to be quoted
    length	it's length

  NOTE
    This function works correctly with utf8 or single-byte charset strings.
    May fail with some multibyte charsets though.
*/

void append_unescaped(String *res, const char *pos, uint32_t length)
{
  const char *end= pos+length;
  res->append('\'');

  for (; pos != end ; pos++)
  {
#if defined(USE_MB)
    uint32_t mblen;
    if (use_mb(default_charset_info) &&
        (mblen= my_ismbchar(default_charset_info, pos, end)))
    {
      res->append(pos, mblen);
      pos+= mblen;
      continue;
    }
#endif

    switch (*pos) {
    case 0:				/* Must be escaped for 'mysql' */
      res->append('\\');
      res->append('0');
      break;
    case '\n':				/* Must be escaped for logs */
      res->append('\\');
      res->append('n');
      break;
    case '\r':
      res->append('\\');		/* This gives better readability */
      res->append('r');
      break;
    case '\\':
      res->append('\\');		/* Because of the sql syntax */
      res->append('\\');
      break;
    case '\'':
      res->append('\'');		/* Because of the sql syntax */
      res->append('\'');
      break;
    default:
      res->append(*pos);
      break;
    }
  }
  res->append('\'');
}


	/* Create a .frm file */

File create_frm(Session *session, const char *name, const char *db,
                const char *table, uint32_t reclength, unsigned char *fileinfo,
  		HA_CREATE_INFO *create_info, uint32_t keys, KEY *key_info)
{
  register File file;
  ulong length;
  unsigned char fill[IO_SIZE];
  int create_flags= O_RDWR | O_TRUNC;
  ulong key_comment_total_bytes= 0;
  uint32_t i;

  if (create_info->options & HA_LEX_CREATE_TMP_TABLE)
    create_flags|= O_EXCL;

  /* Fix this when we have new .frm files;  Current limit is 4G rows (QQ) */
  if (create_info->max_rows > UINT32_MAX)
    create_info->max_rows= UINT32_MAX;
  if (create_info->min_rows > UINT32_MAX)
    create_info->min_rows= UINT32_MAX;

  if ((file= my_create(name, CREATE_MODE, create_flags, MYF(0))) >= 0)
  {
    uint32_t key_length, tmp_key_length;
    uint32_t tmp;
    memset(fileinfo, 0, 64);
    /* header */
    fileinfo[0]=(unsigned char) 254;
    fileinfo[1]= 1;
    fileinfo[2]= FRM_VER+3+ test(create_info->varchar);

    fileinfo[3]= (unsigned char) ha_legacy_type(
          ha_checktype(session,ha_legacy_type(create_info->db_type),0,0));
    fileinfo[4]=1;
    int2store(fileinfo+6,IO_SIZE);		/* Next block starts here */
    for (i= 0; i < keys; i++)
    {
      assert(test(key_info[i].flags & HA_USES_COMMENT) == 
                 (key_info[i].comment.length > 0));
      if (key_info[i].flags & HA_USES_COMMENT)
        key_comment_total_bytes += 2 + key_info[i].comment.length;
    }
    /*
      Keep in sync with pack_keys() in unireg.cc
      For each key:
      8 bytes for the key header
      9 bytes for each key-part (MAX_REF_PARTS)
      NAME_LEN bytes for the name
      1 byte for the NAMES_SEP_CHAR (before the name)
      For all keys:
      6 bytes for the header
      1 byte for the NAMES_SEP_CHAR (after the last name)
      9 extra bytes (padding for safety? alignment?)
      comments
    */
    key_length= (keys * (8 + MAX_REF_PARTS * 9 + NAME_LEN + 1) + 16 +
                 key_comment_total_bytes);
    length= next_io_size((ulong) (IO_SIZE+key_length+reclength+
                                  create_info->extra_size));
    int4store(fileinfo+10,length);
    tmp_key_length= (key_length < 0xffff) ? key_length : 0xffff;
    int2store(fileinfo+14,tmp_key_length);
    int2store(fileinfo+16,reclength);
    int4store(fileinfo+18,create_info->max_rows);
    int4store(fileinfo+22,create_info->min_rows);
    /* fileinfo[26] is set in mysql_create_frm() */
    fileinfo[27]=2;				// Use long pack-fields
    /* fileinfo[28 & 29] is set to key_info_length in mysql_create_frm() */
    create_info->table_options|=HA_OPTION_LONG_BLOB_PTR; // Use portable blob pointers
    int2store(fileinfo+30,create_info->table_options);
    fileinfo[32]=0;				// No filename anymore
    fileinfo[33]=5;                             // Mark for 5.0 frm file
    int4store(fileinfo+34,create_info->avg_row_length);
    fileinfo[38]= (create_info->default_table_charset ?
		   create_info->default_table_charset->number : 0);
    fileinfo[39]= (unsigned char) create_info->page_checksum;
    fileinfo[40]= (unsigned char) create_info->row_type;
    /* Next few bytes were for RAID support */
    fileinfo[41]= 0;
    fileinfo[42]= 0;
    int4store(fileinfo+43,create_info->block_size);
 
    fileinfo[44]= 0;
    fileinfo[45]= 0;
    fileinfo[46]= 0;
    int4store(fileinfo+47, key_length);
    tmp= DRIZZLE_VERSION_ID;          // Store to avoid warning from int4store
    int4store(fileinfo+51, tmp);
    int4store(fileinfo+55, create_info->extra_size);
    /*
      59-60 is reserved for extra_rec_buf_length,
      61 for default_part_db_type
    */
    int2store(fileinfo+62, create_info->key_block_size);
    memset(fill, 0, IO_SIZE);
    for (; length > IO_SIZE ; length-= IO_SIZE)
    {
      if (my_write(file,fill, IO_SIZE, MYF(MY_WME | MY_NABP)))
      {
	my_close(file,MYF(0));
	my_delete(name,MYF(0));
	return(-1);
      }
    }
  }
  else
  {
    if (my_errno == ENOENT)
      my_error(ER_BAD_DB_ERROR,MYF(0),db);
    else
      my_error(ER_CANT_CREATE_TABLE,MYF(0),table,my_errno);
  }
  return (file);
} /* create_frm */

/*
  Set up column usage bitmaps for a temporary table

  IMPLEMENTATION
    For temporary tables, we need one bitmap with all columns set and
    a tmp_set bitmap to be used by things like filesort.
*/

void Table::setup_tmp_table_column_bitmaps(unsigned char *bitmaps)
{
  uint32_t field_count= s->fields;

  bitmap_init(&this->def_read_set, (my_bitmap_map*) bitmaps, field_count, false);
  bitmap_init(&this->tmp_set, (my_bitmap_map*) (bitmaps+ bitmap_buffer_size(field_count)), field_count, false);

  /* write_set and all_set are copies of read_set */
  def_write_set= def_read_set;
  s->all_set= def_read_set;
  bitmap_set_all(&this->s->all_set);
  default_column_bitmaps();
}



void Table::updateCreateInfo(HA_CREATE_INFO *create_info)
{
  create_info->max_rows= s->max_rows;
  create_info->min_rows= s->min_rows;
  create_info->table_options= s->db_create_options;
  create_info->avg_row_length= s->avg_row_length;
  create_info->block_size= s->block_size;
  create_info->row_type= s->row_type;
  create_info->default_table_charset= s->table_charset;
  create_info->table_charset= 0;
  create_info->comment= s->comment;

  return;
}

int
rename_file_ext(const char * from,const char * to,const char * ext)
{
  char from_b[FN_REFLEN],to_b[FN_REFLEN];
  strxmov(from_b,from,ext,NULL);
  strxmov(to_b,to,ext,NULL);
  return (my_rename(from_b,to_b,MYF(MY_WME)));
}


/*
  Allocate string field in MEM_ROOT and return it as String

  SYNOPSIS
    get_field()
    mem   	MEM_ROOT for allocating
    field 	Field for retrieving of string
    res         result String

  RETURN VALUES
    1   string is empty
    0	all ok
*/

bool get_field(MEM_ROOT *mem, Field *field, String *res)
{
  char buff[MAX_FIELD_WIDTH], *to;
  String str(buff,sizeof(buff),&my_charset_bin);
  uint32_t length;

  field->val_str(&str);
  if (!(length= str.length()))
  {
    res->length(0);
    return 1;
  }
  if (!(to= strmake_root(mem, str.ptr(), length)))
    length= 0;                                  // Safety fix
  res->set(to, length, ((Field_str*)field)->charset());
  return 0;
}


/*
  Allocate string field in MEM_ROOT and return it as NULL-terminated string

  SYNOPSIS
    get_field()
    mem   	MEM_ROOT for allocating
    field 	Field for retrieving of string

  RETURN VALUES
    NULL  string is empty
    #      pointer to NULL-terminated string value of field
*/

char *get_field(MEM_ROOT *mem, Field *field)
{
  char buff[MAX_FIELD_WIDTH], *to;
  String str(buff,sizeof(buff),&my_charset_bin);
  uint32_t length;

  field->val_str(&str);
  length= str.length();
  if (!length || !(to= (char*) alloc_root(mem,length+1)))
    return NULL;
  memcpy(to,str.ptr(),(uint) length);
  to[length]=0;
  return to;
}

/*
  DESCRIPTION
    given a buffer with a key value, and a map of keyparts
    that are present in this value, returns the length of the value
*/
uint32_t calculate_key_len(Table *table, uint32_t key,
                       const unsigned char *buf __attribute__((unused)),
                       key_part_map keypart_map)
{
  /* works only with key prefixes */
  assert(((keypart_map + 1) & keypart_map) == 0);

  KEY *key_info= table->s->key_info+key;
  KEY_PART_INFO *key_part= key_info->key_part;
  KEY_PART_INFO *end_key_part= key_part + key_info->key_parts;
  uint32_t length= 0;

  while (key_part < end_key_part && keypart_map)
  {
    length+= key_part->store_length;
    keypart_map >>= 1;
    key_part++;
  }
  return length;
}

/*
  Check if database name is valid

  SYNPOSIS
    check_db_name()
    org_name		Name of database and length

  NOTES
    If lower_case_table_names is set then database is converted to lower case

  RETURN
    0	ok
    1   error
*/

bool check_db_name(LEX_STRING *org_name)
{
  char *name= org_name->str;
  uint32_t name_length= org_name->length;

  if (!name_length || name_length > NAME_LEN || name[name_length - 1] == ' ')
    return 1;

  if (lower_case_table_names && name != any_db)
    my_casedn_str(files_charset_info, name);

  return check_identifier_name(org_name);
}


/*
  Allow anything as a table name, as long as it doesn't contain an
  ' ' at the end
  returns 1 on error
*/


bool check_table_name(const char *name, uint32_t length)
{
  if (!length || length > NAME_LEN || name[length - 1] == ' ')
    return 1;
  LEX_STRING ident;
  ident.str= (char*) name;
  ident.length= length;
  return check_identifier_name(&ident);
}


/*
  Eventually, a "length" argument should be added
  to this function, and the inner loop changed to
  check_identifier_name() call.
*/
bool check_column_name(const char *name)
{
  uint32_t name_length= 0;  // name length in symbols
  bool last_char_is_space= true;
  
  while (*name)
  {
#if defined(USE_MB) && defined(USE_MB_IDENT)
    last_char_is_space= my_isspace(system_charset_info, *name);
    if (use_mb(system_charset_info))
    {
      int len=my_ismbchar(system_charset_info, name, 
                          name+system_charset_info->mbmaxlen);
      if (len)
      {
        if (len > 3) /* Disallow non-BMP characters */
          return 1;
        name += len;
        name_length++;
        continue;
      }
    }
#else
    last_char_is_space= *name==' ';
#endif
    /*
      NAMES_SEP_CHAR is used in FRM format to separate SET and ENUM values.
      It is defined as 0xFF, which is a not valid byte in utf8.
      This assert is to catch use of this byte if we decide to
      use non-utf8 as system_character_set.
    */
    assert(*name != NAMES_SEP_CHAR);
    name++;
    name_length++;
  }
  /* Error if empty or too long column name */
  return last_char_is_space || (uint) name_length > NAME_CHAR_LEN;
}


/**
  Checks whether a table is intact. Should be done *just* after the table has
  been opened.

  @param[in] table             The table to check
  @param[in] table_f_count     Expected number of columns in the table
  @param[in] table_def         Expected structure of the table (column name
                               and type)

  @retval  false  OK
  @retval  TRUE   There was an error. An error message is output
                  to the error log.  We do not push an error
                  message into the error stack because this
                  function is currently only called at start up,
                  and such errors never reach the user.
*/

bool
Table::table_check_intact(const uint32_t table_f_count,
                          const TABLE_FIELD_W_TYPE *table_def)
{
  uint32_t i;
  bool error= false;
  bool fields_diff_count;

  fields_diff_count= (s->fields != table_f_count);
  if (fields_diff_count)
  {

    /* previous MySQL version */
    if (DRIZZLE_VERSION_ID > s->mysql_version)
    {
      sql_print_error(ER(ER_COL_COUNT_DOESNT_MATCH_PLEASE_UPDATE),
                      alias, table_f_count, s->fields,
                      s->mysql_version, DRIZZLE_VERSION_ID);
      return(true);
    }
    else if (DRIZZLE_VERSION_ID == s->mysql_version)
    {
      sql_print_error(ER(ER_COL_COUNT_DOESNT_MATCH_CORRUPTED), alias,
                      table_f_count, s->fields);
      return(true);
    }
    /*
      Something has definitely changed, but we're running an older
      version of MySQL with new system tables.
      Let's check column definitions. If a column was added at
      the end of the table, then we don't care much since such change
      is backward compatible.
    */
  }
  char buffer[STRING_BUFFER_USUAL_SIZE];
  for (i=0 ; i < table_f_count; i++, table_def++)
  {
    String sql_type(buffer, sizeof(buffer), system_charset_info);
    sql_type.length(0);
    if (i < s->fields)
    {
      Field *field= this->field[i];

      if (strncmp(field->field_name, table_def->name.str,
                  table_def->name.length))
      {
        /*
          Name changes are not fatal, we use ordinal numbers to access columns.
          Still this can be a sign of a tampered table, output an error
          to the error log.
        */
        sql_print_error(_("Incorrect definition of table %s.%s: "
                        "expected column '%s' at position %d, found '%s'."),
                        s->db.str, alias, table_def->name.str, i,
                        field->field_name);
      }
      field->sql_type(sql_type);
      /*
        Generally, if column types don't match, then something is
        wrong.

        However, we only compare column definitions up to the
        length of the original definition, since we consider the
        following definitions compatible:

        1. DATETIME and DATETIM
        2. INT(11) and INT(11
        3. SET('one', 'two') and SET('one', 'two', 'more')

        For SETs or ENUMs, if the same prefix is there it's OK to
        add more elements - they will get higher ordinal numbers and
        the new table definition is backward compatible with the
        original one.
       */
      if (strncmp(sql_type.c_ptr_safe(), table_def->type.str,
                  table_def->type.length - 1))
      {
        sql_print_error(_("Incorrect definition of table %s.%s: "
                        "expected column '%s' at position %d to have type "
                        "%s, found type %s."), s->db.str, alias,
                        table_def->name.str, i, table_def->type.str,
                        sql_type.c_ptr_safe());
        error= true;
      }
      else if (table_def->cset.str && !field->has_charset())
      {
        sql_print_error(_("Incorrect definition of table %s.%s: "
                        "expected the type of column '%s' at position %d "
                        "to have character set '%s' but the type has no "
                        "character set."), s->db.str, alias,
                        table_def->name.str, i, table_def->cset.str);
        error= true;
      }
      else if (table_def->cset.str &&
               strcmp(field->charset()->csname, table_def->cset.str))
      {
        sql_print_error(_("Incorrect definition of table %s.%s: "
                        "expected the type of column '%s' at position %d "
                        "to have character set '%s' but found "
                        "character set '%s'."), s->db.str, alias,
                        table_def->name.str, i, table_def->cset.str,
                        field->charset()->csname);
        error= true;
      }
    }
    else
    {
      sql_print_error(_("Incorrect definition of table %s.%s: "
                      "expected column '%s' at position %d to have type %s "
                      " but the column is not found."),
                      s->db.str, alias,
                      table_def->name.str, i, table_def->type.str);
      error= true;
    }
  }
  return(error);
}


/*
  Create Item_field for each column in the table.

  SYNPOSIS
    Table::fill_item_list()
      item_list          a pointer to an empty list used to store items

  DESCRIPTION
    Create Item_field object for each column in the table and
    initialize it with the corresponding Field. New items are
    created in the current Session memory root.

  RETURN VALUE
    0                    success
    1                    out of memory
*/

bool Table::fill_item_list(List<Item> *item_list) const
{
  /*
    All Item_field's created using a direct pointer to a field
    are fixed in Item_field constructor.
  */
  for (Field **ptr= field; *ptr; ptr++)
  {
    Item_field *item= new Item_field(*ptr);
    if (!item || item_list->push_back(item))
      return true;
  }
  return false;
}

/*
  Reset an existing list of Item_field items to point to the
  Fields of this table.

  SYNPOSIS
    Table::fill_item_list()
      item_list          a non-empty list with Item_fields

  DESCRIPTION
    This is a counterpart of fill_item_list used to redirect
    Item_fields to the fields of a newly created table.
    The caller must ensure that number of items in the item_list
    is the same as the number of columns in the table.
*/

void Table::reset_item_list(List<Item> *item_list) const
{
  List_iterator_fast<Item> it(*item_list);
  for (Field **ptr= field; *ptr; ptr++)
  {
    Item_field *item_field= (Item_field*) it++;
    assert(item_field != 0);
    item_field->reset_field(*ptr);
  }
}


/*
  Find underlying base tables (TableList) which represent given
  table_to_find (Table)

  SYNOPSIS
    TableList::find_underlying_table()
    table_to_find table to find

  RETURN
    0  table is not found
    found table reference
*/

TableList *TableList::find_underlying_table(Table *table_to_find)
{
  /* is this real table and table which we are looking for? */
  if (table == table_to_find && merge_underlying_list == 0)
    return this;

  for (TableList *tbl= merge_underlying_list; tbl; tbl= tbl->next_local)
  {
    TableList *result;
    if ((result= tbl->find_underlying_table(table_to_find)))
      return result;
  }
  return 0;
}

/*
  cleunup items belonged to view fields translation table

  SYNOPSIS
    TableList::cleanup_items()
*/

void TableList::cleanup_items()
{
  if (!field_translation)
    return;

  for (Field_translator *transl= field_translation;
       transl < field_translation_end;
       transl++)
    transl->item->walk(&Item::cleanup_processor, 0, 0);
}


/*
  Set insert_values buffer

  SYNOPSIS
    set_insert_values()
    mem_root   memory pool for allocating

  RETURN
    false - OK
    TRUE  - out of memory
*/

bool TableList::set_insert_values(MEM_ROOT *mem_root)
{
  if (table)
  {
    if (!table->insert_values &&
        !(table->insert_values= (unsigned char *)alloc_root(mem_root,
                                                   table->s->rec_buff_length)))
      return true;
  }

  return false;
}


/*
  Test if this is a leaf with respect to name resolution.

  SYNOPSIS
    TableList::is_leaf_for_name_resolution()

  DESCRIPTION
    A table reference is a leaf with respect to name resolution if
    it is either a leaf node in a nested join tree (table, view,
    schema table, subquery), or an inner node that represents a
    NATURAL/USING join, or a nested join with materialized join
    columns.

  RETURN
    TRUE if a leaf, false otherwise.
*/
bool TableList::is_leaf_for_name_resolution()
{
  return (is_natural_join || is_join_columns_complete || !nested_join);
}


/*
  Retrieve the first (left-most) leaf in a nested join tree with
  respect to name resolution.

  SYNOPSIS
    TableList::first_leaf_for_name_resolution()

  DESCRIPTION
    Given that 'this' is a nested table reference, recursively walk
    down the left-most children of 'this' until we reach a leaf
    table reference with respect to name resolution.

  IMPLEMENTATION
    The left-most child of a nested table reference is the last element
    in the list of children because the children are inserted in
    reverse order.

  RETURN
    If 'this' is a nested table reference - the left-most child of
      the tree rooted in 'this',
    else return 'this'
*/

TableList *TableList::first_leaf_for_name_resolution()
{
  TableList *cur_table_ref= NULL;
  nested_join_st *cur_nested_join;

  if (is_leaf_for_name_resolution())
    return this;
  assert(nested_join);

  for (cur_nested_join= nested_join;
       cur_nested_join;
       cur_nested_join= cur_table_ref->nested_join)
  {
    List_iterator_fast<TableList> it(cur_nested_join->join_list);
    cur_table_ref= it++;
    /*
      If the current nested join is a RIGHT JOIN, the operands in
      'join_list' are in reverse order, thus the first operand is
      already at the front of the list. Otherwise the first operand
      is in the end of the list of join operands.
    */
    if (!(cur_table_ref->outer_join & JOIN_TYPE_RIGHT))
    {
      TableList *next;
      while ((next= it++))
        cur_table_ref= next;
    }
    if (cur_table_ref->is_leaf_for_name_resolution())
      break;
  }
  return cur_table_ref;
}


/*
  Retrieve the last (right-most) leaf in a nested join tree with
  respect to name resolution.

  SYNOPSIS
    TableList::last_leaf_for_name_resolution()

  DESCRIPTION
    Given that 'this' is a nested table reference, recursively walk
    down the right-most children of 'this' until we reach a leaf
    table reference with respect to name resolution.

  IMPLEMENTATION
    The right-most child of a nested table reference is the first
    element in the list of children because the children are inserted
    in reverse order.

  RETURN
    - If 'this' is a nested table reference - the right-most child of
      the tree rooted in 'this',
    - else - 'this'
*/

TableList *TableList::last_leaf_for_name_resolution()
{
  TableList *cur_table_ref= this;
  nested_join_st *cur_nested_join;

  if (is_leaf_for_name_resolution())
    return this;
  assert(nested_join);

  for (cur_nested_join= nested_join;
       cur_nested_join;
       cur_nested_join= cur_table_ref->nested_join)
  {
    cur_table_ref= cur_nested_join->join_list.head();
    /*
      If the current nested is a RIGHT JOIN, the operands in
      'join_list' are in reverse order, thus the last operand is in the
      end of the list.
    */
    if ((cur_table_ref->outer_join & JOIN_TYPE_RIGHT))
    {
      List_iterator_fast<TableList> it(cur_nested_join->join_list);
      TableList *next;
      cur_table_ref= it++;
      while ((next= it++))
        cur_table_ref= next;
    }
    if (cur_table_ref->is_leaf_for_name_resolution())
      break;
  }
  return cur_table_ref;
}


/*****************************************************************************
  Functions to handle column usage bitmaps (read_set, write_set etc...)
*****************************************************************************/

/* Reset all columns bitmaps */

void Table::clear_column_bitmaps()
{
  /*
    Reset column read/write usage. It's identical to:
    bitmap_clear_all(&table->def_read_set);
    bitmap_clear_all(&table->def_write_set);
  */
  memset(def_read_set.bitmap, 0, s->column_bitmap_size*2);
  column_bitmaps_set(&def_read_set, &def_write_set);
}


/*
  Tell handler we are going to call position() and rnd_pos() later.
  
  NOTES:
  This is needed for handlers that uses the primary key to find the
  row. In this case we have to extend the read bitmap with the primary
  key fields.
*/

void Table::prepare_for_position()
{

  if ((file->ha_table_flags() & HA_PRIMARY_KEY_IN_READ_INDEX) &&
      s->primary_key < MAX_KEY)
  {
    mark_columns_used_by_index_no_reset(s->primary_key, read_set);
    /* signal change */
    file->column_bitmaps_signal();
  }
  return;
}


/*
  Mark that only fields from one key is used

  NOTE:
    This changes the bitmap to use the tmp bitmap
    After this, you can't access any other columns in the table until
    bitmaps are reset, for example with Table::clear_column_bitmaps()
    or Table::restore_column_maps_after_mark_index()
*/

void Table::mark_columns_used_by_index(uint32_t index)
{
  MY_BITMAP *bitmap= &tmp_set;

  (void) file->extra(HA_EXTRA_KEYREAD);
  bitmap_clear_all(bitmap);
  mark_columns_used_by_index_no_reset(index, bitmap);
  column_bitmaps_set(bitmap, bitmap);
  return;
}


/*
  Restore to use normal column maps after key read

  NOTES
    This reverse the change done by mark_columns_used_by_index

  WARNING
    For this to work, one must have the normal table maps in place
    when calling mark_columns_used_by_index
*/

void Table::restore_column_maps_after_mark_index()
{

  key_read= 0;
  (void) file->extra(HA_EXTRA_NO_KEYREAD);
  default_column_bitmaps();
  file->column_bitmaps_signal();
  return;
}


/*
  mark columns used by key, but don't reset other fields
*/

void Table::mark_columns_used_by_index_no_reset(uint32_t index,
                                                   MY_BITMAP *bitmap)
{
  KEY_PART_INFO *key_part= key_info[index].key_part;
  KEY_PART_INFO *key_part_end= (key_part +
                                key_info[index].key_parts);
  for (;key_part != key_part_end; key_part++)
  {
    bitmap_set_bit(bitmap, key_part->fieldnr-1);
    if (key_part->field->vcol_info &&
        key_part->field->vcol_info->expr_item)
      key_part->field->vcol_info->
               expr_item->walk(&Item::register_field_in_bitmap, 
                               1, (unsigned char *) bitmap);
  }
}


/*
  Mark auto-increment fields as used fields in both read and write maps

  NOTES
    This is needed in insert & update as the auto-increment field is
    always set and sometimes read.
*/

void Table::mark_auto_increment_column()
{
  assert(found_next_number_field);
  /*
    We must set bit in read set as update_auto_increment() is using the
    store() to check overflow of auto_increment values
  */
  bitmap_set_bit(read_set, found_next_number_field->field_index);
  bitmap_set_bit(write_set, found_next_number_field->field_index);
  if (s->next_number_keypart)
    mark_columns_used_by_index_no_reset(s->next_number_index, read_set);
  file->column_bitmaps_signal();
}


/*
  Mark columns needed for doing an delete of a row

  DESCRIPTON
    Some table engines don't have a cursor on the retrieve rows
    so they need either to use the primary key or all columns to
    be able to delete a row.

    If the engine needs this, the function works as follows:
    - If primary key exits, mark the primary key columns to be read.
    - If not, mark all columns to be read

    If the engine has HA_REQUIRES_KEY_COLUMNS_FOR_DELETE, we will
    mark all key columns as 'to-be-read'. This allows the engine to
    loop over the given record to find all keys and doesn't have to
    retrieve the row again.
*/

void Table::mark_columns_needed_for_delete()
{
  if (file->ha_table_flags() & HA_REQUIRES_KEY_COLUMNS_FOR_DELETE)
  {
    Field **reg_field;
    for (reg_field= field ; *reg_field ; reg_field++)
    {
      if ((*reg_field)->flags & PART_KEY_FLAG)
        bitmap_set_bit(read_set, (*reg_field)->field_index);
    }
    file->column_bitmaps_signal();
  }
  if (file->ha_table_flags() & HA_PRIMARY_KEY_REQUIRED_FOR_DELETE ||
      (mysql_bin_log.is_open() && in_use && in_use->current_stmt_binlog_row_based))
  {
    /*
      If the handler has no cursor capabilites, or we have row-based
      replication active for the current statement, we have to read
      either the primary key, the hidden primary key or all columns to
      be able to do an delete
    */
    if (s->primary_key == MAX_KEY)
      file->use_hidden_primary_key();
    else
    {
      mark_columns_used_by_index_no_reset(s->primary_key, read_set);
      file->column_bitmaps_signal();
    }
  }
}


/*
  Mark columns needed for doing an update of a row

  DESCRIPTON
    Some engines needs to have all columns in an update (to be able to
    build a complete row). If this is the case, we mark all not
    updated columns to be read.

    If this is no the case, we do like in the delete case and mark
    if neeed, either the primary key column or all columns to be read.
    (see mark_columns_needed_for_delete() for details)

    If the engine has HA_REQUIRES_KEY_COLUMNS_FOR_DELETE, we will
    mark all USED key columns as 'to-be-read'. This allows the engine to
    loop over the given record to find all changed keys and doesn't have to
    retrieve the row again.
*/

void Table::mark_columns_needed_for_update()
{
  if (file->ha_table_flags() & HA_REQUIRES_KEY_COLUMNS_FOR_DELETE)
  {
    /* Mark all used key columns for read */
    Field **reg_field;
    for (reg_field= field ; *reg_field ; reg_field++)
    {
      /* Merge keys is all keys that had a column refered to in the query */
      if (merge_keys.is_overlapping((*reg_field)->part_of_key))
        bitmap_set_bit(read_set, (*reg_field)->field_index);
    }
    file->column_bitmaps_signal();
  }
  if ((file->ha_table_flags() & HA_PRIMARY_KEY_REQUIRED_FOR_DELETE) ||
      (mysql_bin_log.is_open() && in_use && in_use->current_stmt_binlog_row_based))
  {
    /*
      If the handler has no cursor capabilites, or we have row-based
      logging active for the current statement, we have to read either
      the primary key, the hidden primary key or all columns to be
      able to do an update
    */
    if (s->primary_key == MAX_KEY)
      file->use_hidden_primary_key();
    else
    {
      mark_columns_used_by_index_no_reset(s->primary_key, read_set);
      file->column_bitmaps_signal();
    }
  }
  /* Mark all virtual columns as writable */
  mark_virtual_columns();
  return;
}


/*
  Mark columns the handler needs for doing an insert

  For now, this is used to mark fields used by the trigger
  as changed.
*/

void Table::mark_columns_needed_for_insert()
{
  if (found_next_number_field)
    mark_auto_increment_column();
  /* Mark all virtual columns as writable */
  mark_virtual_columns();
}

/* 
  @brief Update the write and read table bitmap to allow
         using procedure save_in_field for all virtual columns
         in the table.

  @return       void

  @detail
    Each virtual field is set in the write column map.
    All fields that the virtual columns are based on are set in the
    read bitmap.
*/

void Table::mark_virtual_columns(void)
{
  Field **vfield_ptr, *tmp_vfield;
  bool bitmap_updated= false;

  for (vfield_ptr= vfield; *vfield_ptr; vfield_ptr++)
  {
    tmp_vfield= *vfield_ptr;
    assert(tmp_vfield->vcol_info && tmp_vfield->vcol_info->expr_item);
    tmp_vfield->vcol_info->expr_item->walk(&Item::register_field_in_read_map, 
                                           1, (unsigned char *) 0);
    bitmap_set_bit(read_set, tmp_vfield->field_index);
    bitmap_set_bit(write_set, tmp_vfield->field_index);
    bitmap_updated= true;
  }
  if (bitmap_updated)
    file->column_bitmaps_signal();
}


/*
  Cleanup this table for re-execution.

  SYNOPSIS
    TableList::reinit_before_use()
*/

void TableList::reinit_before_use(Session *session)
{
  /*
    Reset old pointers to TABLEs: they are not valid since the tables
    were closed in the end of previous prepare or execute call.
  */
  table= 0;
  /* Reset is_schema_table_processed value(needed for I_S tables */
  schema_table_state= NOT_PROCESSED;

  TableList *embedded; /* The table at the current level of nesting. */
  TableList *parent_embedding= this; /* The parent nested table reference. */
  do
  {
    embedded= parent_embedding;
    if (embedded->prep_on_expr)
      embedded->on_expr= embedded->prep_on_expr->copy_andor_structure(session);
    parent_embedding= embedded->embedding;
  }
  while (parent_embedding &&
         parent_embedding->nested_join->join_list.head() == embedded);
}

/*
  Return subselect that contains the FROM list this table is taken from

  SYNOPSIS
    TableList::containing_subselect()
 
  RETURN
    Subselect item for the subquery that contains the FROM list
    this table is taken from if there is any
    0 - otherwise

*/

Item_subselect *TableList::containing_subselect()
{    
  return (select_lex ? select_lex->master_unit()->item : 0);
}

/*
  Compiles the tagged hints list and fills up the bitmasks.

  SYNOPSIS
    process_index_hints()
      table         the Table to operate on.

  DESCRIPTION
    The parser collects the index hints for each table in a "tagged list" 
    (TableList::index_hints). Using the information in this tagged list
    this function sets the members Table::keys_in_use_for_query, 
    Table::keys_in_use_for_group_by, Table::keys_in_use_for_order_by,
    Table::force_index and Table::covering_keys.

    Current implementation of the runtime does not allow mixing FORCE INDEX
    and USE INDEX, so this is checked here. Then the FORCE INDEX list 
    (if non-empty) is appended to the USE INDEX list and a flag is set.

    Multiple hints of the same kind are processed so that each clause 
    is applied to what is computed in the previous clause.
    For example:
        USE INDEX (i1) USE INDEX (i2)
    is equivalent to
        USE INDEX (i1,i2)
    and means "consider only i1 and i2".
        
    Similarly
        USE INDEX () USE INDEX (i1)
    is equivalent to
        USE INDEX (i1)
    and means "consider only the index i1"

    It is OK to have the same index several times, e.g. "USE INDEX (i1,i1)" is
    not an error.
        
    Different kind of hints (USE/FORCE/IGNORE) are processed in the following
    order:
      1. All indexes in USE (or FORCE) INDEX are added to the mask.
      2. All IGNORE INDEX

    e.g. "USE INDEX i1, IGNORE INDEX i1, USE INDEX i1" will not use i1 at all
    as if we had "USE INDEX i1, USE INDEX i1, IGNORE INDEX i1".

    As an optimization if there is a covering index, and we have 
    IGNORE INDEX FOR GROUP/order_st, and this index is used for the JOIN part, 
    then we have to ignore the IGNORE INDEX FROM GROUP/order_st.

  RETURN VALUE
    false                no errors found
    TRUE                 found and reported an error.
*/
bool TableList::process_index_hints(Table *tbl)
{
  /* initialize the result variables */
  tbl->keys_in_use_for_query= tbl->keys_in_use_for_group_by= 
    tbl->keys_in_use_for_order_by= tbl->s->keys_in_use;

  /* index hint list processing */
  if (index_hints)
  {
    key_map index_join[INDEX_HINT_FORCE + 1];
    key_map index_order[INDEX_HINT_FORCE + 1];
    key_map index_group[INDEX_HINT_FORCE + 1];
    Index_hint *hint;
    int type;
    bool have_empty_use_join= false, have_empty_use_order= false, 
         have_empty_use_group= false;
    List_iterator <Index_hint> iter(*index_hints);

    /* initialize temporary variables used to collect hints of each kind */
    for (type= INDEX_HINT_IGNORE; type <= INDEX_HINT_FORCE; type++)
    {
      index_join[type].clear_all();
      index_order[type].clear_all();
      index_group[type].clear_all();
    }

    /* iterate over the hints list */
    while ((hint= iter++))
    {
      uint32_t pos;

      /* process empty USE INDEX () */
      if (hint->type == INDEX_HINT_USE && !hint->key_name.str)
      {
        if (hint->clause & INDEX_HINT_MASK_JOIN)
        {
          index_join[hint->type].clear_all();
          have_empty_use_join= true;
        }
        if (hint->clause & INDEX_HINT_MASK_ORDER)
        {
          index_order[hint->type].clear_all();
          have_empty_use_order= true;
        }
        if (hint->clause & INDEX_HINT_MASK_GROUP)
        {
          index_group[hint->type].clear_all();
          have_empty_use_group= true;
        }
        continue;
      }

      /* 
        Check if an index with the given name exists and get his offset in 
        the keys bitmask for the table 
      */
      if (tbl->s->keynames.type_names == 0 ||
          (pos= find_type(&tbl->s->keynames, hint->key_name.str,
                          hint->key_name.length, 1)) <= 0)
      {
        my_error(ER_KEY_DOES_NOT_EXITS, MYF(0), hint->key_name.str, alias);
        return 1;
      }

      pos--;

      /* add to the appropriate clause mask */
      if (hint->clause & INDEX_HINT_MASK_JOIN)
        index_join[hint->type].set_bit (pos);
      if (hint->clause & INDEX_HINT_MASK_ORDER)
        index_order[hint->type].set_bit (pos);
      if (hint->clause & INDEX_HINT_MASK_GROUP)
        index_group[hint->type].set_bit (pos);
    }

    /* cannot mix USE INDEX and FORCE INDEX */
    if ((!index_join[INDEX_HINT_FORCE].is_clear_all() ||
         !index_order[INDEX_HINT_FORCE].is_clear_all() ||
         !index_group[INDEX_HINT_FORCE].is_clear_all()) &&
        (!index_join[INDEX_HINT_USE].is_clear_all() ||  have_empty_use_join ||
         !index_order[INDEX_HINT_USE].is_clear_all() || have_empty_use_order ||
         !index_group[INDEX_HINT_USE].is_clear_all() || have_empty_use_group))
    {
      my_error(ER_WRONG_USAGE, MYF(0), index_hint_type_name[INDEX_HINT_USE],
               index_hint_type_name[INDEX_HINT_FORCE]);
      return 1;
    }

    /* process FORCE INDEX as USE INDEX with a flag */
    if (!index_join[INDEX_HINT_FORCE].is_clear_all() ||
        !index_order[INDEX_HINT_FORCE].is_clear_all() ||
        !index_group[INDEX_HINT_FORCE].is_clear_all())
    {
      tbl->force_index= true;
      index_join[INDEX_HINT_USE].merge(index_join[INDEX_HINT_FORCE]);
      index_order[INDEX_HINT_USE].merge(index_order[INDEX_HINT_FORCE]);
      index_group[INDEX_HINT_USE].merge(index_group[INDEX_HINT_FORCE]);
    }

    /* apply USE INDEX */
    if (!index_join[INDEX_HINT_USE].is_clear_all() || have_empty_use_join)
      tbl->keys_in_use_for_query.intersect(index_join[INDEX_HINT_USE]);
    if (!index_order[INDEX_HINT_USE].is_clear_all() || have_empty_use_order)
      tbl->keys_in_use_for_order_by.intersect (index_order[INDEX_HINT_USE]);
    if (!index_group[INDEX_HINT_USE].is_clear_all() || have_empty_use_group)
      tbl->keys_in_use_for_group_by.intersect (index_group[INDEX_HINT_USE]);

    /* apply IGNORE INDEX */
    tbl->keys_in_use_for_query.subtract (index_join[INDEX_HINT_IGNORE]);
    tbl->keys_in_use_for_order_by.subtract (index_order[INDEX_HINT_IGNORE]);
    tbl->keys_in_use_for_group_by.subtract (index_group[INDEX_HINT_IGNORE]);
  }

  /* make sure covering_keys don't include indexes disabled with a hint */
  tbl->covering_keys.intersect(tbl->keys_in_use_for_query);
  return 0;
}


size_t Table::max_row_length(const unsigned char *data)
{
  size_t length= getRecordLength() + 2 * sizeFields();
  uint32_t *const beg= getBlobField();
  uint32_t *const end= beg + sizeBlobFields();

  for (uint32_t *ptr= beg ; ptr != end ; ++ptr)
  {
    Field_blob* const blob= (Field_blob*) field[*ptr];
    length+= blob->get_length((const unsigned char*)
                              (data + blob->offset(record[0]))) +
      HA_KEY_BLOB_LENGTH;
  }
  return length;
}

/*
  Check type of .frm if we are not going to parse it

  SYNOPSIS
  mysql_frm_type()
  path        path to file

  RETURN
  false       error
  true       table
*/

bool mysql_frm_type(Session *session __attribute__((unused)),
                    char *path, enum legacy_db_type *dbt)
{
  File file;
  unsigned char header[10];     /* This should be optimized */
  int error;

  *dbt= DB_TYPE_UNKNOWN;

  if ((file= open(path, O_RDONLY)) < 0)
    return false;
  error= my_read(file, (unsigned char*) header, sizeof(header), MYF(MY_NABP));
  my_close(file, MYF(MY_WME));

  if (error)
    return false;

  /*  
    This is just a check for DB_TYPE. We'll return default unknown type
    if the following test is true (arg #3). This should not have effect
    on return value from this function (default FRMTYPE_TABLE)
   */  
  if (header[0] != (unsigned char) 254 || header[1] != 1 ||
      (header[2] != FRM_VER && header[2] != FRM_VER+1 &&
       (header[2] < FRM_VER+3 || header[2] > FRM_VER+4)))
    return true;

  *dbt= (enum legacy_db_type) (uint) *(header + 3);
  return true;                   // Is probably a .frm table
}

/****************************************************************************
 Functions for creating temporary tables.
****************************************************************************/


/* Prototypes */
void free_tmp_table(Session *session, Table *entry);

/**
  Create field for temporary table from given field.

  @param session	       Thread handler
  @param org_field    field from which new field will be created
  @param name         New field name
  @param table	       Temporary table
  @param item	       !=NULL if item->result_field should point to new field.
                      This is relevant for how fill_record() is going to work:
                      If item != NULL then fill_record() will update
                      the record in the original table.
                      If item == NULL then fill_record() will update
                      the temporary table
  @param convert_blob_length   If >0 create a varstring(convert_blob_length)
                               field instead of blob.

  @retval
    NULL		on error
  @retval
    new_created field
*/

Field *create_tmp_field_from_field(Session *session, Field *org_field,
                                   const char *name, Table *table,
                                   Item_field *item, uint32_t convert_blob_length)
{
  Field *new_field;

  /* 
    Make sure that the blob fits into a Field_varstring which has 
    2-byte lenght. 
  */
  if (convert_blob_length && convert_blob_length <= Field_varstring::MAX_SIZE &&
      (org_field->flags & BLOB_FLAG))
    new_field= new Field_varstring(convert_blob_length,
                                   org_field->maybe_null(),
                                   org_field->field_name, table->s,
                                   org_field->charset());
  else
    new_field= org_field->new_field(session->mem_root, table,
                                    table == org_field->table);
  if (new_field)
  {
    new_field->init(table);
    new_field->orig_table= org_field->orig_table;
    if (item)
      item->result_field= new_field;
    else
      new_field->field_name= name;
    new_field->flags|= (org_field->flags & NO_DEFAULT_VALUE_FLAG);
    if (org_field->maybe_null() || (item && item->maybe_null))
      new_field->flags&= ~NOT_NULL_FLAG;	// Because of outer join
    if (org_field->type() == DRIZZLE_TYPE_VARCHAR)
      table->s->db_create_options|= HA_OPTION_PACK_RECORD;
    else if (org_field->type() == DRIZZLE_TYPE_DOUBLE)
      ((Field_double *) new_field)->not_fixed= true;
  }
  return new_field;
}

/**
  Create field for temporary table using type of given item.

  @param session                   Thread handler
  @param item                  Item to create a field for
  @param table                 Temporary table
  @param copy_func             If set and item is a function, store copy of
                               item in this array
  @param modify_item           1 if item->result_field should point to new
                               item. This is relevent for how fill_record()
                               is going to work:
                               If modify_item is 1 then fill_record() will
                               update the record in the original table.
                               If modify_item is 0 then fill_record() will
                               update the temporary table
  @param convert_blob_length   If >0 create a varstring(convert_blob_length)
                               field instead of blob.

  @retval
    0  on error
  @retval
    new_created field
*/

static Field *create_tmp_field_from_item(Session *session __attribute__((unused)),
                                         Item *item, Table *table,
                                         Item ***copy_func, bool modify_item,
                                         uint32_t convert_blob_length)
{
  bool maybe_null= item->maybe_null;
  Field *new_field;

  switch (item->result_type()) {
  case REAL_RESULT:
    new_field= new Field_double(item->max_length, maybe_null,
                                item->name, item->decimals, true);
    break;
  case INT_RESULT:
    /* 
      Select an integer type with the minimal fit precision.
      MY_INT32_NUM_DECIMAL_DIGITS is sign inclusive, don't consider the sign.
      Values with MY_INT32_NUM_DECIMAL_DIGITS digits may or may not fit into 
      Field_long : make them Field_int64_t.  
    */
    if (item->max_length >= (MY_INT32_NUM_DECIMAL_DIGITS - 1))
      new_field=new Field_int64_t(item->max_length, maybe_null,
                                   item->name, item->unsigned_flag);
    else
      new_field=new Field_long(item->max_length, maybe_null,
                               item->name, item->unsigned_flag);
    break;
  case STRING_RESULT:
    assert(item->collation.collation);
  
    enum enum_field_types type;
    /*
      DATE/TIME fields have STRING_RESULT result type. 
      To preserve type they needed to be handled separately.
    */
    if ((type= item->field_type()) == DRIZZLE_TYPE_DATETIME ||
        type == DRIZZLE_TYPE_TIME || type == DRIZZLE_TYPE_NEWDATE ||
        type == DRIZZLE_TYPE_TIMESTAMP)
      new_field= item->tmp_table_field_from_field_type(table, 1);
    /* 
      Make sure that the blob fits into a Field_varstring which has 
      2-byte lenght. 
    */
    else if (item->max_length/item->collation.collation->mbmaxlen > 255 &&
             convert_blob_length <= Field_varstring::MAX_SIZE && 
             convert_blob_length)
      new_field= new Field_varstring(convert_blob_length, maybe_null,
                                     item->name, table->s,
                                     item->collation.collation);
    else
      new_field= item->make_string_field(table);
    new_field->set_derivation(item->collation.derivation);
    break;
  case DECIMAL_RESULT:
  {
    uint8_t dec= item->decimals;
    uint8_t intg= ((Item_decimal *) item)->decimal_precision() - dec;
    uint32_t len= item->max_length;

    /*
      Trying to put too many digits overall in a DECIMAL(prec,dec)
      will always throw a warning. We must limit dec to
      DECIMAL_MAX_SCALE however to prevent an assert() later.
    */

    if (dec > 0)
    {
      signed int overflow;

      dec= cmin(dec, (uint8_t)DECIMAL_MAX_SCALE);

      /*
        If the value still overflows the field with the corrected dec,
        we'll throw out decimals rather than integers. This is still
        bad and of course throws a truncation warning.
        +1: for decimal point
      */

      overflow= my_decimal_precision_to_length(intg + dec, dec,
                                               item->unsigned_flag) - len;

      if (overflow > 0)
        dec= cmax(0, dec - overflow);            // too long, discard fract
      else
        len -= item->decimals - dec;            // corrected value fits
    }

    new_field= new Field_new_decimal(len, maybe_null, item->name,
                                     dec, item->unsigned_flag);
    break;
  }
  case ROW_RESULT:
  default:
    // This case should never be choosen
    assert(0);
    new_field= 0;
    break;
  }
  if (new_field)
    new_field->init(table);
    
  if (copy_func && item->is_result_field())
    *((*copy_func)++) = item;			// Save for copy_funcs
  if (modify_item)
    item->set_result_field(new_field);
  if (item->type() == Item::NULL_ITEM)
    new_field->is_created_from_null_item= true;
  return new_field;
}


/**
  Create field for information schema table.

  @param session		Thread handler
  @param table		Temporary table
  @param item		Item to create a field for

  @retval
    0			on error
  @retval
    new_created field
*/

Field *create_tmp_field_for_schema(Session *session __attribute__((unused)),
                                   Item *item, Table *table)
{
  if (item->field_type() == DRIZZLE_TYPE_VARCHAR)
  {
    Field *field;
    if (item->max_length > MAX_FIELD_VARCHARLENGTH)
      field= new Field_blob(item->max_length, item->maybe_null,
                            item->name, item->collation.collation);
    else
      field= new Field_varstring(item->max_length, item->maybe_null,
                                 item->name,
                                 table->s, item->collation.collation);
    if (field)
      field->init(table);
    return field;
  }
  return item->tmp_table_field_from_field_type(table, 0);
}


/**
  Create field for temporary table.

  @param session		Thread handler
  @param table		Temporary table
  @param item		Item to create a field for
  @param type		Type of item (normally item->type)
  @param copy_func	If set and item is a function, store copy of item
                       in this array
  @param from_field    if field will be created using other field as example,
                       pointer example field will be written here
  @param default_field	If field has a default value field, store it here
  @param group		1 if we are going to do a relative group by on result
  @param modify_item	1 if item->result_field should point to new item.
                       This is relevent for how fill_record() is going to
                       work:
                       If modify_item is 1 then fill_record() will update
                       the record in the original table.
                       If modify_item is 0 then fill_record() will update
                       the temporary table
  @param convert_blob_length If >0 create a varstring(convert_blob_length)
                             field instead of blob.

  @retval
    0			on error
  @retval
    new_created field
*/

Field *create_tmp_field(Session *session, Table *table,Item *item, Item::Type type,
                        Item ***copy_func, Field **from_field,
                        Field **default_field,
                        bool group, bool modify_item,
                        bool table_cant_handle_bit_fields __attribute__((unused)),
                        bool make_copy_field,
                        uint32_t convert_blob_length)
{
  Field *result;
  Item::Type orig_type= type;
  Item *orig_item= 0;

  if (type != Item::FIELD_ITEM &&
      item->real_item()->type() == Item::FIELD_ITEM)
  {
    orig_item= item;
    item= item->real_item();
    type= Item::FIELD_ITEM;
  }

  switch (type) {
  case Item::SUM_FUNC_ITEM:
  {
    Item_sum *item_sum=(Item_sum*) item;
    result= item_sum->create_tmp_field(group, table, convert_blob_length);
    if (!result)
      my_error(ER_OUT_OF_RESOURCES, MYF(ME_FATALERROR));
    return result;
  }
  case Item::FIELD_ITEM:
  case Item::DEFAULT_VALUE_ITEM:
  {
    Item_field *field= (Item_field*) item;
    bool orig_modify= modify_item;
    if (orig_type == Item::REF_ITEM)
      modify_item= 0;
    /*
      If item have to be able to store NULLs but underlaid field can't do it,
      create_tmp_field_from_field() can't be used for tmp field creation.
    */
    if (field->maybe_null && !field->field->maybe_null())
    {
      result= create_tmp_field_from_item(session, item, table, NULL,
                                         modify_item, convert_blob_length);
      *from_field= field->field;
      if (result && modify_item)
        field->result_field= result;
    } 
    else
      result= create_tmp_field_from_field(session, (*from_field= field->field),
                                          orig_item ? orig_item->name :
                                          item->name,
                                          table,
                                          modify_item ? field :
                                          NULL,
                                          convert_blob_length);
    if (orig_type == Item::REF_ITEM && orig_modify)
      ((Item_ref*)orig_item)->set_result_field(result);
    if (field->field->eq_def(result))
      *default_field= field->field;
    return result;
  }
  /* Fall through */
  case Item::FUNC_ITEM:
    /* Fall through */
  case Item::COND_ITEM:
  case Item::FIELD_AVG_ITEM:
  case Item::FIELD_STD_ITEM:
  case Item::SUBSELECT_ITEM:
    /* The following can only happen with 'CREATE TABLE ... SELECT' */
  case Item::PROC_ITEM:
  case Item::INT_ITEM:
  case Item::REAL_ITEM:
  case Item::DECIMAL_ITEM:
  case Item::STRING_ITEM:
  case Item::REF_ITEM:
  case Item::NULL_ITEM:
  case Item::VARBIN_ITEM:
    if (make_copy_field)
    {
      assert(((Item_result_field*)item)->result_field);
      *from_field= ((Item_result_field*)item)->result_field;
    }
    return create_tmp_field_from_item(session, item, table,
                                      (make_copy_field ? 0 : copy_func),
                                       modify_item, convert_blob_length);
  case Item::TYPE_HOLDER:  
    result= ((Item_type_holder *)item)->make_field_by_type(table);
    result->set_derivation(item->collation.derivation);
    return result;
  default:					// Dosen't have to be stored
    return 0;
  }
}

/**
  Create a temp table according to a field list.

  Given field pointers are changed to point at tmp_table for
  send_fields. The table object is self contained: it's
  allocated in its own memory root, as well as Field objects
  created for table columns.
  This function will replace Item_sum items in 'fields' list with
  corresponding Item_field items, pointing at the fields in the
  temporary table, unless this was prohibited by true
  value of argument save_sum_fields. The Item_field objects
  are created in Session memory root.

  @param session                  thread handle
  @param param                a description used as input to create the table
  @param fields               list of items that will be used to define
                              column types of the table (also see NOTES)
  @param group                TODO document
  @param distinct             should table rows be distinct
  @param save_sum_fields      see NOTES
  @param select_options
  @param rows_limit
  @param table_alias          possible name of the temporary table that can
                              be used for name resolving; can be "".
*/

#define STRING_TOTAL_LENGTH_TO_PACK_ROWS 128
#define AVG_STRING_LENGTH_TO_PACK_ROWS   64
#define RATIO_TO_PACK_ROWS	       2

Table *
create_tmp_table(Session *session,TMP_TABLE_PARAM *param,List<Item> &fields,
		 order_st *group, bool distinct, bool save_sum_fields,
		 uint64_t select_options, ha_rows rows_limit,
		 char *table_alias)
{
  MEM_ROOT *mem_root_save, own_root;
  Table *table;
  TABLE_SHARE *share;
  uint	i,field_count,null_count,null_pack_length;
  uint32_t  copy_func_count= param->func_count;
  uint32_t  hidden_null_count, hidden_null_pack_length, hidden_field_count;
  uint32_t  blob_count,group_null_items, string_count;
  uint32_t  temp_pool_slot=MY_BIT_NONE;
  uint32_t fieldnr= 0;
  ulong reclength, string_total_length;
  bool  using_unique_constraint= 0;
  bool  use_packed_rows= 0;
  bool  not_all_columns= !(select_options & TMP_TABLE_ALL_COLUMNS);
  char  *tmpname,path[FN_REFLEN];
  unsigned char	*pos, *group_buff, *bitmaps;
  unsigned char *null_flags;
  Field **reg_field, **from_field, **default_field;
  uint32_t *blob_field;
  Copy_field *copy=0;
  KEY *keyinfo;
  KEY_PART_INFO *key_part_info;
  Item **copy_func;
  MI_COLUMNDEF *recinfo;
  uint32_t total_uneven_bit_length= 0;
  bool force_copy_fields= param->force_copy_fields;

  status_var_increment(session->status_var.created_tmp_tables);

  if (use_temp_pool && !(test_flags & TEST_KEEP_TMP_TABLES))
    temp_pool_slot = bitmap_lock_set_next(&temp_pool);

  if (temp_pool_slot != MY_BIT_NONE) // we got a slot
    sprintf(path, "%s_%lx_%i", tmp_file_prefix,
            current_pid, temp_pool_slot);
  else
  {
    /* if we run out of slots or we are not using tempool */
    sprintf(path,"%s%lx_%"PRIx64"_%x", tmp_file_prefix,current_pid,
            session->thread_id, session->tmp_table++);
  }

  /*
    No need to change table name to lower case as we are only creating
    MyISAM or HEAP tables here
  */
  fn_format(path, path, mysql_tmpdir, "", MY_REPLACE_EXT|MY_UNPACK_FILENAME);


  if (group)
  {
    if (!param->quick_group)
      group=0;					// Can't use group key
    else for (order_st *tmp=group ; tmp ; tmp=tmp->next)
    {
      /*
        marker == 4 means two things:
        - store NULLs in the key, and
        - convert BIT fields to 64-bit long, needed because MEMORY tables
          can't index BIT fields.
      */
      (*tmp->item)->marker= 4;
      if ((*tmp->item)->max_length >= CONVERT_IF_BIGGER_TO_BLOB)
	using_unique_constraint=1;
    }
    if (param->group_length >= MAX_BLOB_WIDTH)
      using_unique_constraint=1;
    if (group)
      distinct=0;				// Can't use distinct
  }

  field_count=param->field_count+param->func_count+param->sum_func_count;
  hidden_field_count=param->hidden_field_count;

  /*
    When loose index scan is employed as access method, it already
    computes all groups and the result of all aggregate functions. We
    make space for the items of the aggregate function in the list of
    functions TMP_TABLE_PARAM::items_to_copy, so that the values of
    these items are stored in the temporary table.
  */
  if (param->precomputed_group_by)
    copy_func_count+= param->sum_func_count;
  
  init_sql_alloc(&own_root, TABLE_ALLOC_BLOCK_SIZE, 0);

  if (!multi_alloc_root(&own_root,
                        &table, sizeof(*table),
                        &share, sizeof(*share),
                        &reg_field, sizeof(Field*) * (field_count+1),
                        &default_field, sizeof(Field*) * (field_count),
                        &blob_field, sizeof(uint)*(field_count+1),
                        &from_field, sizeof(Field*)*field_count,
                        &copy_func, sizeof(*copy_func)*(copy_func_count+1),
                        &param->keyinfo, sizeof(*param->keyinfo),
                        &key_part_info,
                        sizeof(*key_part_info)*(param->group_parts+1),
                        &param->start_recinfo,
                        sizeof(*param->recinfo)*(field_count*2+4),
                        &tmpname, (uint) strlen(path)+1,
                        &group_buff, (group && ! using_unique_constraint ?
                                      param->group_length : 0),
                        &bitmaps, bitmap_buffer_size(field_count)*2,
                        NULL))
  {
    if (temp_pool_slot != MY_BIT_NONE)
      bitmap_lock_clear_bit(&temp_pool, temp_pool_slot);
    return(NULL);				/* purecov: inspected */
  }
  /* Copy_field belongs to TMP_TABLE_PARAM, allocate it in Session mem_root */
  if (!(param->copy_field= copy= new (session->mem_root) Copy_field[field_count]))
  {
    if (temp_pool_slot != MY_BIT_NONE)
      bitmap_lock_clear_bit(&temp_pool, temp_pool_slot);
    free_root(&own_root, MYF(0));               /* purecov: inspected */
    return(NULL);				/* purecov: inspected */
  }
  param->items_to_copy= copy_func;
  my_stpcpy(tmpname,path);
  /* make table according to fields */

  memset(table, 0, sizeof(*table));
  memset(reg_field, 0, sizeof(Field*)*(field_count+1));
  memset(default_field, 0, sizeof(Field*) * (field_count));
  memset(from_field, 0, sizeof(Field*)*field_count);

  table->mem_root= own_root;
  mem_root_save= session->mem_root;
  session->mem_root= &table->mem_root;

  table->field=reg_field;
  table->alias= table_alias;
  table->reginfo.lock_type=TL_WRITE;	/* Will be updated */
  table->db_stat=HA_OPEN_KEYFILE+HA_OPEN_RNDFILE;
  table->map=1;
  table->temp_pool_slot = temp_pool_slot;
  table->copy_blobs= 1;
  table->in_use= session;
  table->quick_keys.init();
  table->covering_keys.init();
  table->keys_in_use_for_query.init();

  table->setShare(share);
  init_tmp_table_share(session, share, "", 0, tmpname, tmpname);
  share->blob_field= blob_field;
  share->blob_ptr_size= portable_sizeof_char_ptr;
  share->db_low_byte_first=1;                // True for HEAP and MyISAM
  share->table_charset= param->table_charset;
  share->primary_key= MAX_KEY;               // Indicate no primary key
  share->keys_for_keyread.init();
  share->keys_in_use.init();

  /* Calculate which type of fields we will store in the temporary table */

  reclength= string_total_length= 0;
  blob_count= string_count= null_count= hidden_null_count= group_null_items= 0;
  param->using_indirect_summary_function=0;

  List_iterator_fast<Item> li(fields);
  Item *item;
  Field **tmp_from_field=from_field;
  while ((item=li++))
  {
    Item::Type type=item->type();
    if (not_all_columns)
    {
      if (item->with_sum_func && type != Item::SUM_FUNC_ITEM)
      {
        if (item->used_tables() & OUTER_REF_TABLE_BIT)
          item->update_used_tables();
        if (type == Item::SUBSELECT_ITEM ||
            (item->used_tables() & ~OUTER_REF_TABLE_BIT))
        {
	  /*
	    Mark that the we have ignored an item that refers to a summary
	    function. We need to know this if someone is going to use
	    DISTINCT on the result.
	  */
	  param->using_indirect_summary_function=1;
	  continue;
        }
      }
      if (item->const_item() && (int) hidden_field_count <= 0)
        continue; // We don't have to store this
    }
    if (type == Item::SUM_FUNC_ITEM && !group && !save_sum_fields)
    {						/* Can't calc group yet */
      ((Item_sum*) item)->result_field=0;
      for (i=0 ; i < ((Item_sum*) item)->arg_count ; i++)
      {
	Item **argp= ((Item_sum*) item)->args + i;
	Item *arg= *argp;
	if (!arg->const_item())
	{
	  Field *new_field=
            create_tmp_field(session, table, arg, arg->type(), &copy_func,
                             tmp_from_field, &default_field[fieldnr],
                             group != 0,not_all_columns,
                             distinct, 0,
                             param->convert_blob_length);
	  if (!new_field)
	    goto err;					// Should be OOM
	  tmp_from_field++;
	  reclength+=new_field->pack_length();
	  if (new_field->flags & BLOB_FLAG)
	  {
	    *blob_field++= fieldnr;
	    blob_count++;
	  }
	  *(reg_field++)= new_field;
          if (new_field->real_type() == DRIZZLE_TYPE_VARCHAR)
          {
            string_count++;
            string_total_length+= new_field->pack_length();
          }
          session->mem_root= mem_root_save;
          session->change_item_tree(argp, new Item_field(new_field));
          session->mem_root= &table->mem_root;
	  if (!(new_field->flags & NOT_NULL_FLAG))
          {
	    null_count++;
            /*
              new_field->maybe_null() is still false, it will be
              changed below. But we have to setup Item_field correctly
            */
            (*argp)->maybe_null=1;
          }
          new_field->field_index= fieldnr++;
	}
      }
    }
    else
    {
      /*
	The last parameter to create_tmp_field() is a bit tricky:

	We need to set it to 0 in union, to get fill_record() to modify the
	temporary table.
	We need to set it to 1 on multi-table-update and in select to
	write rows to the temporary table.
	We here distinguish between UNION and multi-table-updates by the fact
	that in the later case group is set to the row pointer.
      */
      Field *new_field= (param->schema_table) ?
        create_tmp_field_for_schema(session, item, table) :
        create_tmp_field(session, table, item, type, &copy_func,
                         tmp_from_field, &default_field[fieldnr],
                         group != 0,
                         !force_copy_fields &&
                           (not_all_columns || group !=0),
                         /*
                           If item->marker == 4 then we force create_tmp_field
                           to create a 64-bit longs for BIT fields because HEAP
                           tables can't index BIT fields directly. We do the same
                           for distinct, as we want the distinct index to be
                           usable in this case too.
                         */
                         item->marker == 4 || param->bit_fields_as_long,
                         force_copy_fields,
                         param->convert_blob_length);

      if (!new_field)
      {
	if (session->is_fatal_error)
	  goto err;				// Got OOM
	continue;				// Some kindf of const item
      }
      if (type == Item::SUM_FUNC_ITEM)
	((Item_sum *) item)->result_field= new_field;
      tmp_from_field++;
      reclength+=new_field->pack_length();
      if (!(new_field->flags & NOT_NULL_FLAG))
	null_count++;
      if (new_field->flags & BLOB_FLAG)
      {
        *blob_field++= fieldnr;
	blob_count++;
      }
      if (item->marker == 4 && item->maybe_null)
      {
	group_null_items++;
	new_field->flags|= GROUP_FLAG;
      }
      new_field->field_index= fieldnr++;
      *(reg_field++)= new_field;
    }
    if (!--hidden_field_count)
    {
      /*
        This was the last hidden field; Remember how many hidden fields could
        have null
      */
      hidden_null_count=null_count;
      /*
	We need to update hidden_field_count as we may have stored group
	functions with constant arguments
      */
      param->hidden_field_count= fieldnr;
      null_count= 0;
    }
  }
  assert(fieldnr == (uint) (reg_field - table->field));
  assert(field_count >= (uint) (reg_field - table->field));
  field_count= fieldnr;
  *reg_field= 0;
  *blob_field= 0;				// End marker
  share->fields= field_count;

  /* If result table is small; use a heap */
  /* future: storage engine selection can be made dynamic? */
  if (blob_count || using_unique_constraint ||
      (select_options & (OPTION_BIG_TABLES | SELECT_SMALL_RESULT)) ==
      OPTION_BIG_TABLES || (select_options & TMP_TABLE_FORCE_MYISAM))
  {
    share->db_plugin= ha_lock_engine(0, myisam_hton);
    table->file= get_new_handler(share, &table->mem_root,
                                 share->db_type());
    if (group &&
	(param->group_parts > table->file->max_key_parts() ||
	 param->group_length > table->file->max_key_length()))
      using_unique_constraint=1;
  }
  else
  {
    share->db_plugin= ha_lock_engine(0, heap_hton);
    table->file= get_new_handler(share, &table->mem_root,
                                 share->db_type());
  }
  if (!table->file)
    goto err;


  if (!using_unique_constraint)
    reclength+= group_null_items;	// null flag is stored separately

  share->blob_fields= blob_count;
  if (blob_count == 0)
  {
    /* We need to ensure that first byte is not 0 for the delete link */
    if (param->hidden_field_count)
      hidden_null_count++;
    else
      null_count++;
  }
  hidden_null_pack_length=(hidden_null_count+7)/8;
  null_pack_length= (hidden_null_pack_length +
                     (null_count + total_uneven_bit_length + 7) / 8);
  reclength+=null_pack_length;
  if (!reclength)
    reclength=1;				// Dummy select
  /* Use packed rows if there is blobs or a lot of space to gain */
  if (blob_count || ((string_total_length >= STRING_TOTAL_LENGTH_TO_PACK_ROWS) && (reclength / string_total_length <= RATIO_TO_PACK_ROWS || (string_total_length / string_count) >= AVG_STRING_LENGTH_TO_PACK_ROWS)))
    use_packed_rows= 1;

  share->reclength= reclength;
  {
    uint32_t alloc_length=ALIGN_SIZE(reclength+MI_UNIQUE_HASH_LENGTH+1);
    share->rec_buff_length= alloc_length;
    if (!(table->record[0]= (unsigned char*)
                            alloc_root(&table->mem_root, alloc_length*3)))
      goto err;
    table->record[1]= table->record[0]+alloc_length;
    share->default_values= table->record[1]+alloc_length;
  }
  copy_func[0]=0;				// End marker
  param->func_count= copy_func - param->items_to_copy; 

  table->setup_tmp_table_column_bitmaps(bitmaps);

  recinfo=param->start_recinfo;
  null_flags=(unsigned char*) table->record[0];
  pos=table->record[0]+ null_pack_length;
  if (null_pack_length)
  {
    memset(recinfo, 0, sizeof(*recinfo));
    recinfo->type=FIELD_NORMAL;
    recinfo->length=null_pack_length;
    recinfo++;
    memset(null_flags, 255, null_pack_length);	// Set null fields

    table->null_flags= (unsigned char*) table->record[0];
    share->null_fields= null_count+ hidden_null_count;
    share->null_bytes= null_pack_length;
  }
  null_count= (blob_count == 0) ? 1 : 0;
  hidden_field_count=param->hidden_field_count;
  for (i=0,reg_field=table->field; i < field_count; i++,reg_field++,recinfo++)
  {
    Field *field= *reg_field;
    uint32_t length;
    memset(recinfo, 0, sizeof(*recinfo));

    if (!(field->flags & NOT_NULL_FLAG))
    {
      if (field->flags & GROUP_FLAG && !using_unique_constraint)
      {
	/*
	  We have to reserve one byte here for NULL bits,
	  as this is updated by 'end_update()'
	*/
	*pos++=0;				// Null is stored here
	recinfo->length=1;
	recinfo->type=FIELD_NORMAL;
	recinfo++;
	memset(recinfo, 0, sizeof(*recinfo));
      }
      else
      {
	recinfo->null_bit= 1 << (null_count & 7);
	recinfo->null_pos= null_count/8;
      }
      field->move_field(pos,null_flags+null_count/8,
			1 << (null_count & 7));
      null_count++;
    }
    else
      field->move_field(pos,(unsigned char*) 0,0);
    field->reset();

    /*
      Test if there is a default field value. The test for ->ptr is to skip
      'offset' fields generated by initalize_tables
    */
    if (default_field[i] && default_field[i]->ptr)
    {
      /* 
         default_field[i] is set only in the cases  when 'field' can
         inherit the default value that is defined for the field referred
         by the Item_field object from which 'field' has been created.
      */
      my_ptrdiff_t diff;
      Field *orig_field= default_field[i];
      /* Get the value from default_values */
      diff= (my_ptrdiff_t) (orig_field->table->s->default_values-
                            orig_field->table->record[0]);
      orig_field->move_field_offset(diff);      // Points now at default_values
      if (orig_field->is_real_null())
        field->set_null();
      else
      {
        field->set_notnull();
        memcpy(field->ptr, orig_field->ptr, field->pack_length());
      }
      orig_field->move_field_offset(-diff);     // Back to record[0]
    } 

    if (from_field[i])
    {						/* Not a table Item */
      copy->set(field,from_field[i],save_sum_fields);
      copy++;
    }
    length=field->pack_length();
    pos+= length;

    /* Make entry for create table */
    recinfo->length=length;
    if (field->flags & BLOB_FLAG)
      recinfo->type= (int) FIELD_BLOB;
    else
      recinfo->type=FIELD_NORMAL;
    if (!--hidden_field_count)
      null_count=(null_count+7) & ~7;		// move to next byte

    // fix table name in field entry
    field->table_name= &table->alias;
  }

  param->copy_field_end=copy;
  param->recinfo=recinfo;
  store_record(table,s->default_values);        // Make empty default record

  if (session->variables.tmp_table_size == ~ (uint64_t) 0)		// No limit
    share->max_rows= ~(ha_rows) 0;
  else
    share->max_rows= (ha_rows) (((share->db_type() == heap_hton) ?
                                 cmin(session->variables.tmp_table_size,
                                     session->variables.max_heap_table_size) :
                                 session->variables.tmp_table_size) /
			         share->reclength);
  set_if_bigger(share->max_rows,1);		// For dummy start options
  /*
    Push the LIMIT clause to the temporary table creation, so that we
    materialize only up to 'rows_limit' records instead of all result records.
  */
  set_if_smaller(share->max_rows, rows_limit);
  param->end_write_records= rows_limit;

  keyinfo= param->keyinfo;

  if (group)
  {
    table->group=group;				/* Table is grouped by key */
    param->group_buff=group_buff;
    share->keys=1;
    share->uniques= test(using_unique_constraint);
    table->key_info=keyinfo;
    keyinfo->key_part=key_part_info;
    keyinfo->flags=HA_NOSAME;
    keyinfo->usable_key_parts=keyinfo->key_parts= param->group_parts;
    keyinfo->key_length=0;
    keyinfo->rec_per_key=0;
    keyinfo->algorithm= HA_KEY_ALG_UNDEF;
    keyinfo->name= (char*) "group_key";
    order_st *cur_group= group;
    for (; cur_group ; cur_group= cur_group->next, key_part_info++)
    {
      Field *field=(*cur_group->item)->get_tmp_table_field();
      bool maybe_null=(*cur_group->item)->maybe_null;
      key_part_info->null_bit=0;
      key_part_info->field=  field;
      key_part_info->offset= field->offset(table->record[0]);
      key_part_info->length= (uint16_t) field->key_length();
      key_part_info->type=   (uint8_t) field->key_type();
      key_part_info->key_type =
	((ha_base_keytype) key_part_info->type == HA_KEYTYPE_TEXT ||
	 (ha_base_keytype) key_part_info->type == HA_KEYTYPE_VARTEXT1 ||
	 (ha_base_keytype) key_part_info->type == HA_KEYTYPE_VARTEXT2) ?
	0 : FIELDFLAG_BINARY;
      if (!using_unique_constraint)
      {
	cur_group->buff=(char*) group_buff;
	if (!(cur_group->field= field->new_key_field(session->mem_root,table,
                                                     group_buff +
                                                     test(maybe_null),
                                                     field->null_ptr,
                                                     field->null_bit)))
	  goto err; /* purecov: inspected */
	if (maybe_null)
	{
	  /*
	    To be able to group on NULL, we reserved place in group_buff
	    for the NULL flag just before the column. (see above).
	    The field data is after this flag.
	    The NULL flag is updated in 'end_update()' and 'end_write()'
	  */
	  keyinfo->flags|= HA_NULL_ARE_EQUAL;	// def. that NULL == NULL
	  key_part_info->null_bit=field->null_bit;
	  key_part_info->null_offset= (uint) (field->null_ptr -
					      (unsigned char*) table->record[0]);
          cur_group->buff++;                        // Pointer to field data
	  group_buff++;                         // Skipp null flag
	}
        /* In GROUP BY 'a' and 'a ' are equal for VARCHAR fields */
        key_part_info->key_part_flag|= HA_END_SPACE_ARE_EQUAL;
	group_buff+= cur_group->field->pack_length();
      }
      keyinfo->key_length+=  key_part_info->length;
    }
  }

  if (distinct && field_count != param->hidden_field_count)
  {
    /*
      Create an unique key or an unique constraint over all columns
      that should be in the result.  In the temporary table, there are
      'param->hidden_field_count' extra columns, whose null bits are stored
      in the first 'hidden_null_pack_length' bytes of the row.
    */
    if (blob_count)
    {
      /*
        Special mode for index creation in MyISAM used to support unique
        indexes on blobs with arbitrary length. Such indexes cannot be
        used for lookups.
      */
      share->uniques= 1;
    }
    null_pack_length-=hidden_null_pack_length;
    keyinfo->key_parts= ((field_count-param->hidden_field_count)+
			 (share->uniques ? test(null_pack_length) : 0));
    table->distinct= 1;
    share->keys= 1;
    if (!(key_part_info= (KEY_PART_INFO*)
          alloc_root(&table->mem_root,
                     keyinfo->key_parts * sizeof(KEY_PART_INFO))))
      goto err;
    memset(key_part_info, 0, keyinfo->key_parts * sizeof(KEY_PART_INFO));
    table->key_info=keyinfo;
    keyinfo->key_part=key_part_info;
    keyinfo->flags=HA_NOSAME | HA_NULL_ARE_EQUAL;
    keyinfo->key_length=(uint16_t) reclength;
    keyinfo->name= (char*) "distinct_key";
    keyinfo->algorithm= HA_KEY_ALG_UNDEF;
    keyinfo->rec_per_key=0;

    /*
      Create an extra field to hold NULL bits so that unique indexes on
      blobs can distinguish NULL from 0. This extra field is not needed
      when we do not use UNIQUE indexes for blobs.
    */
    if (null_pack_length && share->uniques)
    {
      key_part_info->null_bit=0;
      key_part_info->offset=hidden_null_pack_length;
      key_part_info->length=null_pack_length;
      key_part_info->field= new Field_varstring(table->record[0],
                                                (uint32_t) key_part_info->length,
                                                0,
                                                (unsigned char*) 0,
                                                (uint) 0,
                                                Field::NONE,
                                                NULL, 
                                                table->s,
                                                &my_charset_bin);
      if (!key_part_info->field)
        goto err;
      key_part_info->field->init(table);
      key_part_info->key_type=FIELDFLAG_BINARY;
      key_part_info->type=    HA_KEYTYPE_BINARY;
      key_part_info++;
    }
    /* Create a distinct key over the columns we are going to return */
    for (i=param->hidden_field_count, reg_field=table->field + i ;
	 i < field_count;
	 i++, reg_field++, key_part_info++)
    {
      key_part_info->null_bit=0;
      key_part_info->field=    *reg_field;
      key_part_info->offset=   (*reg_field)->offset(table->record[0]);
      key_part_info->length=   (uint16_t) (*reg_field)->pack_length();
      /* TODO:
        The below method of computing the key format length of the
        key part is a copy/paste from opt_range.cc, and table.cc.
        This should be factored out, e.g. as a method of Field.
        In addition it is not clear if any of the Field::*_length
        methods is supposed to compute the same length. If so, it
        might be reused.
      */
      key_part_info->store_length= key_part_info->length;

      if ((*reg_field)->real_maybe_null())
        key_part_info->store_length+= HA_KEY_NULL_LENGTH;
      if ((*reg_field)->type() == DRIZZLE_TYPE_BLOB || 
          (*reg_field)->real_type() == DRIZZLE_TYPE_VARCHAR)
        key_part_info->store_length+= HA_KEY_BLOB_LENGTH;

      key_part_info->type=     (uint8_t) (*reg_field)->key_type();
      key_part_info->key_type =
	((ha_base_keytype) key_part_info->type == HA_KEYTYPE_TEXT ||
	 (ha_base_keytype) key_part_info->type == HA_KEYTYPE_VARTEXT1 ||
	 (ha_base_keytype) key_part_info->type == HA_KEYTYPE_VARTEXT2) ?
	0 : FIELDFLAG_BINARY;
    }
  }

  if (session->is_fatal_error)				// If end of memory
    goto err;					 /* purecov: inspected */
  share->db_record_offset= 1;
  if (share->db_type() == myisam_hton)
  {
    if (table->create_myisam_tmp_table(param->keyinfo, param->start_recinfo,
				       &param->recinfo, select_options))
      goto err;
  }
  if (table->open_tmp_table())
    goto err;

  session->mem_root= mem_root_save;

  return(table);

err:
  session->mem_root= mem_root_save;
  table->free_tmp_table(session);                    /* purecov: inspected */
  if (temp_pool_slot != MY_BIT_NONE)
    bitmap_lock_clear_bit(&temp_pool, temp_pool_slot);
  return(NULL);				/* purecov: inspected */
}

/****************************************************************************/

/**
  Create a reduced Table object with properly set up Field list from a
  list of field definitions.

    The created table doesn't have a table handler associated with
    it, has no keys, no group/distinct, no copy_funcs array.
    The sole purpose of this Table object is to use the power of Field
    class to read/write data to/from table->record[0]. Then one can store
    the record in any container (RB tree, hash, etc).
    The table is created in Session mem_root, so are the table's fields.
    Consequently, if you don't BLOB fields, you don't need to free it.

  @param session         connection handle
  @param field_list  list of column definitions

  @return
    0 if out of memory, Table object in case of success
*/

Table *create_virtual_tmp_table(Session *session, List<Create_field> &field_list)
{
  uint32_t field_count= field_list.elements;
  uint32_t blob_count= 0;
  Field **field;
  Create_field *cdef;                           /* column definition */
  uint32_t record_length= 0;
  uint32_t null_count= 0;                 /* number of columns which may be null */
  uint32_t null_pack_length;              /* NULL representation array length */
  uint32_t *blob_field;
  unsigned char *bitmaps;
  Table *table;
  TABLE_SHARE *share;

  if (!multi_alloc_root(session->mem_root,
                        &table, sizeof(*table),
                        &share, sizeof(*share),
                        &field, (field_count + 1) * sizeof(Field*),
                        &blob_field, (field_count+1) *sizeof(uint),
                        &bitmaps, bitmap_buffer_size(field_count)*2,
                        NULL))
    return 0;

  memset(table, 0, sizeof(*table));
  memset(share, 0, sizeof(*share));
  table->field= field;
  table->s= share;
  share->blob_field= blob_field;
  share->fields= field_count;
  share->blob_ptr_size= portable_sizeof_char_ptr;
  table->setup_tmp_table_column_bitmaps(bitmaps);

  /* Create all fields and calculate the total length of record */
  List_iterator_fast<Create_field> it(field_list);
  while ((cdef= it++))
  {
    *field= make_field(share, 0, cdef->length,
                       (unsigned char*) (f_maybe_null(cdef->pack_flag) ? "" : 0),
                       f_maybe_null(cdef->pack_flag) ? 1 : 0,
                       cdef->pack_flag, cdef->sql_type, cdef->charset,
                       cdef->unireg_check,
                       cdef->interval, cdef->field_name);
    if (!*field)
      goto error;
    (*field)->init(table);
    record_length+= (*field)->pack_length();
    if (! ((*field)->flags & NOT_NULL_FLAG))
      null_count++;

    if ((*field)->flags & BLOB_FLAG)
      share->blob_field[blob_count++]= (uint) (field - table->field);

    field++;
  }
  *field= NULL;                             /* mark the end of the list */
  share->blob_field[blob_count]= 0;            /* mark the end of the list */
  share->blob_fields= blob_count;

  null_pack_length= (null_count + 7)/8;
  share->reclength= record_length + null_pack_length;
  share->rec_buff_length= ALIGN_SIZE(share->reclength + 1);
  table->record[0]= (unsigned char*) session->alloc(share->rec_buff_length);
  if (!table->record[0])
    goto error;

  if (null_pack_length)
  {
    table->null_flags= (unsigned char*) table->record[0];
    share->null_fields= null_count;
    share->null_bytes= null_pack_length;
  }

  table->in_use= session;           /* field->reset() may access table->in_use */
  {
    /* Set up field pointers */
    unsigned char *null_pos= table->record[0];
    unsigned char *field_pos= null_pos + share->null_bytes;
    uint32_t null_bit= 1;

    for (field= table->field; *field; ++field)
    {
      Field *cur_field= *field;
      if ((cur_field->flags & NOT_NULL_FLAG))
        cur_field->move_field(field_pos);
      else
      {
        cur_field->move_field(field_pos, (unsigned char*) null_pos, null_bit);
        null_bit<<= 1;
        if (null_bit == (1 << 8))
        {
          ++null_pos;
          null_bit= 1;
        }
      }
      cur_field->reset();

      field_pos+= cur_field->pack_length();
    }
  }
  return table;
error:
  for (field= table->field; *field; ++field)
    delete *field;                         /* just invokes field destructor */
  return 0;
}


bool Table::open_tmp_table()
{
  int error;
  if ((error=file->ha_open(this, s->table_name.str,O_RDWR,
                                  HA_OPEN_TMP_TABLE | HA_OPEN_INTERNAL_TABLE)))
  {
    file->print_error(error,MYF(0)); /* purecov: inspected */
    db_stat=0;
    return(1);
  }
  (void) file->extra(HA_EXTRA_QUICK);		/* Faster */
  return(0);
}


/*
  Create MyISAM temporary table

  SYNOPSIS
    create_myisam_tmp_table()
      keyinfo         Description of the index (there is always one index)
      start_recinfo   MyISAM's column descriptions
      recinfo INOUT   End of MyISAM's column descriptions
      options         Option bits
   
  DESCRIPTION
    Create a MyISAM temporary table according to passed description. The is
    assumed to have one unique index or constraint.

    The passed array or MI_COLUMNDEF structures must have this form:

      1. 1-byte column (afaiu for 'deleted' flag) (note maybe not 1-byte
         when there are many nullable columns)
      2. Table columns
      3. One free MI_COLUMNDEF element (*recinfo points here)
   
    This function may use the free element to create hash column for unique
    constraint.

   RETURN
     false - OK
     true  - Error
*/

bool Table::create_myisam_tmp_table(KEY *keyinfo, 
                                    MI_COLUMNDEF *start_recinfo,
                                    MI_COLUMNDEF **recinfo, 
				    uint64_t options)
{
  int error;
  MI_KEYDEF keydef;
  MI_UNIQUEDEF uniquedef;
  TABLE_SHARE *share= s;

  if (share->keys)
  {						// Get keys for ni_create
    bool using_unique_constraint=0;
    HA_KEYSEG *seg= (HA_KEYSEG*) alloc_root(&this->mem_root,
                                            sizeof(*seg) * keyinfo->key_parts);
    if (!seg)
      goto err;

    memset(seg, 0, sizeof(*seg) * keyinfo->key_parts);
    if (keyinfo->key_length >= file->max_key_length() ||
	keyinfo->key_parts > file->max_key_parts() ||
	share->uniques)
    {
      /* Can't create a key; Make a unique constraint instead of a key */
      share->keys=    0;
      share->uniques= 1;
      using_unique_constraint=1;
      memset(&uniquedef, 0, sizeof(uniquedef));
      uniquedef.keysegs=keyinfo->key_parts;
      uniquedef.seg=seg;
      uniquedef.null_are_equal=1;

      /* Create extra column for hash value */
      memset(*recinfo, 0, sizeof(**recinfo));
      (*recinfo)->type= FIELD_CHECK;
      (*recinfo)->length=MI_UNIQUE_HASH_LENGTH;
      (*recinfo)++;
      share->reclength+=MI_UNIQUE_HASH_LENGTH;
    }
    else
    {
      /* Create an unique key */
      memset(&keydef, 0, sizeof(keydef));
      keydef.flag=HA_NOSAME | HA_BINARY_PACK_KEY | HA_PACK_KEY;
      keydef.keysegs=  keyinfo->key_parts;
      keydef.seg= seg;
    }
    for (uint32_t i=0; i < keyinfo->key_parts ; i++,seg++)
    {
      Field *field=keyinfo->key_part[i].field;
      seg->flag=     0;
      seg->language= field->charset()->number;
      seg->length=   keyinfo->key_part[i].length;
      seg->start=    keyinfo->key_part[i].offset;
      if (field->flags & BLOB_FLAG)
      {
	seg->type=
	((keyinfo->key_part[i].key_type & FIELDFLAG_BINARY) ?
	 HA_KEYTYPE_VARBINARY2 : HA_KEYTYPE_VARTEXT2);
	seg->bit_start= (uint8_t)(field->pack_length() - share->blob_ptr_size);
	seg->flag= HA_BLOB_PART;
	seg->length=0;			// Whole blob in unique constraint
      }
      else
      {
	seg->type= keyinfo->key_part[i].type;
      }
      if (!(field->flags & NOT_NULL_FLAG))
      {
	seg->null_bit= field->null_bit;
	seg->null_pos= (uint) (field->null_ptr - (unsigned char*) record[0]);
	/*
	  We are using a GROUP BY on something that contains NULL
	  In this case we have to tell MyISAM that two NULL should
	  on INSERT be regarded at the same value
	*/
	if (!using_unique_constraint)
	  keydef.flag|= HA_NULL_ARE_EQUAL;
      }
    }
  }
  MI_CREATE_INFO create_info;
  memset(&create_info, 0, sizeof(create_info));

  if ((options & (OPTION_BIG_TABLES | SELECT_SMALL_RESULT)) ==
      OPTION_BIG_TABLES)
    create_info.data_file_length= ~(uint64_t) 0;

  if ((error=mi_create(share->table_name.str, share->keys, &keydef,
		       (uint) (*recinfo-start_recinfo),
		       start_recinfo,
		       share->uniques, &uniquedef,
		       &create_info,
		       HA_CREATE_TMP_TABLE)))
  {
    file->print_error(error,MYF(0));	/* purecov: inspected */
    db_stat=0;
    goto err;
  }
  status_var_increment(in_use->status_var.created_tmp_disk_tables);
  share->db_record_offset= 1;
  return false;
 err:
  return true;
}


void Table::free_tmp_table(Session *session)
{
  MEM_ROOT own_root= mem_root;
  const char *save_proc_info;

  save_proc_info=session->get_proc_info();
  session->set_proc_info("removing tmp table");

  if (file)
  {
    if (db_stat)
      file->ha_drop_table(s->table_name.str);
    else
      file->ha_delete_table(s->table_name.str);
    delete file;
  }

  /* free blobs */
  for (Field **ptr= field ; *ptr ; ptr++)
    (*ptr)->free();
  free_io_cache(this);

  if (temp_pool_slot != MY_BIT_NONE)
    bitmap_lock_clear_bit(&temp_pool, temp_pool_slot);

  plugin_unlock(0, s->db_plugin);

  free_root(&own_root, MYF(0)); /* the table is allocated in its own root */
  session->set_proc_info(save_proc_info);

  return;
}

/**
  If a HEAP table gets full, create a MyISAM table and copy all rows
  to this.
*/

bool create_myisam_from_heap(Session *session, Table *table,
                             MI_COLUMNDEF *start_recinfo,
                             MI_COLUMNDEF **recinfo, 
			     int error, bool ignore_last_dupp_key_error)
{
  Table new_table;
  TABLE_SHARE share;
  const char *save_proc_info;
  int write_err;

  if (table->s->db_type() != heap_hton || 
      error != HA_ERR_RECORD_FILE_FULL)
  {
    table->file->print_error(error,MYF(0));
    return(1);
  }
  new_table= *table;
  share= *table->s;
  new_table.s= &share;
  new_table.s->db_plugin= ha_lock_engine(session, myisam_hton);
  if (!(new_table.file= get_new_handler(&share, &new_table.mem_root,
                                        new_table.s->db_type())))
    return(1);				// End of memory

  save_proc_info=session->get_proc_info();
  session->set_proc_info("converting HEAP to MyISAM");

  if (new_table.create_myisam_tmp_table(table->key_info, start_recinfo,
					recinfo, session->lex->select_lex.options | 
					session->options))
    goto err2;
  if (new_table.open_tmp_table())
    goto err1;
  if (table->file->indexes_are_disabled())
    new_table.file->ha_disable_indexes(HA_KEY_SWITCH_ALL);
  table->file->ha_index_or_rnd_end();
  table->file->ha_rnd_init(1);
  if (table->no_rows)
  {
    new_table.file->extra(HA_EXTRA_NO_ROWS);
    new_table.no_rows=1;
  }

#ifdef TO_BE_DONE_LATER_IN_4_1
  /*
    To use start_bulk_insert() (which is new in 4.1) we need to find
    all places where a corresponding end_bulk_insert() should be put.
  */
  table->file->info(HA_STATUS_VARIABLE); /* update table->file->stats.records */
  new_table.file->ha_start_bulk_insert(table->file->stats.records);
#else
  /* HA_EXTRA_WRITE_CACHE can stay until close, no need to disable it */
  new_table.file->extra(HA_EXTRA_WRITE_CACHE);
#endif

  /*
    copy all old rows from heap table to MyISAM table
    This is the only code that uses record[1] to read/write but this
    is safe as this is a temporary MyISAM table without timestamp/autoincrement.
  */
  while (!table->file->rnd_next(new_table.record[1]))
  {
    write_err= new_table.file->ha_write_row(new_table.record[1]);
    if (write_err)
      goto err;
  }
  /* copy row that filled HEAP table */
  if ((write_err=new_table.file->ha_write_row(table->record[0])))
  {
    if (new_table.file->is_fatal_error(write_err, HA_CHECK_DUP) ||
	!ignore_last_dupp_key_error)
      goto err;
  }

  /* remove heap table and change to use myisam table */
  (void) table->file->ha_rnd_end();
  (void) table->file->close();                  // This deletes the table !
  delete table->file;
  table->file=0;
  plugin_unlock(0, table->s->db_plugin);
  share.db_plugin= my_plugin_lock(0, &share.db_plugin);
  new_table.s= table->s;                       // Keep old share
  *table= new_table;
  *table->s= share;
  
  table->file->change_table_ptr(table, table->s);
  table->use_all_columns();
  if (save_proc_info)
  {
    const char *new_proc_info=
      (!strcmp(save_proc_info,"Copying to tmp table") ?
      "Copying to tmp table on disk" : save_proc_info);
    session->set_proc_info(new_proc_info);
  }
  return(0);

 err:
  table->file->print_error(write_err, MYF(0));
  (void) table->file->ha_rnd_end();
  (void) new_table.file->close();
 err1:
  new_table.file->ha_delete_table(new_table.s->table_name.str);
 err2:
  delete new_table.file;
  session->set_proc_info(save_proc_info);
  table->mem_root= new_table.mem_root;
  return(1);
}

my_bitmap_map *Table::use_all_columns(MY_BITMAP *bitmap)
{
  my_bitmap_map *old= bitmap->bitmap;
  bitmap->bitmap= s->all_set.bitmap;
  return old;
}

void Table::restore_column_map(my_bitmap_map *old)
{
  read_set->bitmap= old;
}

uint32_t Table::find_shortest_key(const key_map *usable_keys)
{
  uint32_t min_length= UINT32_MAX;
  uint32_t best= MAX_KEY;
  if (!usable_keys->is_clear_all())
  {
    for (uint32_t nr=0; nr < s->keys ; nr++)
    {
      if (usable_keys->is_set(nr))
      {
        if (key_info[nr].key_length < min_length)
        {
          min_length= key_info[nr].key_length;
          best=nr;
        }
      }
    }
  }
  return best;
}

/*****************************************************************************
  Remove duplicates from tmp table
  This should be recoded to add a unique index to the table and remove
  duplicates
  Table is a locked single thread table
  fields is the number of fields to check (from the end)
*****************************************************************************/

bool Table::compare_record(Field **ptr)
{
  for (; *ptr ; ptr++)
  {
    if ((*ptr)->cmp_offset(s->rec_buff_length))
      return true;
  }
  return false;
}

/* Return false if row hasn't changed */

bool Table::compare_record()
{
  if (s->blob_fields + s->varchar_fields == 0)
    return cmp_record(this, record[1]);
  /* Compare null bits */
  if (memcmp(null_flags,
	     null_flags + s->rec_buff_length,
	     s->null_bytes))
    return true;				// Diff in NULL value
  /* Compare updated fields */
  for (Field **ptr= field ; *ptr ; ptr++)
  {
    if (bitmap_is_set(write_set, (*ptr)->field_index) &&
	(*ptr)->cmp_binary_offset(s->rec_buff_length))
      return true;
  }
  return false;
}





/*****************************************************************************
  The different ways to read a record
  Returns -1 if row was not found, 0 if row was found and 1 on errors
*****************************************************************************/

/** Help function when we get some an error from the table handler. */

int Table::report_error(int error)
{
  if (error == HA_ERR_END_OF_FILE || error == HA_ERR_KEY_NOT_FOUND)
  {
    status= STATUS_GARBAGE;
    return -1;					// key not found; ok
  }
  /*
    Locking reads can legally return also these errors, do not
    print them to the .err log
  */
  if (error != HA_ERR_LOCK_DEADLOCK && error != HA_ERR_LOCK_WAIT_TIMEOUT)
    sql_print_error(_("Got error %d when reading table '%s'"),
		    error, s->path.str);
  file->print_error(error,MYF(0));

  return 1;
}


/*
  Calculate data for each virtual field marked for write in the
  corresponding column map.

  SYNOPSIS
    update_virtual_fields_marked_for_write()
    table                  The Table object
    ignore_stored          Indication whether physically stored virtual
                           fields do not need updating.
                           This value is false when during INSERT and UPDATE
                           and true in all other cases.
 
  RETURN
    0  - Success
    >0 - Error occurred during the generation/calculation of a virtual field value

*/

int update_virtual_fields_marked_for_write(Table *table,
                                           bool ignore_stored)
{
  Field **vfield_ptr, *vfield;
  int error= 0;
  if ((not table) or (not table->vfield))
    return(0);

  /* Iterate over virtual fields in the table */
  for (vfield_ptr= table->vfield; *vfield_ptr; vfield_ptr++)
  {
    vfield= (*vfield_ptr);
    assert(vfield->vcol_info && vfield->vcol_info->expr_item);
    /*
      Only update those fields that are marked in the write_set bitmap
      and not _already_ physically stored in the database.
    */
    if (bitmap_is_set(table->write_set, vfield->field_index) &&
        (not (ignore_stored && vfield->is_stored))
       )
    {
      /* Generate the actual value of the virtual fields */
      error= vfield->vcol_info->expr_item->save_in_field(vfield, 0);
    }
  }
  return(0);
}


/*****************************************************************************
** Instansiate templates
*****************************************************************************/

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
template class List<String>;
template class List_iterator<String>;
#endif
