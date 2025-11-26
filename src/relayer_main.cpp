#include <errno.h>
#include <unistd.h>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <cstring>

#include <proton/connection.hpp>
#include <proton/delivery.hpp>
#include <proton/message.hpp>
#include <proton/tracker.hpp>
#include <proton/connection_options.hpp>

#include <sys/types.h>
#include <sys/socket.h>

// TCLAP headers
#include "tclap/CmdLine.h"

// Internal headers
#include "messagerelayeramqp.h"

// Value for an infinite timeout for poll()
// Any negative value disables timeout and makes poll() waiting indefinitely for new events 
// (i.e., eiter packets or writes on the "unlock pipe" to gracefully terminate the relayer)
#define INDEFINITE_BLOCK -1

// Global atomic flag to terminate the whole program in case of errors
std::atomic<bool> terminatorFlag;

double retry_interval_seconds=0.0;

// Thread callback function
void *msgrelayer_callback(void *arg) {
	msgrelayerAMQP *cr_AMQP_class_ptr=static_cast<msgrelayerAMQP *>(arg);
	int unlock_pd_wr=cr_AMQP_class_ptr->getUnlockPipeDescriptorWrite();

	// Checking this just as a matter of additional safety
	if(cr_AMQP_class_ptr!=NULL) {
		while(terminatorFlag==false) {
			try {
				// Create a new Qpid Proton container and run it to start the AMQP 1.0 event loop
				proton::container(*cr_AMQP_class_ptr).run();

				pthread_exit(NULL);
			} catch (const std::exception& e) {
				std::cerr << "Qpid Proton library error while running CAMrelayerAMQP. Please find more details below." << std::endl;
				std::cerr << e.what() << std::endl;

				if(retry_interval_seconds<=0) {
					terminatorFlag = true;
					if(unlock_pd_wr<=0 || write(unlock_pd_wr,"\0",1)<0) {
						fprintf(stderr,"Warning: could not gracefully terminate the AMQP client thread.\n"
							"Its termination will be forced.\n");
						exit(EXIT_FAILURE);
					}
				} else {
					usleep(retry_interval_seconds*1e6);
				}
			}
		}
	} else {
		std::cerr << "Error. NULL CAMrelayerAMQP object. Cannot start the AMQP client." << std::endl;
		terminatorFlag = true;
		if(unlock_pd_wr<=0 || write(unlock_pd_wr,"\0",1)<0) {
			fprintf(stderr,"Warning: could not gracefully terminate the AMQP client thread.\n"
				"Its termination will be forced.\n");
			exit(EXIT_FAILURE);
		}
	}

	// Even if this thread will run forever (then, we can think also about adding, in the future, some way to gracefully terminated it)
	// it is good pratice to terminate it with pthread_exit()
	pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
	// Create thread structure to pass the needed arguments to the thread callback
	pthread_camrelayer_args_t cam_args;
	int listen_port = 49900;
	std::string bind_ip = "0.0.0.0";
	int minimum_msg_size = 0;
	bool quadk_enable = false;

	std::string amqp_username="";
	std::string amqp_password="";
	bool amqp_reconnect=false;
	bool amqp_allow_sasl=false;
	bool amqp_allow_plain=false;
	long amqp_idle_timeout_ms=-1;

	// Parse the command line options with the TCLAP library
	try {
		TCLAP::CmdLine cmd("UDP->AMQP 1.0 relayer", ' ', "1.1");

		// Arguments: short option, long option, description, is it mandatory?, default value, type indication (just a string to help the user)
		TCLAP::ValueArg<std::string> urlArg("U","url","Broker URL (with port)",true,"127.0.0.1:5672","string");
		cmd.add(urlArg);

		TCLAP::ValueArg<std::string> queueArg("Q","queue","Broker queue or topic",true,"topic://5gcarmen.examples","string");
		cmd.add(queueArg);

		TCLAP::ValueArg<int> portArg("P","listen-port","Port for the UDP communication with ms-van3t",false,49900,"int");
		cmd.add(portArg);

		TCLAP::ValueArg<std::string> interfaceArg("b","bindto","IP address of the interface to bind to. If set to '0.0.0.0' no specific interface will be used to bind the UDP socker",false,"0.0.0.0","string");
		cmd.add(interfaceArg);

		TCLAP::ValueArg<int> minsizeArg("s","min-msg-size","Set a minimum message size. All UDP messages with a smaller payload size will be discarded.",false,0,"int");
		cmd.add(minsizeArg);

		// To quickly test the transmission of quadkeys, you can use, with nc, --> echo -e "\x1b\x74\xeb\xfc\x06\xa6\xac\x38hello" >/dev/udp/localhost/49900
		// This command will relay a message with content "echo" and coordinates corresponding to a point near Trento, Italy (46.0647420,11.1586360)
		TCLAP::SwitchArg quadkeysArg("q","enable-quadkeys","When specified, the relayer expects each UDP packet to include, in the first 64 bits, a value of latitude (32 bits) followed by a value of longitude (32 bits)."
			"These values should be specified as degrees*1e7, in network byte order, when sending UDP packets to the relayer. Do not specify this option if you don't plan to add any geographical information at the beginning of each of your packets!");
		cmd.add(quadkeysArg);

		TCLAP::ValueArg<std::string> amqp_usernameArg("u","amqp-username","Username for the AMQP connection (if required)",false,"","string");
		cmd.add(amqp_usernameArg);

		TCLAP::ValueArg<std::string> amqp_passwordArg("p","amqp-password","Password for the AMQP connection (if required)",false,"","string");
		cmd.add(amqp_passwordArg);

		TCLAP::SwitchArg amqp_reconnectArg("r","amqp-reconnect","Enable automatic reconnection to the AMQP broker.");
		cmd.add(amqp_reconnectArg);

		TCLAP::SwitchArg amqp_allow_saslArg("S","amqp-sasl-auth","Enable AMQP SASL authentication.");
		cmd.add(amqp_allow_saslArg);

		TCLAP::SwitchArg amqp_allow_plainArg("I","amqp-plain-auth","Enable AMQP PLAIN authentication.");
		cmd.add(amqp_allow_plainArg);

		TCLAP::ValueArg<int> amqp_idle_timeout_msArg("t","amqp-idle-timeout","Set the AMQP connection idle timeout. Any value < 0 will keep the default Qpid Proton AMQP library setting, while a value equal to 0 means setting the idle timeout to FOREVER (i.e., disable the idle timeout).",false,-1,"int");
		cmd.add(amqp_idle_timeout_msArg);

		TCLAP::ValueArg<double> retryIntervalArg("R","retry-interval","Setting this option will make the relayer periodically retry connecting to the broker, if a connection is not possible, or if it gets disconnected. A retry interval in seconds should be specified. A value equal to 0 will make the relayer terminate with an error in case of disconnection.",false,0.0,"double");
		cmd.add(retryIntervalArg);

		cmd.parse(argc,argv);

		cam_args.m_broker_address=urlArg.getValue();
		cam_args.m_queue_name=queueArg.getValue();
		listen_port=portArg.getValue();
		bind_ip=interfaceArg.getValue();
		minimum_msg_size=minsizeArg.getValue();
		quadk_enable=quadkeysArg.getValue();

		amqp_username=amqp_usernameArg.getValue();
		amqp_password=amqp_passwordArg.getValue();
		amqp_reconnect=amqp_reconnectArg.getValue();
		amqp_allow_sasl=amqp_allow_saslArg.getValue();
		amqp_allow_plain=amqp_allow_plainArg.getValue();
		amqp_idle_timeout_ms=amqp_idle_timeout_msArg.getValue();

		retry_interval_seconds=retryIntervalArg.getValue();

		std::cout << "The relayer will connect to " + cam_args.m_broker_address + "/" + cam_args.m_queue_name << std::endl;
	} catch (TCLAP::ArgException &tclape) { 
		std::cerr << "TCLAP error: " << tclape.error() << " for argument " << tclape.argId() << std::endl;
	}

	// Create a pipe for the graceful termination of the relayer in case of errors
	int unlock_pd[2];

	if(pipe(unlock_pd)<0) {
		std::cerr << "Error: could not create the pipe for the graceful termination of the AMQP client thread." << std::endl;
		exit(EXIT_FAILURE);
	}

	// CAM relayer object
	msgrelayerAMQP msg_relayer_obj;

	// Creation of the thread
	// CAM Relayer Thread attributes
	pthread_attr_t tattr;
	// CAM Relayer Thread ID
	pthread_t curr_tid;

	// Store the "write" pipe descriptor into the msgrelayerAMQP object (to enable an easy retrieval in the AMQP client thread)
	msg_relayer_obj.setUnlockPipeDescriptorWrite(unlock_pd[1]);

	// Set the terminator flag to false
	terminatorFlag = false;

	// Set the arguments/parameters of the CAMrelayerAMQP object
	msg_relayer_obj.set_args(cam_args);

	// Set username, if specified
	if(amqp_username.length()>0) {
		msg_relayer_obj.setUsername(amqp_username);
	}
	// Set password, if specified
	if(amqp_password.length()>0) {
		msg_relayer_obj.setUsername(amqp_password);
	}
	// Set connection options
	msg_relayer_obj.setConnectionOptions(amqp_allow_sasl,amqp_allow_plain,amqp_reconnect);
	msg_relayer_obj.setIdleTimeout(amqp_idle_timeout_ms);

	// pthread_attr_init()/pthread_attr_setdetachstate()/pthread_attr_destroy() may probably be removed in the future
	// If removed, the second argument of pthread_create() should be NULL instead of &tattr
	pthread_attr_init(&tattr);
	pthread_attr_setdetachstate(&tattr,PTHREAD_CREATE_DETACHED);

	// Passing as argument, to the thread, a pointer to the CAM_relayer_obj CAMrelayerAMQP object
	// pthread_create() actually creates a new (parallel) thread, running the content of the function "CAMrelayer_callback" (which must be a void *(void *) function)
	pthread_create(&curr_tid,&tattr,msgrelayer_callback,(void *) &(msg_relayer_obj));
	pthread_attr_destroy(&tattr);

	// Wait for the sender to be open before moving on (as required and as described inside camrelayeramqp.h)
	bool sender_ready_status;

	std::cout << "Waiting for the AMQP sender to be ready..." << std::endl;

	sender_ready_status=msg_relayer_obj.wait_sender_ready(&terminatorFlag);

	std::cout << "Sender should be ready. Status (0 = error, 1 = ok): " << sender_ready_status << std::endl;

	// Create UDP socket
	int sfd = socket(AF_INET,SOCK_DGRAM,0);
	
	if(sfd<=0) {
		std::cerr << "Error: cannot create socket. Details: " << std::string(strerror(errno)) << std::endl;
		exit(EXIT_FAILURE);
	}

	// Bind UDP socket
	struct sockaddr_in address;
	address.sin_family = AF_INET;

	if(bind_ip=="0.0.0.0") {
		address.sin_addr.s_addr = INADDR_ANY;
	} else {
		if(inet_pton(AF_INET,bind_ip.c_str(),&address.sin_addr)<1) {
			std::cerr << "Error: cannot set an IP address to bind to." << std::endl;
			close(sfd);
			exit(EXIT_FAILURE);
		}
		std::cout << "Bind IP address: " << inet_ntoa(address.sin_addr) << std::endl;
	}

	address.sin_port = htons(listen_port);
	socklen_t addrlen = sizeof(struct sockaddr_in);

	// Reciving up to the maximum allowed by a MTU of 1500, when using UDP
	uint8_t buffer[1460];
	int buf_length = sizeof(buffer);

	if(bind(sfd,(struct sockaddr*) &address,addrlen)<0) {
		std::cerr << "Error: cannot bind socket. Details: " << std::string(strerror(errno)) << std::endl;
		close(sfd);
		exit(EXIT_FAILURE);
	}

	int recv_bytes;
	struct pollfd rxMon[2];

	rxMon[0].fd=sfd;
	rxMon[0].revents=0;
	rxMon[0].events=POLLIN;

	rxMon[1].fd=unlock_pd[0];
	rxMon[1].revents=0;
	rxMon[1].events=POLLIN;

	while(terminatorFlag==false) {
		if(poll(rxMon,2,INDEFINITE_BLOCK)>0) {
			// Poll unlocked via received message: parse and relay the received data
			if(rxMon[0].revents>0) {
				recv_bytes = recvfrom(sfd, buffer, buf_length, 0, NULL, NULL);

				// Discard all the received messages with a message size smaller than minimum_msg_size bytes
				if(recv_bytes < minimum_msg_size) {
					continue;
				}

				if(quadk_enable==true) {
					latlon_t curr_coordinates;
					memcpy(&curr_coordinates,(void *) buffer, sizeof(latlon_t));
					double lat = (double)((int) ntohl(curr_coordinates.lat))/1e7;
					double lon = (double)((int) ntohl(curr_coordinates.lon))/1e7;

					msg_relayer_obj.sendMessage_AMQP(((uint8_t*)buffer)+sizeof(latlon_t),((int)recv_bytes)-sizeof(latlon_t),lat,lon,18);
				} else {
					msg_relayer_obj.sendMessage_AMQP(((uint8_t*)buffer),((int)recv_bytes));
				}
			} else if(rxMon[1].revents>0) {
				std::cerr << "The UDP-AMQP relayer has terminated due to an error." << std::endl;
				// Poll unlocked via pipe: just break out of the loop
				break;
			}
		}
	}

	close(sfd);
	close(unlock_pd[0]);
	close(unlock_pd[1]);

	return 0;

}
