#include "GbDns.h"
#include "GbMutex.h"
#include "ScopedLock.h"
#include "Conf.h"
#include "Mem.h"
#include "third-party/c-ares/ares.h"
#include "ip.h"
#include <arpa/nameser.h>
#include <netdb.h>
#include <vector>
#include <string>
#include <queue>

static ares_channel s_channel;
static pthread_t s_thread;
static bool s_stop = false;

static pthread_cond_t s_requestCond = PTHREAD_COND_INITIALIZER;
static GbMutex s_requestMtx;

static std::queue<struct DnsItem*> s_callbackQueue;
static GbMutex s_callbackQueueMtx;

template<typename T>
class AresList {
public:
	AresList();
	~AresList();

	T* getHead();
	void append(T *item);

private:
	T *m_head;
};

template<typename T> AresList<T>::AresList()
	: m_head(NULL) {
}

template<typename T> AresList<T>::~AresList() {
	while (m_head) {
		T *node = m_head;
		m_head = m_head->next;
		mfree(node, sizeof(T), "ares-item");
	}
}

template<typename T> T* AresList<T>::getHead() {
	return m_head;
}

template<typename T> void AresList<T>::append(T *node) {
	node->next = NULL;

	if (m_head) {
		T *last = m_head;
		while (last->next) {
			last = last->next;
		}
		last->next = node;
	} else {
		m_head = node;
	}
}

static void* processing_thread(void *args) {
	while (!s_stop) {
		fd_set read_fds, write_fds;
		FD_ZERO(&read_fds);
		FD_ZERO(&write_fds);

		int nfds;

		{
			ScopedLock sl(s_requestMtx);
			nfds = ares_fds(s_channel, &read_fds, &write_fds);
			if (nfds == 0) {
				// wait until new request comes in
				pthread_cond_wait(&s_requestCond, &s_requestMtx.mtx);
				continue;
			}
		}

		timeval tv;
		timeval *tvp = ares_timeout(s_channel, NULL, &tv);

		int count = select(nfds, &read_fds, &write_fds, NULL, tvp);
		int status = errno;
		if (count < 0 && status != EINVAL) {
			logError("select fail: %d", status);
			return 0;
		}

		ares_process(s_channel, &read_fds, &write_fds);
	}

	return 0;
}

bool GbDns::initialize() {
	if (ares_library_init(ARES_LIB_INIT_ALL) != ARES_SUCCESS) {
		logError("Unable to init ares library");
		return false;
	}

	if (ares_init(&s_channel) != ARES_SUCCESS) {
		logError("Unable to init ares channel");
		return false;
	}

	int optmask = ARES_OPT_FLAGS;
	ares_options options;
	memset(&options, 0, sizeof(options));

	// don't default init servers (use null values from options)
	optmask |= ARES_OPT_SERVERS;

	// lookup from hostfile & dns servers
	options.lookups = strdup("fb");
	optmask |= ARES_OPT_LOOKUPS;

	if (ares_init_options(&s_channel, &options, optmask) != ARES_SUCCESS) {
		logError("Unable to init ares options");
		return false;
	}

	// setup dns servers
	AresList<ares_addr_port_node> servers;
	for (int i = 0; i < g_conf.m_numDns; ++i) {
		ares_addr_port_node *server = (ares_addr_port_node*)mmalloc(sizeof(ares_addr_port_node), "ares-server");
		if (server == NULL) {
			logError("Unable allocate ares server");
			return false;
		}

		server->addr.addr4.s_addr = g_conf.m_dnsIps[i];
		server->family = AF_INET;
		server->udp_port = g_conf.m_dnsPorts[i];

		servers.append(server);
	}

	if (ares_set_servers_ports(s_channel, servers.getHead()) != ARES_SUCCESS) {
		logError("Unable to set ares server settings");
		return false;
	}

	// create processing thread
	if (pthread_create(&s_thread, nullptr, processing_thread, nullptr) != 0) {
		logError("Unable to create ares processing thread");
		return false;
	}

	return true;
}

void GbDns::finalize() {
	s_stop = true;

	pthread_cond_broadcast(&s_requestCond);
	pthread_join(s_thread, nullptr);

	ares_destroy(s_channel);
	ares_library_cleanup();
}

