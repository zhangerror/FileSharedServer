#ifndef _Z_EPOLL_HPP_
#define _Z_EPOLL_HPP_
#include <iostream>
#include <string>
#include <vector>
#include <sys/epoll.h>

#define MAX_EPOLL	64

class Epoll {
public:
	Epoll():z_epfd(-1) { }
	~Epoll() { }
	
	bool Init();
	bool Add(TcpSocket& socket);
	bool Delete(TcpSocket &socket);
	bool Wait(std::vector<TcpSocket> &list, int timeout=3000);
	
private:
	int z_epfd;
};

bool Epoll::Init() {
	z_epfd = epoll_create(MAX_EPOLL);
	if(z_epfd < 0) {
		std::cerr << "create epoll error\n";
		return false;
	}
	return true;
}
bool Epoll::Add(TcpSocket& socket) {
	struct epoll_event ev;
	int fd = socket.GetSocketFd();
	ev.events = EPOLLIN;
	ev.data.fd = fd;
	int ret = epoll_ctl(z_epfd, EPOLL_CTL_ADD, fd, &ev);
	if(ret < 0) {
		std::cerr << "append monitor error\n";
		return false;
	}
	
	return true;
}

bool Epoll::Delete(TcpSocket &socket) {
	int fd = socket.GetSocketFd();
	int ret = epoll_ctl(z_epfd, EPOLL_CTL_DEL, fd, NULL);
	if(ret < 0) {
		std::cerr << "remove monitor error\n";
		return true;
	}
	
	return true;
}

bool Epoll::Wait(std::vector<TcpSocket> &list, int timeout) {
	struct epoll_event evs[MAX_EPOLL];
	int nfds = epoll_wait(z_epfd, evs, MAX_EPOLL, timeout);
	if(nfds < 0) {
		std::cerr << "epoll monitor error\n";
		return false;
	}
	else if(nfds == 0) {
		//std::cerr << "epoll wait timeout\n";
		return false;
	}
	
	for(int i=0; i<nfds; ++i) {
		int fd = evs[i].data.fd;
		TcpSocket socket;
		socket.SetSocketFd(fd);
		list.push_back(socket);
	}
	
	return true;
}

#endif	
