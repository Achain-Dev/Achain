#ifndef exceptions_h
#define exceptions_h

#include "glua/lprefix.h"
#include <glua/thinkyoung_lua_api.h>

#include <string>
#include <exception>
#include <cstdint>
#include <boost/preprocessor/stringize.hpp>

namespace glua
{
	namespace core
	{
		class GluaException : public std::exception
		{
		protected:
			int64_t _code;
			std::string _name_value;
			std::string _error_msg;
		public:
			inline GluaException() : _code(0) { }
			inline GluaException(int64_t code, const std::string &name_value, const std::string &error_msg)
				:_code(code), _name_value(name_value), _error_msg(error_msg){}
			inline GluaException(const GluaException& other) {
				_code = other._code;
				_name_value = other._name_value;
				_error_msg = other._error_msg;
			}
			inline GluaException(const char *msg)
			{
				_code = THINKYOUNG_API_SIMPLE_ERROR;
				_error_msg = msg;
			}
			inline GluaException& operator=(const GluaException& other) {
				if (this != &other)
				{
					_error_msg = other._error_msg;
				}
				return *this;
			}
			inline virtual ~GluaException() {}
                
#ifdef WIN32
            inline virtual const char* what() const
#else
            inline virtual const char* what() const noexcept
#endif 
            {
				return _error_msg.c_str();
			}
			inline virtual int64_t code() const {
				return _code;
			}
			inline std::string name() const
			{
				return _name_value;
			}
			inline virtual std::shared_ptr<glua::core::GluaException> dynamic_copy_exception()const
			{
				return std::make_shared<GluaException>(*this);
			} 
			inline virtual void dynamic_rethrow_exception()const 
			{
				if (code() == 0)
					throw *this; 
				else
					glua::core::GluaException::dynamic_rethrow_exception();
			} 
		};

	}
}

#define LUA_DECLARE_DERIVED_EXCEPTION(TYPE, BASE, CODE, WHAT) \
	class TYPE : public BASE  \
   { \
      public: \
       explicit TYPE( int64_t code, const std::string& name_value, const std::string& what_value ) \
       :BASE( code, name_value, what_value ){} \
		explicit TYPE()	\
		:BASE(CODE, BOOST_PP_STRINGIZE(TYPE), WHAT) {} \
       virtual std::shared_ptr<glua::core::GluaException> dynamic_copy_exception()const \
       { return std::make_shared<TYPE>( *this ); } \
       virtual void dynamic_rethrow_exception()const \
       { if( code() == CODE ) throw *this;\
         else glua::core::GluaException::dynamic_rethrow_exception(); \
       } \
   };

#define LUA_DECLARE_EXCEPTION(TYPE, CODE, WHAT) LUA_DECLARE_DERIVED_EXCEPTION(TYPE, glua::core::GluaException, CODE, WHAT)

namespace thinkyoung
{
	namespace lua
	{
		namespace exceptions
		{
			LUA_DECLARE_EXCEPTION(lua_debug_exception, 1, "lua debug exception");
		}
	}
}


#endif