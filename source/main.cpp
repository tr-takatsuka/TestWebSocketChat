// TestWebSocketChat.cpp : コンソール アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"
#include "ChatServer.h"

int main(int argc, char** argv)
{
	// オプションの設計
	namespace po = boost::program_options;
	po::options_description opt("options");
	opt.add_options()
		("help,h",														"show help")
		("port,p",		po::value<uint16_t>()->default_value(11111),	"listen port");

	// argc, argv を解析して、結果をvmに格納
	po::variables_map vm;
	try {
		po::store(po::parse_command_line(argc, argv, opt), vm);
		po::notify(vm);
	}catch(std::exception& e){
		std::cout << e.what() << std::endl;
		std::cout << opt << std::endl;				// ヘルプ表示
		return 0;
	}

	if( vm.count("help") ){
		std::cout << opt << std::endl;				// ヘルプ表示
		return 0;
	}

	uint16_t nPort = vm["port"].as<uint16_t>();

	CWorkerThreadT<CChatServer> server(
		[nPort](boost::asio::io_service &ioService){
			boost::asio::ip::tcp::endpoint ep(boost::asio::ip::tcp::v4(),nPort);
			return std::make_unique<CChatServer>(ioService,ep);
		});

	static CSignal signal;
	const std::vector<int> signals={
			SIGINT,
#ifdef _MSC_VER
			SIGBREAK,
#else
			SIGHUP,SIGTERM
#endif
		};
	for( auto i : signals ){
		std::signal( i,
			[](int n){
				signal.SetReady();
			});
	}
	signal.WaitReady();

	return 0;
}

