# Docker commands for building a Docker container with the UDP-AMQP 1.0 relayer

In order to build a Docker container containing the relayer, you should use, **from the main repository folder**:
```
sudo docker build -t relayer_image -f docker/Dockerfile .
```

You can then run the container with (sample parameters are shown here - you are then expected to modify them depending on where the S-LDM will be deployed and which area it will cover):
```
sudo docker run -dit \
--env BROKER_URL="127.0.0.1:5672" \
--env AMQP_TOPIC="topic://relay.sample" \
--env UDP_LISTEN_PORT="49900" \
--env MIN_MSG_SIZE="0" \
--env BIND_IP_ADDR="0.0.0.0" \
--net=host --name=relayer_container relayer_image
```

Or (in a single line):
```
sudo docker run -dit --env BROKER_URL="127.0.0.1:5672" --env AMQP_TOPIC="topic://relay.sample" --env UDP_LISTEN_PORT="49900" --env MIN_MSG_SIZE="0" --env BIND_IP_ADDR="0.0.0.0" --net=host --name=relayer_container relayer_image
```

The commands reported above will run the relayer container and grant it access to the host networking (i.e., everything will work, from the network point of view, as if the relayer is run outside the container). This behaviour can be modified by changing the `--net=host` option.

After running the container, it can be started/stopped with `sudo docker container start relayer_container` and `sudo docker container stop relayer_container`.

You can view, instead, the output of the relayer with `sudo docker logs relayer_container`.