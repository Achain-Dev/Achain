:::: move files
@echo please input the sourcecode solution directory 
xcopy /y  /f  GenesisJson.cpp   %1\Chain\libraries\blockchain\GenesisJson.cpp
xcopy /y  /f  GenesisJson_test.cpp  %1\Chain\libraries\blockchain\GenesisJson_test.cpp
xcopy /y  /f  SeedNodes.hpp   %1\Chain\libraries\include\client\SeedNodes.hpp
