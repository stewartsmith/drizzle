#ifndef DRIZZLED_ORDER_H
#define DRIZZLED_ORDER_H

/* Order clause list element */

struct order_st {
  struct order_st *next;
  Item	 **item;			/* Point at item in select fields */
  Item	 *item_ptr;			/* Storage for initial item */
  Item   **item_copy;			/* For SPs; the original item ptr */
  int    counter;                       /* position in SELECT list, correct
                                           only if counter_used is true*/
  bool	 asc;				/* true if ascending */
  bool	 free_me;			/* true if item isn't shared  */
  bool	 in_field_list;			/* true if in select field list */
  bool   counter_used;                  /* parameter was counter of columns */
  Field  *field;			/* If tmp-table group */
  char	 *buff;				/* If tmp-table group */
  table_map used, depend_map;
};

#endif /* DRIZZLED_ORDER_H */
