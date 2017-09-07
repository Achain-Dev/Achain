#include <glua/glua_api_types.h>
#include <glua/thinkyoung_lua_api.h>

GluaModuleByteStream::GluaModuleByteStream()
{
	is_bytes = false;
	contract_level = CONTRACT_LEVEL_TEMP;
	contract_state = CONTRACT_STATE_VALID;
}

GluaModuleByteStream::~GluaModuleByteStream()
{
	contract_apis.clear();
	offline_apis.clear();
	contract_emit_events.clear();
}