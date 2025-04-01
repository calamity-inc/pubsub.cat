#include <iostream>

#include <CertStore.hpp>
#include <HttpRequest.hpp>
#include <json.hpp>
#include <rsa.hpp>
#include <Server.hpp>
#include <ServerWebService.hpp>
#include <Socket.hpp>
#include <string.hpp>
#include <WebSocketMessage.hpp>

using namespace soup;

static Server serv;

struct Subscriber
{
	std::unordered_set<std::string> subscriptions;

	void subscribe(std::string&& topic)
	{
		subscriptions.emplace(std::move(topic));
	}

	void unsubscribe(const std::string& topic)
	{
		subscriptions.erase(topic);
	}

	[[nodiscard]] bool isSubscribedTo(const std::string& topic) const noexcept
	{
		return subscriptions.count(topic);
	}
};

static void broadcast(const std::string& topic, const std::string& msg)
{
	for (const auto& w : serv.workers)
	{
		if (w->type == WORKER_TYPE_SOCKET
			&& reinterpret_cast<Socket*>(w.get())->custom_data.isStructInMap(Subscriber)
			&& reinterpret_cast<Socket*>(w.get())->custom_data.getStructFromMapConst(Subscriber).isSubscribedTo(topic)
			)
		{
			ServerWebService::wsSendText(*reinterpret_cast<Socket*>(w.get()), msg);
		}
	}
}

static void handleRequest(Socket& s, HttpRequest&& req, ServerWebService&)
{
	if (req.path.substr(0, 11) == "/pub?topic=")
	{
		if (!req.body.empty())
		{
			std::string topic = req.path.substr(11);
			std::string& msg = req.body;

			std::string outgoing;
			outgoing.reserve(topic.size() + 1 + msg.size());
			outgoing.append(topic);
			outgoing.push_back(':');
			outgoing.append(msg);

			broadcast(topic, outgoing);
			return ServerWebService::send204(s);
		}
		return ServerWebService::send400(s);
	}
	return ServerWebService::sendContent(s, "See https://pubsub.cat/ for usage information.");
}

int main()
{
	auto certstore = soup::make_shared<CertStore>();
	{
		X509Certchain certchain;
		SOUP_ASSERT(certchain.fromPem(string::fromFile("cert/cert.pem")));
		certstore->add(
			std::move(certchain),
			RsaPrivateKey::fromPem(string::fromFile("cert/key.pem"))
		);
	}

	ServerWebService web_srv{ &handleRequest };
	web_srv.should_accept_websocket_connection = [](Socket&, const HttpRequest& req, ServerWebService&)
	{
		return true;
	};
	web_srv.on_websocket_message = [](WebSocketMessage& msg, Socket& s, ServerWebService&)
	{
		switch (msg.data.c_str()[0])
		{
		case '+':
			s.custom_data.getStructFromMap(Subscriber).subscribe(msg.data.substr(1));
			break;

		case '-':
			s.custom_data.getStructFromMap(Subscriber).unsubscribe(msg.data.substr(1));
			break;

		case '^':
			if (size_t topic_off = msg.data.find_first_of(':', 1); topic_off != std::string::npos)
			{
				std::string topic = msg.data.substr(1, topic_off - 1);
				broadcast(topic, msg.data.substr(1));
			}
			break;
		}
	};
	if (!serv.bind(80, &web_srv))
	{
		std::cout << "Failed to bind to port 80." << std::endl;
		return 1;
	}
	if (!serv.bindCrypto(443, &web_srv, std::move(certstore)))
	{
		std::cout << "Failed to bind to port 443." << std::endl;
		return 2;
	}
	std::cout << "Listening on ports 80 and 443." << std::endl;
	serv.run();
}
