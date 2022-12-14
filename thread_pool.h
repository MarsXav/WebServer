#ifndef THREADPOOL_H
#define THREADPOOL

#include <pthread.h>
#include <cstdio>
#include <exception.h>
#include <list>
#include "locker.h"

// thread pool class, define as template class is to ensure the code reuse, template parameter T is task class
template<typename T>
class threadpool {
public:
	threadpool(int thread_number = 8, int max_requests = 10000);
	~threadpool();
	bool append(T* request);
private:
	static void* work(void* arg);
	void run();

private:
	// thread amount
	int m_thread_number;
	// thread container, size is m_thread_number
	pthread_t *m_threads;
	//request queue, maximum num of requests
	int m_max_requests;
	// request list 
	std::list<T*> m_workqueue;
	// mutex
	locker m_mqueuelocker;
	// semaphore, identify if there's any tasks to be handled
	sem m_queuestat;
	// if determinate thread
	bool m_stop;
};

template<typename T>
threadpool<T>::threadpool(int thread_number, int max_requests) :
	m_thread_number(thread_number), m_max_request(max_requests),
	m_stop(false), m_threads(NULL) {
		if ((thread_number <= 0) or (max_requests <= 0)) {
			throw std::exception();
		}
		m_threads = new pthread_t[m_thread_number];
		if (!m_threads){
			throw std::exception();
		}
		//create thread_Number threads and set them as thread_detach
		for (int i = 0; i < thread_number; i++){
			printf("create the %dth thread\n", i);	

			int ret = pthread_create(m_threads + i, NULL, work, this);
			if (ret != 0){
				delete [] m_threads;
				throw std::exception;
			}
			if (pthread_detach(m_thread[i])){
				delete [] m_threads;
				throw std::exception;
			}
		}
	}

template<typename T> 
threadpool<T>::~threadpool(){
	delete[] m_threads;
	m_stop = true;
}

template<typename T>
bool threadpool<T>:: append(T* request){
	m_queuelocker.lock();
	if (m_workqueue.size() > m_max_requests){
		m_queuelocker.unlock();
		return false;
	}

	m_workqueue.push_back(request);
	m_queuelocker.unlock();
	m_queuestat.post();
	return true;
}

template<typename T>
void* threadpool<T>::worker(void* arg){
	threadpool *pool = (threadpool *)arg;
	pool->run();
	return pool;
}

template<typename T>
void threadpool<T>::run(){
	while(!m_stop){
		m_queuestat.wait();
		m_queuelocker.lock();
		if (m_workqueue.empty()){
			m_queuelocker.unlock();
			continue;
		}
		T* request = m_workqueue.front();
		m_workqueue.pop_front();
		m_queuelocker.unlock();

		if (!request){
			continue;	
		}

		request -> process();
	}
}

#endif
