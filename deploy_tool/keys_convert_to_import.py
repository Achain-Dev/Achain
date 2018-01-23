#!/usr/bin/env python
# coding=utf-8

walletaccount=">>> wallet_create achaintest 12345678 \n"
fd_keys = open("./wif_keys.json", "r")
fd_import = open("./import_wif_keys.json", "w")

fd_import.write(walletaccount);
for line in  fd_keys:
    if line[0] == '[' or line[0] == ']':
        continue
    line = line.replace("\"", "").replace(",", "")
    print(line)
    line = line.strip()
    # print(line)
    line = ">>> import_key " + line + '\n'
    fd_import.write(line)

     
fd_import.write(">>> delegate_set_network_min_connection_count 0 \n")
fd_import.write(">>> wallet_delegate_set_block_production ALL true \n")