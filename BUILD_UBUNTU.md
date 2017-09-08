These instructions worked on a fresh Ubuntu 14.04 LTS image.

    sudo apt-get update
    sudo apt-get install cmake git libreadline-dev uuid-dev g++ libdb++-dev libdb-dev zip libssl-dev openssl build-essential python-dev autotools-dev libicu-dev libbz2-dev libboost-dev libboost-all-dev
    git clone https://github.com/Achain-Dev/Achain.git
    cd Achain
    git submodule update --init --recursive
    cmake .
    export LC_ALL="en_US.UTF-8"
    make
