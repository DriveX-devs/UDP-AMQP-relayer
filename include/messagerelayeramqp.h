#ifndef MESSAGERELAYERAMQP_H
#define MESSAGERELAYERAMQP_H

#include <proton/messaging_handler.hpp>
#include <proton/container.hpp>
#include <proton/work_queue.hpp>
#include <atomic> // For std::atomic<bool>

typedef struct _pthread_camrelayer_args {
	std::string m_broker_address;
	std::string m_queue_name;
} pthread_camrelayer_args_t;

typedef struct _latlon_t {
	int32_t lat;
	int32_t lon;
} latlon_t;

class msgrelayerAMQP : public proton::messaging_handler {
	// For an example of usage of work_queue() to "inject" extra work (i.e. send CAMs) from external thread, see also:
	// http://qpid.apache.org/releases/qpid-proton-0.32.0/proton/cpp/examples/multithreaded_client.cpp.html
	pthread_camrelayer_args_t cr_arg_cl;         // AMQP and application parameters
	proton::work_queue *m_work_queue_ptr;        // Pointer to a work queue for "injecting" CAMs from an external thread
	proton::sender m_sender;                     // Sender to the CAM queue/topic
	std::atomic<bool> m_sender_ready;            // = true when the sender is ready (i.e. we can send CAMs), = false otherwise

	// Internal authentication/configuration variables
	std::string m_username;
	std::string m_password;
	bool m_reconnect=false;
	bool m_allow_sasl=false;
	bool m_allow_plain=false;
	long m_idle_timeout_ms=-1;
	int m_unlock_pd_wr=-1;

	// Qpid Proton event callbacks
	void on_container_start(proton::container& c) override;
	void on_connection_open(proton::connection& c) override;
	void on_sender_open(proton::sender& protonsender) override;
	void on_sendable (proton::sender& sndr) override;
	void on_message(proton::delivery &dlvr, proton::message &msg) override;

	public:
		// Empty constructor
		// You must call set_args just after the usage of an empty constructor, otherwise the behaviour may be undefined
		msgrelayerAMQP();

		// Full constructor (no need to call set_args() after using this constructor)
		msgrelayerAMQP(const pthread_camrelayer_args_t camrelay_args);

		void set_args(const pthread_camrelayer_args_t camrelay_args);

		// Public function to be called from any external thread to trigger the transmission of a message
		// uint8_t *buffer should contain the message bytes
		// int bufsize should contain the size, in bytes, of "buffer"
		void sendMessage_AMQP(uint8_t *buffer, int bufsize);
		void sendMessage_AMQP(uint8_t *buffer, int bufsize, const double &lat, const double &lon, const int &lev);

		// Public function to wait for the sender to be ready, before calling sendMessage_AMQP()
		// The application, after starting the container with run(), should call wait_sender_ready()
		// before attempting any call to sendMessage_AMQP(), otherwise messages may not be relayed
		bool wait_sender_ready(void);
		bool wait_sender_ready(std::atomic<bool> *terminatorFlag);

		// Set credentials/configuration options
		void setUsername(std::string username) {
			m_username=username;
		}

		void setPassword(std::string password) {
			m_password=password;
		}

		void setConnectionOptions(bool allow_sasl,bool allow_plain,bool reconnect = false) {
			m_allow_sasl=allow_sasl;
			m_allow_plain=allow_plain;
			m_reconnect=reconnect;
		}

		void setCredentials(std::string username,std::string password,bool allow_sasl,bool allow_plain,bool reconnect = false) {
			m_username=username;
			m_password=password;
			m_allow_sasl=allow_sasl;
			m_allow_plain=allow_plain;
			m_reconnect=reconnect;
		}

		void setIdleTimeout(long idle_timeout_ms) {
			m_idle_timeout_ms=idle_timeout_ms;
		}

		void setUnlockPipeDescriptorWrite(int unlock_pd_wr) {
			m_unlock_pd_wr=unlock_pd_wr;
		}

		int getUnlockPipeDescriptorWrite(void) {
			return m_unlock_pd_wr;
		}
};

#endif // MESSAGERELAYERAMQP_H
