#pragma once

class CSignal
	:boost::noncopyable
{
	std::mutex				m_mutex;
	std::condition_variable	m_condition;
	bool					m_bReady = false;
public:
	void SetReady()
	{
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_bReady = true;
		}
		m_condition.notify_all();
	}

	void WaitReady()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_condition.wait( lock, [this]{return m_bReady;} );
	}
};

template <class T> class CWorkerThreadT
{
	std::unique_ptr<T>								m_upClass;
	std::thread										m_thread;
	std::unique_ptr<boost::asio::io_service::work>	m_upWork;
	CSignal											m_signalDestructor;
	std::unique_ptr<boost::asio::io_service>		m_upIoService;
public:
	template <class FuncFactory> CWorkerThreadT(
			FuncFactory fFactory=[](boost::asio::io_service &ioService){
				return std::make_unique<T>(ioService);
			}
		)
		:m_upIoService(std::make_unique<boost::asio::io_service>())
	{
		std::shared_ptr<CSignal> spSignal = std::make_shared<CSignal>();
		m_thread = std::thread(
			[this,spSignal,fFactory]{
				try{
					m_upClass = fFactory(*m_upIoService);	// クラス構築
					assert( m_upClass.get() );
					spSignal->SetReady();					// 表スレッドを走らす
					m_upWork = std::make_unique<boost::asio::io_service::work>(*m_upIoService);
					m_upIoService->run();
				}catch( const std::exception &e ){
					e.what();
				}catch(...){
				}
				spSignal->SetReady();					// 表スレッドを走らす(failsafe)
				m_signalDestructor.WaitReady();			// デストラクタがコールされるまで待機
				m_upClass.reset();						// T の削除
				m_upIoService.reset();					// io_service破棄
			});
		spSignal->WaitReady();							// クラス構築が終わるまで待つ
	}

	~CWorkerThreadT()
	{
		m_upWork.reset();
		m_upIoService->post(
			[](){
				throw bool(true);		// io_service::run() 終了
			});
		m_signalDestructor.SetReady();
		m_thread.join();
	}

	T& ref()
	{
		assert( m_upClass.get() );
		return *m_upClass;
	}
};

