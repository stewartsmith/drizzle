/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011, Henrik Ingo.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <config.h>
#include <stdio.h>

#include <drizzled/error.h>
#include <drizzled/plugin/function.h>
#include <drizzled/function/str/strfunc.h>
#include <drizzled/temporal.h>

#include <v8.h>
#define JS_ENGINE "v8"

using namespace std;
using namespace drizzled;


namespace drizzle_plugin {
namespace js {

v8::Handle<v8::Value> V8Version(const v8::Arguments& args);
v8::Handle<v8::Value> JsEngine(const v8::Arguments& args);
const char* v8_to_char(const v8::String::Utf8Value& value);
void emit_drizzle_error(v8::TryCatch* try_catch);


// TODO: So this is a function that returns strings? 
// What is the class for functions that return mixed types?
// Or is this as it should be, apparently js('1') + js('2') does the right thing already.

class JsFunction : public Item_str_func
{
public:
  String *val_str(String *);

  const char *func_name() const 
  { 
    return "js"; 
  }

  void fix_length_and_dec() 
  { 
    maybe_null= 1;
    max_length= MAX_BLOB_WIDTH;   
  }

  bool check_argument_count(int n)
  {
    return (n >= 1);
  }
};

/**
 * @brief Extracts a C string from a V8 Utf8Value
 * 
 * Idea copied from v8 sources, samples/shell.cc. Makes code easier to read than
 * (char *)(*utf8value)
 */
const char* v8_to_char(const v8::String::Utf8Value& value) {
  return *value ? *value : "<javascript v8 string conversion failed>";
}

/**
 * @brief Take v8 exception and emit Drizzle error to client
 * 
 * This is adapted from ReportException() in v8 samples/shell.cc. 
 */
void emit_drizzle_error(v8::TryCatch* try_catch)
{
  v8::HandleScope handle_scope;
  v8::String::Utf8Value exception(try_catch->Exception());
  const char* exception_string = v8_to_char(exception);
  v8::Handle<v8::Message> message = try_catch->Message();
  if (message.IsEmpty()) {
    // V8 didn't provide any extra information about this error; just
    // print the exception.
    my_error(ER_SCRIPT, MYF(0), exception_string);
  } else {
    char buf[2048]; 
    int linenum = message->GetLineNumber();
    sprintf(buf, "At line %i: %.1900s (Do SHOW ERRORS for more information.)", linenum, exception_string);
    my_error(ER_SCRIPT, MYF(0), buf);
    // Print line of source code and where error happened.
    v8::String::Utf8Value sourceline(message->GetSourceLine());
    const char* sourceline_string = v8_to_char(sourceline);
    sprintf(buf, "Line %i: %.160s", linenum, sourceline_string);
    my_error(ER_SCRIPT, MYF(0), buf);    
    int start = message->GetStartColumn();
    sprintf(buf, "Check your script starting at: '%.50s'", &sourceline_string[start]);
    my_error(ER_SCRIPT, MYF(0), buf);
    v8::String::Utf8Value stack_trace(try_catch->StackTrace());
    if (stack_trace.length() > 0) {
      const char* stack_trace_string = v8_to_char(stack_trace);
      my_error(ER_SCRIPT, MYF(0), stack_trace_string);
    }
  }
}

/**
 * @brief Implements js() - execute JavaScript code
 * 
 * @todo row_result types are not yet handled, what are they anyway?
 * @todo Lot's of performance optimizations postponed for later version:
 *  * When available, use v8::Isolate instead of v8::Locker for multithreading 
 * (or a mix of both). 
 *  * As part of this work, refactor v8 stuff into separate 
 * function, proxy, factory or something...
 *  * Save the compiled script so it can be used again if same script is run
 * many times
 *  * Some of the v8 stuff should be done in initialize()
 * 
 * @note DECIMAL_RESULT type is now a double in JavaScript. This could lose 
 * precision. But to send them as strings would also be awkward (+ operator will 
 * do unexpected things). In any case, we'd need some biginteger (bigdecimal?) 
 * kind of library to do anything with higher precision values anyway. If you 
 * want to keep the precision, you can cast your decimal values to strings 
 * explicitly when passing them as arguments.
 * 
 * @param res Pointer to the drizzled::String object that will contain the result
 * @return a drizzled::String containing the value returned by executed JavaScript code (value of last executed statement) 
 */
String *JsFunction::val_str( String *str )
{
  assert( fixed == 1 );
  // If we return from any of the error conditions during method, then 
  // return value of the drizzle function is null.
  null_value= true; 
  
  String *source_str=NULL;
  source_str = args[0]->val_str( str ); 
  
  // Need to use Locker in multi-threaded app. v8 is unlocked by the destructor 
  // when locker goes out of scope.
  // TODO: Newer versions of v8 provide an Isolate class where you can get a 
  // separate instance of v8 (ie one per thread). v8 2.5.9.9 in Ubuntu 11.04 does 
  // not yet offer it.
  v8::Locker locker;
  // Pass code and arguments into v8...
  v8::HandleScope handle_scope;
  // Create a template for the global object and populate a drizzle object.
  v8::Handle<v8::ObjectTemplate> global  = v8::ObjectTemplate::New();
  // Drizzle will contain API's to drizzle variables, functions and tables
  v8::Handle<v8::ObjectTemplate> db = v8::ObjectTemplate::New();
  v8::Handle<v8::ObjectTemplate> js = v8::ObjectTemplate::New();
  // Bind the 'version' function 
  global->Set( v8::String::New("db"), db );
  db->Set( v8::String::New("js"), js );
  js->Set( v8::String::New("version"), v8::FunctionTemplate::New(V8Version) );
  js->Set( v8::String::New("engine"), v8::FunctionTemplate::New(JsEngine) );
  
  // Now bind the arguments into argv[]
  // v8::Array can only be created when context is already entered (otherwise v8 segfaults!)
  v8::Persistent<v8::Context> context = v8::Context::New( NULL, global );
  if ( context.IsEmpty() ) {
    char buf[100];
    sprintf(buf, "Error in js() while creating JavaScript context in %s.", JS_ENGINE);
    my_error(ER_SCRIPT, MYF(0), buf);
    return NULL;
  }
  context->Enter();
   
  v8::Handle<v8::Array> a = v8::Array::New(arg_count-1);
  for( uint64_t n = 1; n < arg_count; n++ )
  {
    // Need to do this differently for ints, doubles and strings
    // TODO: There is also ROW_RESULT. Is that relevant here? What does it look like? I could pass rows as an array or object.
    if( args[n]->result_type() == INT_RESULT ){
      // TODO: Turns out Drizzle doesn't do unsigned. So this code path can never happen? (I can't test it at least...)
      if( args[n]->is_unsigned() ) {
        a->Set( n-1, v8::Integer::NewFromUnsigned( (uint32_t) args[n]->val_uint() ) );
      } else {
        a->Set( n-1, v8::Integer::New((int32_t)args[n]->val_int() ) );
      }
    } else if ( args[n]->result_type() == REAL_RESULT || args[n]->result_type() == DECIMAL_RESULT ) {
      a->Set( n-1, v8::Number::New(args[n]->val_real() ) );
    } else if ( true || args[n]->result_type() == STRING_RESULT ) {
      if ( args[n]->is_datetime() ) {
        // DATE/TIME values are also STRING_RESULT, make them a Date type in v8
        // Now we need to get the unix timestamp integer, surprisingly tricky...
        // TODO: This should really be just args[n]->get_epoch_seconds(). I need to write a separate patch for Item class one of these days.
        type::Time ltime;
        Timestamp temporal;
        args[n]->get_date(ltime, 0);
        temporal.set_years(ltime.year);
        temporal.set_months(ltime.month);
        temporal.set_days(ltime.day);
        temporal.set_hours(ltime.hour);
        temporal.set_minutes(ltime.minute);
        temporal.set_seconds(ltime.second);
        temporal.set_epoch_seconds();
        if (temporal.is_valid())
        {
          time_t tmp;
          temporal.to_time_t(tmp);
          // Pay attention, Ecmascript defines a date as *milliseconds* since unix epoch
          // Also, on platforms where time_t is 32 bit, we need explicit cast to 64 bit integer
          a->Set( n-1, v8::Date::New(((uint64_t)tmp)*1000) );
        } else {
          a->Set( n-1, v8::String::New(args[n]->val_str(str)->c_str() ) );
        }
      } else {
        // Default to creating string values in JavaScript
        a->Set( n-1, v8::String::New(args[n]->val_str(str)->c_str() ) );
      }
    }
    // If user has given a name to the arguments, pass these as global variables
    if( ! args[n]->is_autogenerated_name ) {
      if( args[n]->result_type() == INT_RESULT ){
        if( args[n]->is_unsigned() ) {
          context->Global()->Set( v8::String::New( args[n]->name ), v8::Integer::NewFromUnsigned( (uint32_t) args[n]->val_uint() ) );
        } else {
          context->Global()->Set( v8::String::New( args[n]->name ), v8::Integer::New((int32_t)args[n]->val_int() ) );
        }
      } else if ( args[n]->result_type() == REAL_RESULT || args[n]->result_type() == DECIMAL_RESULT ) {
        context->Global()->Set( v8::String::New( args[n]->name ), v8::Number::New(args[n]->val_real() ) );
      } else if ( true || args[n]->result_type() == STRING_RESULT ) {
      if ( args[n]->is_datetime() ) {
        // DATE/TIME values are also STRING_RESULT, make them a Date type in v8
        // Now we need to get the unix timestamp integer, surprisingly tricky...
        // TODO: This should really be just args[n]->get_epoch_seconds(). I need to write a separate patch for Item class one of these days.
        type::Time ltime;
        Timestamp temporal;
        args[n]->get_date(ltime, 0);
        temporal.set_years(ltime.year);
        temporal.set_months(ltime.month);
        temporal.set_days(ltime.day);
        temporal.set_hours(ltime.hour);
        temporal.set_minutes(ltime.minute);
        temporal.set_seconds(ltime.second);
        temporal.set_epoch_seconds();
        if (temporal.is_valid())
        {
          time_t tmp;
          temporal.to_time_t(tmp);
          // Pay attention, Ecmascript defines a date as *milliseconds* since unix epoch
          // Also, on platforms where time_t is 32 bit, we need explicit cast to 64 bit integer
          context->Global()->Set( v8::String::New( args[n]->name ), v8::Date::New(((uint64_t)tmp)*1000) );
        } else {
          context->Global()->Set( v8::String::New( args[n]->name ), v8::String::New(args[n]->val_str(str)->c_str() ) );
        }
      } else {
        context->Global()->Set( v8::String::New( args[n]->name ), v8::String::New(args[n]->val_str(str)->c_str() ) );
      }
      }
    }
  }
  //Need to fetch the global element back from context, global doesn't work anymore
  context->Global()->Set( v8::String::New("arguments"), a );

  
  
  // Compile the source code.
  v8::TryCatch try_catch;
  v8::Handle<v8::Value> result;
  // Create a v8 string containing the JavaScript source code.
  // Convert from drizzled::String to char* string to v8::String.
  v8::Handle<v8::String> source = v8::String::New(source_str->c_str());
  v8::Handle<v8::Script> script = v8::Script::Compile(source);
  if ( script.IsEmpty() ) {
    emit_drizzle_error(&try_catch);
    return NULL;
  } else {
    result = script->Run();
    if ( result.IsEmpty() ) {
      assert( try_catch.HasCaught() );
      emit_drizzle_error( &try_catch );
      // Dispose of Persistent objects before returning. (Is it needed?)
      context->Exit();
      context.Dispose();
      return NULL;
    } else {
      assert( !try_catch.HasCaught() );
      if ( result->IsUndefined() ) {
        // Nothing wrong here, but we return Undefined as NULL.
        // Dispose of Persistent objects before returning. (Is it needed?)
        context->Exit();
        context.Dispose();
        return NULL;
      }
    }
  }
    
  // Run the script to get the result.
  //v8::Handle<v8::Value> foo = script->Run();
  v8::Handle<v8::String> rstring = result->ToString();
  
  // Convert the result to a drizzled::String and print it.
  // Allocate space to the drizzled::String 
  str->free(); //TODO: Check the source for alloc(), but apparently I don't need this line?
  str->alloc( rstring->Utf8Length() );
  // Now copy string from v8 heap to drizzled heap
  rstring->WriteUtf8( str->ptr() );
  // drizzled::String doesn't actually set string length properly in alloc(), so set it now
  str->length( rstring->Utf8Length() );
 
  context->Exit();
  context.Dispose();

  // There was no error and value returned is not undefined, so it's not null.
  null_value= false;
  return str;
}




plugin::Create_function<JsFunction> *js_function = NULL;

static int initialize( module::Context &context )
{
  js_function = new plugin::Create_function<JsFunction>("js");
  context.add( js_function );
  // Initialize V8
  v8::V8::Initialize();
  return 0;
}


/* Functions that are part of the JavaScript API ***************************/

/**
 * @brief Binds as db.js.version() inside JavaScript.
 * @return Version number of v8 engine
 */
v8::Handle<v8::Value> V8Version( const v8::Arguments& ) {
  return v8::String::New( v8::V8::GetVersion() );
}

/**
 * @brief Binds as db.js.engine() inside JavaScript.
 * @return The string "v8"
 */
v8::Handle<v8::Value> JsEngine( const v8::Arguments& ) {
  return v8::String::New( JS_ENGINE );
}

} // namespace js

} // namespace drizzle_plugin

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "js",
  "0.9",
  "Henrik Ingo",
  N_("Executes JavaScript code with supplied arguments"),
  PLUGIN_LICENSE_GPL,
  drizzle_plugin::js::initialize,
  NULL,
  NULL
}
DRIZZLE_DECLARE_PLUGIN_END;
       
