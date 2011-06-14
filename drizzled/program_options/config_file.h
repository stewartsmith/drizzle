/*
 * Copyright (C) 2002-2004 Vladimir Prus.
 * Copyright (C) 2010 Monty Taylor
 *
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#pragma once

#include <boost/program_options.hpp>
#include <boost/program_options/eof_iterator.hpp>
#include <boost/static_assert.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/algorithm/string.hpp>

#include <boost/noncopyable.hpp>

#include <iosfwd>
#include <vector>
#include <utility>
#include <set>

namespace drizzled
{
namespace program_options
{

typedef std::pair<std::string, std::string> option_result_pair;
std::string parse_suffix(const std::string& arg_val);
option_result_pair parse_size_suffixes(std::string s);
option_result_pair parse_size_arg(std::string s);

std::string parse_suffix(const std::string& arg_val)
{
  try
  {
    size_t size_suffix_pos= arg_val.find_last_of("kmgKMG");
    if (size_suffix_pos == arg_val.size()-1)
    {
      char suffix= arg_val[size_suffix_pos];
      std::string size_val(arg_val.substr(0, size_suffix_pos));

      uint64_t base_size= boost::lexical_cast<uint64_t>(size_val);
      uint64_t new_size= 0;

      switch (suffix)
      {
      case 'K':
      case 'k':
        new_size= base_size * 1024;
        break;
      case 'M':
      case 'm':
        new_size= base_size * 1024 * 1024;
        break;
      case 'G':
      case 'g':
        new_size= base_size * 1024 * 1024 * 1024;
        break;
      }
      return boost::lexical_cast<std::string>(new_size);
    }
  }
  catch (std::exception&)
  { }

  return arg_val;
}

option_result_pair parse_size_suffixes(std::string s)
{
  size_t equal_pos= s.find("=");
  if (equal_pos != std::string::npos)
  {
    std::string arg_key(s.substr(0, equal_pos));
    std::string arg_val(parse_suffix(s.substr(equal_pos+1)));

    if (arg_val != s.substr(equal_pos+1))
    {
      return std::make_pair(arg_key, arg_val);
    }
  }

  return std::make_pair(std::string(""), std::string(""));
}

option_result_pair parse_size_arg(std::string s)
{
  if (s.find("--") == 0)
  {
    return parse_size_suffixes(s.substr(2));
  }
  return make_pair(std::string(""), std::string(""));
}

class invalid_syntax :
  public boost::program_options::error
{
public:
  enum kind_t
  {
    long_not_allowed = 30,
    long_adjacent_not_allowed,
    short_adjacent_not_allowed,
    empty_adjacent_parameter,
    missing_parameter,
    extra_parameter,
    unrecognized_line
  };

  invalid_syntax(const std::string& in_tokens, kind_t in_kind);


  // gcc says that throw specification on dtor is loosened
  // without this line
  ~invalid_syntax() throw() {}

  kind_t kind() const
  {
    return m_kind;
  }


  const std::string& tokens() const
  {
    return m_tokens;
  }


protected:
  /** Used to convert kind_t to a related error text */
  static std::string error_message(kind_t kind)
  {
    // Initially, store the message in 'const char*' variable, to avoid
    // conversion to string in all cases.
    const char* msg;
    switch(kind)
    {
    case long_not_allowed:
      msg = "long options are not allowed";
      break;
    case long_adjacent_not_allowed:
      msg = "parameters adjacent to long options not allowed";
      break;
    case short_adjacent_not_allowed:
      msg = "parameters adjust to short options are not allowed";
      break;
    case empty_adjacent_parameter:
      msg = "adjacent parameter is empty";
      break;
    case missing_parameter:
      msg = "required parameter is missing";
      break;
    case extra_parameter:
      msg = "extra parameter";
      break;
    case unrecognized_line:
      msg = "unrecognized line";
      break;
    default:
      msg = "unknown error";
    }
    return msg;
  }

private:
  // TODO: copy ctor might throw
  std::string m_tokens;

  kind_t m_kind;
};

invalid_syntax::invalid_syntax(const std::string& in_tokens,
                               invalid_syntax::kind_t in_kind) :
  boost::program_options::error(error_message(in_kind).append(" in '").append(in_tokens).append("'")),
  m_tokens(in_tokens),
  m_kind(in_kind)
{ }

