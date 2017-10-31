#ifndef glua_vm_h
#define glua_vm_h

#include <glua/lprefix.h>
#include <memory>

namespace glua
{
	namespace vm
	{

		class VMCallinfo
		{
			
		};
		
		class VMState
		{
			
		};

		typedef std::shared_ptr<VMState> VMStateP;

		class VM
		{
		public:
			VM();
			virtual ~VM();

			// void load();

			void execute(VMStateP state);
		};
	}
}

#endif