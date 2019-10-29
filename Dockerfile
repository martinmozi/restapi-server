FROM ubuntu:18.04                
WORKDIR /

RUN apt-get update && \
	apt-get -y install wget && \
	apt-get -y install git && \
	apt-get -y install libssl-dev && \
	apt-get -y install build-essential && \
	apt-get -y install cmake && \
    cd /tmp && \
    wget https://dl.bintray.com/boostorg/release/1.70.0/source/boost_1_70_0.tar.gz  && \
    tar -xzf boost_1_70_0.tar.gz  && \
    cd boost_1_70_0  && \
    chmod +x bootstrap.sh  && \
    ./bootstrap.sh  && \
    ./b2 install --prefix=/usr --with-filesystem --with-system --with-coroutine  && \
    cd ../..  && \
    rm -rf boost_1_70_0
	
RUN cd /tmp && \
    git clone https://github.com/martinmozi/restapi-server.git --single-branch --branch async && \
    cd restapi-server && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make -j4 && \
    make install 
	