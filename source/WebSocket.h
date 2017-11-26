#pragma once

#include "Thread.h"

namespace WebSocket
{
namespace asio = boost::asio;

inline std::string GetBase64(const std::vector<uint8_t>& data)
{
	std::vector<uint8_t> buf(data);
	const size_t number_of_padd_chars = (buf.size() % 3 != 0) ? 3 - (buf.size() % 3) : 0;
	buf.insert(buf.end(), number_of_padd_chars, '\0');
	typedef boost::archive::iterators::base64_from_binary<
				boost::archive::iterators::transform_width<
					const uint8_t*,6,8
				>
			>
		base64_iterator;
	base64_iterator begin(&buf[0]), end(&buf[0] + data.size()); 
	std::string result(begin, end);
	result.insert(result.end(), number_of_padd_chars, '=');
	return result;
}

inline std::array<uint8_t,20> GetSha1(const std::string& s)
{
	boost::uuids::detail::sha1 cipher;
	cipher.process_bytes(s.c_str(), s.length());
	uint32_t digest[5]={};
	cipher.get_digest(digest);
	std::array<uint8_t,20> result;
	for( size_t i=0; i < result.size(); ++i){
		result[i] = static_cast<uint8_t>( digest[i >> 2] >> 8 * (3 - (i & 0x03)) );
	}
	return result;
}

class CAcceptor;

class CConnection final
{
	friend class CAcceptor;
public:
	struct CHead{
		uint8_t					Fin			: 1;
		uint8_t					Rsv1		: 1;
		uint8_t					Rsv2		: 1;
		uint8_t					Rsv3		: 1;
		uint8_t					Opcode		: 4;
		uint8_t					Mask		: 1;
		uint64_t				PayloadLength;
		std::array<uint8_t,4>	MaskingKey;
	};
	typedef std::function<void (std::shared_ptr<CConnection>,const CConnection::CHead&,const std::vector<uint8_t>&)> FuncOnReceived;
	typedef std::function<void (std::shared_ptr<CConnection>)> FuncOnClosed;
private:
	asio::ip::tcp::socket			m_socket;
	asio::strand					m_strand;
	std::list<std::vector<uint8_t>>	m_queueWrite;
	FuncOnReceived					m_fOnReceived;
	FuncOnClosed					m_fOnClosed;
private:
	CConnection(asio::ip::tcp::socket &socket,const asio::strand &strand)
		:m_socket(std::move(socket))
		,m_strand(strand)
	{
	}
public:
	const asio::ip::tcp::socket& GetSocket()const
	{
		return m_socket;
	}

	void Send(const std::vector<uint8_t> &vPayload)
	{

		CHead h = {0,};
		h.Fin = 1;
		h.Opcode = 1; 			
		h.PayloadLength = vPayload.size();

		std::vector<uint8_t> v;
		{
			uint8_t t = 0;
			t |= h.Fin  ? 0x80 : 0;
			t |= h.Rsv1 ? 0x40 : 0;
			t |= h.Rsv2 ? 0x20 : 0;
			t |= h.Rsv3 ? 0x10 : 0;
			t |= h.Opcode;
			v.push_back(t);
		}
		{
			uint8_t t = 0;
			t |= h.Mask ? 0x80 : 0;
			if( h.PayloadLength <= 125){
				t |= h.PayloadLength;
				v.push_back(t);
			}else{
				int byte = 0;
				if( h.PayloadLength <= 0xffff ){	// 16bit で PayloadLength
					t |= 126;
					byte = 2;
				}else{								// 64bit で PayloadLength
					t |= 127;
					byte = 8;
				}
				v.push_back(t);
				uint64_t n = h.PayloadLength;
				const size_t pos = v.size();
				for(int i=0; i<byte; i++ ){
					v.insert( v.begin()+pos, n & 0xff );
					n /= 0x100;
				}
			}
		}

		v.insert( v.end(), vPayload.begin(), vPayload.end() );

		asio::spawn(m_strand,
			[this,v](asio::yield_context yield){
				try{
					m_queueWrite.push_back(v);
					if( m_queueWrite.size()<=1 ){
						do{
							asio::async_write(m_socket, asio::buffer(m_queueWrite.front()), yield );
							m_queueWrite.pop_front();
						}while( !m_queueWrite.empty() );
					}
				}catch(...){
				}
			});

	}

};


class CAcceptor
{
private:
	const std::shared_ptr<bool>	m_spAlive;
	asio::ip::tcp::acceptor		m_acceptor;
public:
	typedef std::function<std::pair<CConnection::FuncOnReceived,CConnection::FuncOnClosed> (std::shared_ptr<CConnection>)> FuncOnConnected;
public:
	const asio::ip::tcp::acceptor& GetAcceptor()const
	{
		return m_acceptor;
	}

