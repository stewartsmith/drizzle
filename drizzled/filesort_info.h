#ifndef DRIZZLED_FILESORT_INFO_ST_H
#define DRIZZLED_FILESORT_INFO_ST_H

/* Information on state of filesort */
struct filesort_info_st
{
  IO_CACHE *io_cache;           /* If sorted through filesort */
  uchar     **sort_keys;        /* Buffer for sorting keys */
  uchar     *buffpek;           /* Buffer for buffpek structures */
  uint      buffpek_len;        /* Max number of buffpeks in the buffer */
  uchar     *addon_buf;         /* Pointer to a buffer if sorted with fields */
  size_t    addon_length;       /* Length of the buffer */
  struct st_sort_addon_field *addon_field;     /* Pointer to the fields info */
  void    (*unpack)(struct st_sort_addon_field *, uchar *); /* To unpack back */
  uchar     *record_pointers;    /* If sorted in memory */
  ha_rows   found_records;      /* How many records in sort */
};

#endif /* DRIZZLED_FILESORT_INFO_ST_H */