struct DnsItem {
	DnsItem(void (*callback)(GbDns::DnsResponse *response, void *state), void *state)
		: m_callback(callback)
		, m_state(state) {
	}

	std::vector<in_addr_t> m_ips;
	std::vector<std::string> m_nameservers;
	void (*m_callback)(GbDns::DnsResponse *response, void *state);
	void *m_state;
};

static void a_callback(void *arg, int status, int timeouts, unsigned char *abuf, int alen) {
	DnsItem *item = static_cast<DnsItem*>(arg);

	if (status != ARES_SUCCESS) {
		log(LOG_INFO, "dns: ares_error='%s'", ares_strerror(status));

		if (abuf == NULL) {
			logTrace(true, "dns: no abuf returned");
			s_callbackQueue.push(item);
			return;
		}
	}

	hostent *host = nullptr;
	int naddrttls = 5;
	ares_addrttl addrttls[naddrttls];
	int parse_status = ares_parse_a_reply(abuf, alen, &host, addrttls, &naddrttls);
	if (parse_status == ARES_SUCCESS) {
		for (int i = 0; i < naddrttls; ++i) {
			char ipbuf[16];
			logf(LOG_TRACE, "dns: ip=%s ttl=%d", iptoa(addrttls[i].ipaddr.s_addr, ipbuf), addrttls[i].ttl);
			item->m_ips.push_back(addrttls[i].ipaddr.s_addr);
		}
		s_callbackQueue.push(item);

		/// @todo alc free memory
	}

	if (parse_status != ARES_SUCCESS) {
		logTrace(true, "@@@@!!!!!error %s", ares_strerror(parse_status));
		s_callbackQueue.push(item);
	}
}

static void ns_callback(void *arg, int status, int timeouts, unsigned char *abuf, int alen) {
	DnsItem *item = static_cast<DnsItem*>(arg);

	if (status != ARES_SUCCESS) {
		log(LOG_INFO, "ares_error='%s'", ares_strerror(status));

		if (abuf == NULL) {
			logTrace(true, "@@@@!!!!!error");
			s_callbackQueue.push(item);
			return;
		}
	}

	hostent *host = nullptr;
	int parse_status = ares_parse_ns_reply(abuf, alen, &host);
	if (parse_status == ARES_SUCCESS) {
		for (int i = 0; host->h_aliases[i] != NULL; ++i) {
			logTrace(true, "ns[%d]='%s'", i, host->h_aliases[i]);
			item->m_nameservers.push_back(host->h_aliases[i]);
		}

		ares_free_hostent(host);
	}

//	if (parse_status == ARES_ENODATA) {
//
//	}

	if (parse_status != ARES_SUCCESS) {
		logTrace(true, "@@@@!!!!!error %s", ares_strerror(parse_status));

		if (parse_status != ARES_EDESTRUCTION) {
			logTrace(true, "@@@@!!!!!error %s", ares_strerror(parse_status));
			s_callbackQueue.push(item);
		}
		return;
	}



	s_callbackQueue.push(item);
}

void GbDns::getARecord(const char *hostname, void (*callback)(GbDns::DnsResponse *response, void *state), void *state) {
	DnsItem *item = new DnsItem(callback, state);

	ScopedLock sl(s_requestMtx);
	ares_query(s_channel, hostname, C_IN, T_A, a_callback, item);
	pthread_cond_signal(&s_requestCond);
}

void GbDns::getNSRecord(const char *hostname, void (*callback)(GbDns::DnsResponse *response, void *state), void *state) {
	DnsItem *item = new DnsItem(callback, state);

	ScopedLock sl(s_requestMtx);
	ares_query(s_channel, hostname, C_IN, T_NS, ns_callback, item);
	pthread_cond_signal(&s_requestCond);
}

void GbDns::makeCallbacks() {
	s_callbackQueueMtx.lock();
	while (!s_callbackQueue.empty()) {
		DnsItem *item = s_callbackQueue.front();
		s_callbackQueueMtx.unlock();

		DnsResponse response;
		response.m_nameservers = std::move(item->m_nameservers);
		response.m_ips = std::move(item->m_ips);

		item->m_callback(&response, item->m_state);
		delete item;

		s_callbackQueueMtx.lock();
		s_callbackQueue.pop();
	}
	s_callbackQueueMtx.unlock();
}
