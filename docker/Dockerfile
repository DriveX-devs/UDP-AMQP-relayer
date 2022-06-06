FROM ubuntu:20.04

ENV TZ=Europe/Rome

# Default broker URL and port
ENV BROKER_URL 127.0.0.1:5672

# Default broker topic
ENV AMQP_TOPIC topic://relay.sample

# Default UDP port to listen to
ENV UDP_LISTEN_PORT 49900

# Minimum message size to be relayed
ENV MIN_MSG_SIZE 0

# IP address of the interface to bind to (0.0.0.0, as default value, will bind to any interface)
ENV BIND_IP_ADDR 0.0.0.0

# To set a default plain authentication with username "user" and password "passwd", use: ENV OTHER_OPTIONS "-u user -p passwd -I"
# To enable quadkeys computation and transmission, add "--enable-quadkeys" to ENV OTHER_OPTIONS

# Set timezone
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# Create "relayer" user
RUN useradd -ms /bin/bash relayer

# Copy the relayer source code inside the container
COPY . /home/relayer/UDP-AMQP-relayer

# Install dependencies
RUN apt-get update && \
    apt-get install -y apt-utils && \
    apt-get install -y build-essential cmake cmake-curses-gui uuid-dev libssl-dev libsasl2-2 libsasl2-dev libsasl2-modules wget python3

# Download Qpid Proton 0.35.0
RUN wget https://archive.apache.org/dist/qpid/proton/0.35.0/qpid-proton-0.35.0.tar.gz && \
    tar zxvf qpid-proton-0.35.0.tar.gz

# Install Qpid Proton
RUN cd qpid-proton-0.35.0 && \
    mkdir build && \
    cd build && \
    cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DSYSINSTALL_BINDINGS=ON -DBUILD_PYTHON=OFF -DBUILD_RUBY=OFF -DBUILD_GO=OFF && \
    make all && \
    make install

# Compile the UDP-AMQP relayer
RUN cd /home/relayer/UDP-AMQP-relayer && \
    make fullclean && \
    make
    
USER relayer

WORKDIR /home/relayer/UDP-AMQP-relayer
# EXPOSE 5671 5672 8161

CMD ["/bin/sh", "-c", "./UDPAMQPrelayer --url ${BROKER_URL} --queue ${AMQP_TOPIC} --listen-port ${UDP_LISTEN_PORT} --min-msg-size ${MIN_MSG_SIZE} --bindto ${BIND_IP_ADDR} ${OTHER_OPTIONS}"]