namespace detail
{

/** Standalone parser for config files in ini-line format.
  The parser is a model of single-pass lvalue iterator, and
  default constructor creates past-the-end-iterator. The typical usage is:
  config_file_iterator i(is, ... set of options ...), e;
  for(; i !=e; ++i) {
 *i;
 }

 Syntax conventions:

 - config file can not contain positional options
 - '#' is comment character: it is ignored together with
 the rest of the line.
 - variable assignments are in the form
 name '=' value.
 spaces around '=' are trimmed.
 - Section names are given in brackets. 

 The actual option name is constructed by combining current section
 name and specified option name, with dot between. If section_name 
 already contains dot at the end, new dot is not inserted. For example:
 @verbatim
 [gui.accessibility]
 visual_bell=yes
 @endverbatim
 will result in option "gui.accessibility.visual_bell" with value
 "yes" been returned.

 */    
class common_config_file_iterator :
  public boost::eof_iterator<common_config_file_iterator,
                             boost::program_options::option>
{
public:
  common_config_file_iterator()
  {
    found_eof();
  }

  common_config_file_iterator(const std::set<std::string>& in_allowed_options,
                              bool allow_unregistered) :
    allowed_options(in_allowed_options),
    m_allow_unregistered(allow_unregistered)
  {
    for(std::set<std::string>::const_iterator i = allowed_options.begin();
        i != allowed_options.end(); 
        ++i)
    {
      add_option(i->c_str());
    }
  }

  virtual ~common_config_file_iterator() {}

public: // Method required by eof_iterator

  void get()
  {
    std::string s;
    std::string::size_type n;
    bool found = false;

    while(this->getline(s)) {

      // strip '#' comments and whitespace
      if ((n = s.find('#')) != std::string::npos)
        s = s.substr(0, n);
      boost::trim(s);

      if (!s.empty()) {
        // Handle section name
        if (*s.begin() == '[' && *s.rbegin() == ']')
        {
          m_prefix = s.substr(1, s.size()-2);
          if (*m_prefix.rbegin() != '.')
            m_prefix += '.';
        }
        else
        {
          
          std::string name;
          std::string option_value("true");

          if ((n = s.find('=')) != std::string::npos)
          {

            name = m_prefix + boost::trim_copy(s.substr(0, n));
            option_value = boost::trim_copy(parse_suffix(s.substr(n+1)));

          }
          else
          {
            name = m_prefix + boost::trim_copy(s);
          }

          bool registered = allowed_option(name);
          if (!registered && !m_allow_unregistered)
            boost::throw_exception(boost::program_options::unknown_option(name));

          found = true;
          this->value().string_key = name;
          this->value().value.clear();
          this->value().value.push_back(option_value);
          this->value().unregistered = !registered;
          this->value().original_tokens.clear();
          this->value().original_tokens.push_back(name);
          this->value().original_tokens.push_back(option_value);
          break;

        }
      }
    }
    if (!found)
      found_eof();
  }

protected: // Stubs for derived classes

  // Obtains next line from the config file
  // Note: really, this design is a bit ugly
  // The most clean thing would be to pass 'line_iterator' to
  // constructor of this class, but to avoid templating this class
  // we'd need polymorphic iterator, which does not exist yet.
  virtual bool getline(std::string&) { return false; }

private:
  /** Adds another allowed option. If the 'name' ends with
    '*', then all options with the same prefix are
    allowed. For example, if 'name' is 'foo*', then 'foo1' and
    'foo_bar' are allowed. */
  void add_option(const char* name)
  {
    std::string s(name);
    assert(!s.empty());
    if (*s.rbegin() == '*')
    {
      s.resize(s.size()-1);
      bool bad_prefixes(false);
      // If 's' is a prefix of one of allowed suffix, then
      // lower_bound will return that element.
      // If some element is prefix of 's', then lower_bound will
      // return the next element.
      std::set<std::string>::iterator i = allowed_prefixes.lower_bound(s);
      if (i != allowed_prefixes.end())
      {
        if (i->find(s) == 0)
          bad_prefixes = true;                    
      }
      if (i != allowed_prefixes.begin())
      {
        --i;
        if (s.find(*i) == 0)
          bad_prefixes = true;
      }
      if (bad_prefixes)
        boost::throw_exception(boost::program_options::error("bad prefixes"));
      allowed_prefixes.insert(s);
    }
  }


  // Returns true if 's' is a registered option name.
  bool allowed_option(const std::string& s) const
  {
    std::set<std::string>::const_iterator i = allowed_options.find(s);
    if (i != allowed_options.end())
      return true;        
    // If s is "pa" where "p" is allowed prefix then
    // lower_bound should find the element after "p". 
    // This depends on 'allowed_prefixes' invariant.
    i = allowed_prefixes.lower_bound(s);
    if (i != allowed_prefixes.begin() && s.find(*--i) == 0)
      return true;
    return false;
  }


  // That's probably too much data for iterator, since
  // it will be copied, but let's not bother for now.
  std::set<std::string> allowed_options;
  // Invariant: no element is prefix of other element.
  std::set<std::string> allowed_prefixes;
  std::string m_prefix;
  bool m_allow_unregistered;
};

template<class charT>
class basic_config_file_iterator :
  public common_config_file_iterator
{
public:

  basic_config_file_iterator()
  {
    found_eof();
  }

  /** Creates a config file parser for the specified stream. */
  basic_config_file_iterator(std::basic_istream<charT>& is, 
                             const std::set<std::string>& allowed_options,
                             bool allow_unregistered = false); 

private: // base overrides

  bool getline(std::string&);

private: // internal data
  boost::shared_ptr<std::basic_istream<charT> > is;
};

typedef basic_config_file_iterator<char> config_file_iterator;
typedef basic_config_file_iterator<wchar_t> wconfig_file_iterator;

struct null_deleter
{
  void operator()(void const *) const {}
};


template<class charT>
basic_config_file_iterator<charT>::
basic_config_file_iterator(std::basic_istream<charT>& in_is, 
                           const std::set<std::string>& in_allowed_options,
                           bool in_allow_unregistered) :
  common_config_file_iterator(in_allowed_options, in_allow_unregistered)
{
  this->is.reset(&in_is, null_deleter());                 
  get();
}


// Specializing this function for wchar_t causes problems on
// borland and vc7, as well as on metrowerks. On the first two
// I don't know a workaround, so make use of 'to_internal' to
// avoid specialization.
template<class charT>
bool
basic_config_file_iterator<charT>::getline(std::string& s)
{
  if (std::getline(*is, s))
  {
    return true;
  }
  else
  {
    return false;
  }
}

} /* namespace detail */