	~CAcceptor()
	{
		m_acceptor.close();
	}

	CAcceptor( asio::io_service &ioService,FuncOnConnected fOnConnected,asio::ip::tcp::endpoint ep=asio::ip::tcp::endpoint(asio::ip::tcp::v4(),18018))
		:m_spAlive(std::make_shared<bool>(true))
		,m_acceptor(ioService)
	{
		try{
			m_acceptor.open( asio::ip::tcp::v4() );
			m_acceptor.bind( ep );
			m_acceptor.listen( asio::ip::tcp::socket::max_connections );

			asio::spawn(ioService,
				[this,&ioService,fOnConnected,wpAlive=std::weak_ptr<bool>(m_spAlive)](asio::yield_context yield){
					while( !wpAlive.expired() ){
						auto spSocket(std::make_shared<asio::ip::tcp::socket>(ioService));
						boost::system::error_code ec;
						m_acceptor.async_accept( *spSocket, yield[ec] );
						if( !ec ){
							asio::strand strand(spSocket->get_io_service());
							asio::spawn(strand,
								[&ioService,fOnConnected,spSocket,strand,wpAlive](asio::yield_context yield){
									std::shared_ptr<CConnection> spConnection(new CConnection(*spSocket,strand));
									const std::weak_ptr<CConnection> wpConnection(spConnection);

									try{
										asio::streambuf streambuf;
										auto fRead = [&streambuf,wpConnection,yield](size_t readsize)
											{
												const std::shared_ptr<CConnection> sp(wpConnection);
												if( !sp ) throw std::runtime_error( std::string("destroyed CConnection") );
												std::vector<uint8_t> v;
												if( readsize > streambuf.size() ){
													asio::async_read(sp->m_socket, streambuf, asio::transfer_at_least( readsize - streambuf.size() ), yield );
												}
												auto b = asio::buffers_begin(streambuf.data());
												std::copy( b, b+readsize, std::back_inserter(v) );
												streambuf.consume(readsize);
												return v;
											};

										// リクエストヘッダ読み込み
										const size_t size = asio::async_read_until(spConnection->m_socket, streambuf, "\r\n\r\n", yield );
										const std::string sHeader( asio::buffer_cast<const char*>(streambuf.data()), size );
										streambuf.consume(size);

										std::smatch match;
										// Content-Length 取得
										const int contentLength = std::regex_search( sHeader, match, std::regex("\\nContent-Length: (\\d+)\r",std::regex_constants::icase)) ? std::stoi(match[1].str()) : 0;
										// リクエストボディ読み込み
										const std::vector<uint8_t> vBody = fRead(contentLength);
										// Connection: Upgrade 確認
										const bool bConnectionUpgrade = std::regex_search( sHeader, match, std::regex("\\nConnection: .*\\bUpgrade\\b.*\r",std::regex_constants::icase) );
										// Upgrade: websocket 確認
										const bool bUpgradeWebSocket = std::regex_search( sHeader, match, std::regex("\\nUpgrade: .*\\bwebsocket\\b.*\r",std::regex_constants::icase) );
										// Sec-WebSocket-Key
										const std::string sWebSocketKey = std::regex_search( sHeader, match, std::regex("\\nSec-WebSocket-Key: ([a-zA-Z0-9/+]+=*)\r",std::regex_constants::icase)) ? match[1].str() : "";

										// WebSocketリクエスト以外はエラー
										if( !bConnectionUpgrade || !bUpgradeWebSocket || sWebSocketKey.empty() ){
											const std::string res = "HTTP/1.1 400 Bad Request\r\n\r\nBad Request";
											asio::async_write(spConnection->m_socket, asio::buffer(res), yield );
											throw std::runtime_error( std::string(res) );
										}

										{// レスポンス送信
											const auto sha1 = GetSha1( sWebSocketKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11" );
											const std::string sAccept = GetBase64(std::vector<uint8_t>(sha1.begin(),sha1.end()));
											const std::string res = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + sAccept + "\r\n\r\n";
											asio::async_write(spConnection->m_socket, asio::buffer(res), yield );
										}

										// 接続完了通知 (コルーチン内からではなく表コンテキストからコールする)
										ioService.post(
											[fOnConnected,spConnection,wpAlive=wpAlive](){
												if( !wpAlive.expired() ){
													auto pairFuncs = fOnConnected(spConnection);
													spConnection->m_fOnReceived = pairFuncs.first;
													spConnection->m_fOnClosed = pairFuncs.second;
												}
											});
										spConnection.reset();			// ここで内部的な所有権は破棄

										while( !wpConnection.expired() ){

											CConnection::CHead head = {0,};

											{// FIN:1 RSV1:1 RSV2:1 RSV3:1 Opcode:4
												uint8_t t = fRead(1)[0];
												head.Fin    = (t & 0x80) != 0;
												head.Rsv1   = (t & 0x40) != 0;
												head.Rsv2   = (t & 0x20) != 0;
												head.Rsv3   = (t & 0x10) != 0;
												head.Opcode =  t & 0x0f;
											}

											{// Mask:1 PayloadLength:7
												uint8_t t = fRead(1)[0];
												head.Mask = (t & 0x80) != 0;
												std::vector<uint8_t> v = { static_cast<uint8_t>( t & 0x7f ) };
												switch( v[0] ){
												case 126: v=fRead(2); break;	// 次の 16bit が PayloadLength
												case 127: v=fRead(8); break;	// 次の 64bit が PayloadLength
												}
												head.PayloadLength = 0;
												for( auto i : v ){
													head.PayloadLength *= 0x100;
													head.PayloadLength += i;
												}
											}

											// Masking Key
											for( auto &i : head.MaskingKey ){
												i = fRead(1)[0];
											}

											// PayloadData
											const std::vector<uint8_t> vPayload = [&](){
													std::vector<uint8_t> v = fRead( static_cast<size_t>(head.PayloadLength) );
													size_t k = 0;
													for( auto &i : v ) i = i ^ head.MaskingKey[ (k++) % head.MaskingKey.size() ];
													return v;
												}();

											// 接続完了通知 (コルーチン内からではなく表コンテキストからコールする)
											ioService.post(
												[wpConnection,head,vPayload](){
													if( const std::shared_ptr<CConnection> sp = wpConnection.lock() ){
														if( sp->m_fOnReceived ){
															sp->m_fOnReceived(sp,head,vPayload);		// 通知
														}
													}
												});

										}//while

									}catch(std::regex_error& e){
										std::cout << e.what() << ":" << e.code() << std::endl;
									}catch(std::exception& e){
										std::cout << e.what() << std::endl;
									}catch(...){
									}

									if( const std::shared_ptr<CConnection> sp = wpConnection.lock() ){
										sp->m_socket.close();
									}

									// closed通知 (コルーチン内からではなく表コンテキストからコールする)
									ioService.post(
										[wpConnection](){
											if( const std::shared_ptr<CConnection> sp = wpConnection.lock() ){
												if( sp->m_fOnClosed ){
													sp->m_fOnClosed(sp);			// 通知
												}
											}
										});

								});
						}
					}//while
				});
		}catch(boost::system::system_error &ec){
			auto s = ec.what();
			throw ec;
		}
	}

};

}
