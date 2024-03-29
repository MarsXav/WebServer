#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <cstdio>
#include <exception>
#include <list>
#include "locker.h"

// thread pool class, define as template class is for reuse purposes template parameter T is task class
template<typename T>
class threadpool {
public:
	threadpool(int thread_number = 8, int max_requests = 10000);
	~threadpool();
	bool append(T* request);

private:
	static void* worker(void* arg);
	void run();

private:
	// thread number
	int m_thread_number;
	// thread container, size is m_thread_number
	pthread_t *m_threads;
	//request queue, maximum num of requests
	int m_max_requests;
	// request queue
	std::list<T*> m_workqueue;
	// mutex
	locker m_queuelocker;
	// semaphore, identify if there's any tasks to be handled
	sem m_queuestat;
	// if terminate thread
	bool m_stop;
};

template<typename T>
threadpool<T>::threadpool(int thread_number, int max_requests) : //initialise members
	m_thread_number(thread_number), m_max_requests(max_requests),
	m_stop(false), m_threads(NULL) {
		if ((thread_number <= 0) or (max_requests <= 0)) {
			throw std::exception();
		}
		m_threads = new pthread_t[m_thread_number];
		if (!m_threads){
			throw std::exception();
		}
		//create (thread_number) of threads and set them as thread_detach
		for (int i = 0; i < thread_number; i++){
			printf("create the %dth thread\n", i);	

            // worker function is a static member, which does not have access to non-static members
            // arg is passed to worker so that worker may access the threadpool object
			int ret = pthread_create(m_threads + i, NULL, worker, this);
			if (ret != 0){
				delete [] m_threads;
				throw std::exception();
			}
			if (pthread_detach(m_threads[i])){ // if failed to detach, delete the thread
				delete [] m_threads;
				throw std::exception();
			}
		}
	}

template<typename T> 
threadpool<T>::~threadpool(){
	delete[] m_threads;
	m_stop = true; // terminates the thread
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
		m_queuestat.wait(); //use semaphore to detect if there are tasks to be handled
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
