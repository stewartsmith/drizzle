
#ifndef PLUGIN_LOWER_REWRITER_LOWER_REWRITER_H
#define PLUGIN_LOWER_REWRITER_LOWER_REWRITER_H

#include <drizzled/atomics.h>
#include <drizzled/plugin/query_rewrite.h>

#include <vector>
#include <string>

class LowerRewriter : public drizzled::plugin::QueryRewriter
{

public:

  explicit LowerRewriter(std::string name_arg)
    : 
      drizzled::plugin::QueryRewriter(name_arg) 
  {}

  /** Destructor */
  ~LowerRewriter() {}

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

#endif /* PLUGIN_LOWER_REWRITER_LOWER_REWRITER_H */
