
#ifndef glua_loader_h
#define glua_loader_h

#include "glua/lprefix.h"
#include <stdio.h>
#include <string.h>
#include <string>
#include <list>

#include "glua/llimits.h"
#include "glua/lstate.h"
#include "glua/lua.h"
#include "glua/thinkyoung_lua_api.h"

namespace thinkyoung
{
    namespace lua
    {
        namespace parser
        {

            class LuaLoader
            {
            private:
                GluaModuleByteStream *_stream; // byte code stream
            public:
                LuaLoader(GluaModuleByteStream *stream);
                ~LuaLoader();

                void load_bytecode(); // load bytecode stream to AST
            };

        }
    }
}

#endif