/** Parse a config file. 

  Read from given stream.
*/
template<class charT>
boost::program_options::basic_parsed_options<charT>
parse_config_file(std::basic_istream<charT>& is,
                  const boost::program_options::options_description& desc,
                  bool allow_unregistered = false)
{    
  std::set<std::string> allowed_options;

  const std::vector<boost::shared_ptr<boost::program_options::option_description> >& options = desc.options();
  for (unsigned i = 0; i < options.size(); ++i)
  {
    const boost::program_options::option_description& d= *options[i];

    if (d.long_name().empty())
      boost::throw_exception(boost::program_options::error("long name required for config file"));

    allowed_options.insert(d.long_name());
  }

  // Parser return char strings
  boost::program_options::parsed_options result(&desc);        
  std::copy(detail::basic_config_file_iterator<charT>(is,
                                                      allowed_options,
                                                      allow_unregistered), 
       detail::basic_config_file_iterator<charT>(), 
       std::back_inserter(result.options));
  // Convert char strings into desired type.
  return boost::program_options::basic_parsed_options<charT>(result);
}

/** Parse a config file. 

  Read from file with the given name. The character type is
  passed to the file stream. 
*/
template<class charT>
boost::program_options::basic_parsed_options<charT>
parse_config_file(const char* filename,
                  const boost::program_options::options_description& desc,
                  bool allow_unregistered = false)
{ 
  // Parser return char strings
  std::basic_ifstream< charT > strm(filename);
  if (!strm) 
  {
    boost::throw_exception("Couldn't open file");
  }
  return parse_config_file(strm, desc, allow_unregistered);
}

} /* namespace program_options */
} /* namespace drizzled */


