#pragma once

#include "WebSocket.h"

class CChatServer
{
public:
	WebSocket::CAcceptor								m_webSocketAcceptor;
	std::set<std::shared_ptr<WebSocket::CConnection>>	m_setConnection;
public:
	CChatServer(boost::asio::io_service& ioService,boost::asio::ip::tcp::endpoint ep)
		:m_webSocketAcceptor(ioService,std::bind(&CChatServer::OnConnected,this,std::placeholders::_1),ep)
	{
	}

	std::pair<WebSocket::CConnection::FuncOnReceived,WebSocket::CConnection::FuncOnClosed> OnConnected(std::shared_ptr<WebSocket::CConnection> spConnect)
	{
		m_setConnection.insert(spConnect);
		boost::system::error_code ec;
		boost::asio::ip::tcp::socket::endpoint_type ep = spConnect->GetSocket().remote_endpoint(ec);
		if( !ec ){
			const std::string sJson = R"({"connected":")" + ep.address().to_string() + ":" + std::to_string(ep.port()) + R"("})";
			std::vector<std::uint8_t> v(sJson.begin(),sJson.end());
			for(auto i : m_setConnection ){
				i->Send(v);
			}
		}
		namespace ph = std::placeholders;
		return std::pair<WebSocket::CConnection::FuncOnReceived,WebSocket::CConnection::FuncOnClosed>(std::bind(&CChatServer::OnReceived,this,ph::_1,ph::_2,ph::_3),std::bind(&CChatServer::OnClosed,this,ph::_1));
	}

	void OnClosed(std::shared_ptr<WebSocket::CConnection> spConnect)
	{
		m_setConnection.erase(spConnect);
	}

	void OnReceived(std::shared_ptr<WebSocket::CConnection> spConnect,const WebSocket::CConnection::CHead &head,const std::vector<uint8_t> &vReceived)
	{
		const std::string sReceived(vReceived.begin(),vReceived.end());

		boost::system::error_code ec;
		boost::asio::ip::tcp::socket::endpoint_type ep = spConnect->GetSocket().remote_endpoint(ec);
		if( !ec ){
			boost::property_tree::ptree pt;
			try{
				std::stringstream ss(sReceived);
				boost::property_tree::read_json(ss, pt);
				if( boost::optional<std::string> value = pt.get_optional<std::string>("message") ){
					std::string sMessaeg= *value;
					const std::string sJson = R"({"message":")" + sMessaeg + R"(","from":")" + ep.address().to_string() + ":" + std::to_string(ep.port()) + R"("})";
					std::vector<std::uint8_t> v(sJson.begin(),sJson.end());
					for(auto i : m_setConnection ){
						i->Send(v);
					}
				}
			}catch(const std::exception &e){
				std::cout << e.what() << std::endl;
			}
		}
	}

};
