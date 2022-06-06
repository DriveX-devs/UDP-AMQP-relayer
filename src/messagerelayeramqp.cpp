#include <proton/connection.hpp>
#include <proton/delivery.hpp>
#include <proton/message.hpp>
#include <proton/tracker.hpp>
#include <proton/connection_options.hpp>
#include <proton/reconnect_options.hpp>

#include "messagerelayeramqp.h"
#include "sample_quad_final.h"

#include <iostream>
#include <unistd.h>

bool msgrelayerAMQP::wait_sender_ready(void) {
	// We should not need any mutex lock, as m_sender_ready is defined as std::atomic<bool>
	// Waiting for the sender to become ready, i.e. waiting for m_sender_ready to become "true"
	while(!m_sender_ready);

	return m_sender_ready;
}

void msgrelayerAMQP::sendMessage_AMQP(uint8_t *buffer, int bufsize,const double &lat, const double &lon, const int &lev) {
    QuadKeys::QuadKeyTS tilesys;
	const double latitude = lat;
	const double longitude = lon;

	proton::message msg;
	tilesys.setLevelOfDetail(lev);
	std::string quadkeys = tilesys.LatLonToQuadKey(latitude,longitude);
	msg.properties().put("quadkeys", quadkeys);

	// "Inject" the work of sending a new message with the current sender (m_sender)
	// Checking m_work_queue_ptr!=NULL just for additional safety
	if(m_work_queue_ptr!=NULL) {
		// Create the AMQP message from the buffer
		msg.body(proton::binary(buffer,buffer+bufsize));

		// Add the work of sending the message via m_sender
		m_work_queue_ptr->add([=]() {m_sender.send(msg);});
	}
}

void msgrelayerAMQP::sendMessage_AMQP(uint8_t *buffer, int bufsize) {
	proton::message msg;

	// "Inject" the work of sending a new message with the current sender (m_sender)
	// Checking m_work_queue_ptr!=NULL just for additional safety
	if(m_work_queue_ptr!=NULL) {
		// Create the AMQP message from the buffer
		msg.body(proton::binary(buffer,buffer+bufsize));

		// Add the work of sending the message via m_sender
		m_work_queue_ptr->add([=]() {m_sender.send(msg);});
	}
}

msgrelayerAMQP::msgrelayerAMQP(const pthread_camrelayer_args_t camrelay_args) :
	cr_arg_cl(camrelay_args), m_work_queue_ptr(NULL), m_sender_ready(false) {}

msgrelayerAMQP::msgrelayerAMQP() :
	m_work_queue_ptr(NULL), m_sender_ready(false) {}

void msgrelayerAMQP::set_args(const pthread_camrelayer_args_t camrelay_args) {
	cr_arg_cl=camrelay_args;
}

void msgrelayerAMQP::on_container_start(proton::container& c) {
	proton::connection_options co;
	bool co_set=false;

	if(!m_username.empty()) {
		co.user(m_username);
		co_set = true;

		std::cout<<"AMQP username successfully set: "<<m_username<<std::endl;
	}

	if(!m_password.empty()) {
		co.password(m_password);
		co_set = true;

		std::cout<<"AMQP password successfully set."<<std::endl;
	}

	if(m_reconnect == true) {
		co.reconnect(proton::reconnect_options());
		co_set = true;

		std::cout<<"AMQP automatic reconnection enabled."<<std::endl;
	}

	if(m_allow_sasl == true) {
		co.sasl_enabled(true);
		co_set = true;

		std::cout<<"AMQP SASL authentication enabled."<<std::endl;
	}

	if(m_allow_plain == true) {
		co.sasl_allow_insecure_mechs(true);
		co_set = true;

		std::cout<<"AMQP Plain authentication enabled."<<std::endl;
	}

	if(m_idle_timeout_ms>=0) {
		if(m_idle_timeout_ms==0) {
			co.idle_timeout(proton::duration::FOREVER);

			std::cout<<"Idle timeout set to FOREVER."<<std::endl;
		} else {
			co.idle_timeout(proton::duration(m_idle_timeout_ms));

			std::cout<<"Idle timeout set to "<<m_idle_timeout_ms<<"."<<std::endl;
		}

		co_set = true;
	} else {
		std::cout<<"No idle timeout has been explicitely set."<<std::endl;
	}
	
	if(co_set == true) {
		std::cout << "Connecting to the AMQP broker with user-defined connection options." << std::endl;
		c.connect(cr_arg_cl.m_broker_address,co);
	} else {
		std::cout << "Connecting to the AMQP broker with default connection options." << std::endl;
		c.connect(cr_arg_cl.m_broker_address);
	}

	// Old code - kept here just for reference
	// c.connect(cr_arg_cl.m_broker_address,co.idle_timeout(proton::duration::FOREVER));
	// c.connect(cr_arg_cl.m_broker_address,co.idle_timeout(proton::duration(1000)));
}

void msgrelayerAMQP::on_connection_open(proton::connection& c) {
	c.open_sender(cr_arg_cl.m_queue_name);
}

void msgrelayerAMQP::on_sender_open(proton::sender& protonsender) {
	m_sender=protonsender;

	// Get the work queue pointer out of the sender
	m_work_queue_ptr=&m_sender.work_queue();

	// Set "m_sender_ready" to true -> now the sender is ready and the application can safely call sendMessage_AMQP()
	m_sender_ready=true;
}

// This function basically does nothing -> the std::cout can be optionally enabled to print some debug information
void msgrelayerAMQP::on_sendable(proton::sender &s) {
    //std::cout<<"Credit left: "<<s.credit()<<std::endl;
}

// This function basically does nothing other than printing "on_message" -> you can enable the "on_message" printing for debug purposes by decommenting the content of the function
void msgrelayerAMQP::on_message(proton::delivery &dlvr, proton::message &msg) {
	//std::cout<<"on_message: "<<std::endl;
}
