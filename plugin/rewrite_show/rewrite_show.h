
#ifndef PLUGIN_REWRITE_SHOW_REWRITE_SHOW_H
#define PLUGIN_REWRITE_SHOW_REWRITE_SHOW_H

#include <drizzled/atomics.h>
#include <drizzled/plugin/query_rewrite.h>

#include <vector>
#include <string>

class ShowRewriter : public drizzled::plugin::QueryRewriter
{

public:

  explicit ShowRewriter(std::string name_arg)
    : 
      drizzled::plugin::QueryRewriter(name_arg) 
  {}

  /** Destructor */
  ~ShowRewriter() {}

  /**
   * Returns whether the rewriter is active
   */
  virtual bool isEnabled() const;

  virtual void enable();
  virtual void disable();

  /**
   * Rewriter a query.
   *
   * @param[out] to_rewrite query to rewrite
   */
  void rewrite(std::string &to_rewrite);
  
};

#endif /* PLUGIN_REWRITE_SHOW_REWRITE_SHOW_H */
