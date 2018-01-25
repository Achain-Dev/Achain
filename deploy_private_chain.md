# instructions for you to generate genesis files

go into the directory*Achain/deploy_tool* 

  1. Config test.json and modify its parameters
     ![testjson](https://github.com/Achain-Dev/Achain/blob/master/deploy_tool/test_json.jpg "testjsonlogo")

  2. Run .exe file  **"gen_genesis_json_file.exe"** and it will generate some result files in the directory 
      ![result](https://github.com/Achain-Dev/Achain/blob/master/deploy_tool/result.jpg "resultlogo")
     
  3. Copy *.cpp files under the deploy_tool directory into the corresponding source code directory, replace them and then build GRBIT solution.
     you could also refer to the "copyGenesisfiles.bat" script file.

  4. Run command **"python keys_convert_to_import.py"** and it will generate **"import_wif_keys.json"**

  5. Run command **"GRBit.exe --server --rpcuser a --rpcpassword b --rpcport 12345 --data-dir C:/xxx/"** and then run following command 
     "execute_script xxx/xxx/import_wif_keys.json"
  
  6. Run command "info" or "help" you will get some information and some other commands.



--------------------------------
[testjson-logo]:https://github.com/Achain-Dev/Achain/blob/master/deploy_tool/test_json.jpg "testjsonlogo"
[result-logo]:https://github.com/Achain-Dev/Achain/blob/master/deploy_tool/resultfile.jpg "resultlogo"
