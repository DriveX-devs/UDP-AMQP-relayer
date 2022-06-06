# UDP to AMQP 1.0 relayer

This repository contains a simple, yet efficient, UDP to AMQP 1.0 message relayer.

This relayer is able to receive any UDP message from a given port, and relay it to any AMQP 1.0 broker, given a certain queue or topic.

In order to compile the relayer, you can use the Makefile included in this directory. You can thus compile the relayer executable simply with `make`.
You can then launch the UDP->AMQP relayer with: `./UDPAMQPrelayer --url <broker url> --queue <queue or topic name>`.

If not specified with `--listen-port <port number>`, the relayer will wait for UDP packets on UDP port `49900`.

This relayer has been tested with an [Apache ActiveMQ "Classic"](https://activemq.apache.org/components/classic/download/) broker (version 5).

The relayer relies on the [TCLAP library](http://tclap.sourceforge.net/) in order to parse the command line options.
