#ifndef _Z_THREADPOOL_HPP_
#define _Z_THREADPOOL_HPP_
#include <iostream>
#include <sstream>
#include <thread>
#include <queue>
#include <vector>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#define	MAX_THREAD	16
#define MAX_QUEUE	16

typedef void (*handler_t)(int);

class ThreadTask {	
public:
	ThreadTask(int data, handler_t handler) : z_data(data), task_handler(handler) { }
	
	void TaskRun() {
		return task_handler(z_data);
	}
	
private:
	int 		z_data;			// 要处理的数据
	handler_t	task_handler;	// 数据的处理方法
};

class ThreadPool {
public:
	ThreadPool(int maxq = MAX_QUEUE, int maxt = MAX_THREAD);
	~ThreadPool();
	
	bool PoolInit();
	
	bool TaskPush(ThreadTask &thread_task);
private:
	std::queue<ThreadTask>		z_queue;
	int		z_thread_max_num;
	int		z_queue_max_num;
	pthread_mutex_t		z_mutex;
	pthread_cond_t		z_cond_pro;
	pthread_cond_t		z_cond_con;
	
private:
	void ThreadStart();
};

ThreadPool::ThreadPool(int maxq, int maxt):z_thread_max_num(maxq), z_queue_max_num(maxt) {
	pthread_mutex_init(&z_mutex, NULL);
	pthread_cond_init(&z_cond_con, NULL);
	pthread_cond_init(&z_cond_pro, NULL);
}
ThreadPool::~ThreadPool() {
	pthread_mutex_destroy(&z_mutex);
	pthread_cond_destroy(&z_cond_con);
	pthread_cond_destroy(&z_cond_pro);
}

bool ThreadPool::PoolInit() {
	for(int i=0; i<z_queue_max_num; ++i) {
		std::thread thr(&ThreadPool::ThreadStart, this);
		thr.detach();
	}
	return true;
}

bool ThreadPool::TaskPush(ThreadTask &thread_task) {
	pthread_mutex_lock(&z_mutex);
	while(z_queue.size() == z_thread_max_num) pthread_cond_wait(&z_cond_pro, &z_mutex);
	z_queue.push(thread_task);
	pthread_mutex_unlock(&z_mutex);
	pthread_cond_signal(&z_cond_con);
	return true;
}

void ThreadPool::ThreadStart() {
	while(1) {
		pthread_mutex_lock(&z_mutex);
		while(z_queue.empty()) pthread_cond_wait(&z_cond_con, &z_mutex);
		
		ThreadTask tTask = z_queue.front();
		z_queue.pop();
		pthread_mutex_unlock(&z_mutex);
		pthread_cond_signal(&z_cond_pro);
		tTask.TaskRun();
	}
}

#endif
