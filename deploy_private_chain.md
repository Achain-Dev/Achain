//TODO

go into directory deploy_tool 

1. config test.json and modify parameters

2. run .exe file  "gen_genesis_json_file.exe", and it will generate some result files in the directory 

3. copy .cpp files under the deploy_tool directory into the source code directory and then build GRBIT solution

4. run command "python keys_convert_to_import.py"

5. run command "GRBit.exe --server --rpcuser a --rpcpassword b --rpcport 12345 --data-dir C:/xxx/" and then run following command 
   "execute_script xxx/xxx/import_wif_keys.json"
