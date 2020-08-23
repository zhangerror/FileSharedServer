#ifndef _Z_TCPSOCKET_HPP_
#define _Z_TCPSOCKET_HPP_
#include <iostream>
#include <string>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define HEADER_LEN	8192

class TcpSocket {
public:
	TcpSocket():z_sockfd(-1) { }
	
	int GetSocketFd() { return z_sockfd; }
	void SetSocketFd(int fd) { z_sockfd = fd; }
	
	void SetNonBlock() {
		int flag = fcntl(z_sockfd, F_GETFL, 0);
		fcntl(z_sockfd, F_SETFL, flag | O_NONBLOCK);
	}
	
	bool SocketInit(int port);
	bool Accept(TcpSocket &sock);
	bool RecvPeek(std::string& buf);
	bool Send(const std::string &kBuf);
	bool Recv(std::string &buf, int len);

	bool Close() {
		close(z_sockfd);
		return true;
	}	

private:
	int z_sockfd;
};

bool TcpSocket::SocketInit(int port) {
	z_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(z_sockfd < 0) {
		std::cerr << "socket error\n";
		return false;
	}
	
	int opt = 1;
	setsockopt(z_sockfd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(int));	/* SO_REUSEADDR、opt=1：开启地址重用选项 */
	struct sockaddr_in addr;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);
	addr.sin_family = AF_INET;
	socklen_t len = sizeof(addr);
	int ret = bind(z_sockfd, (struct sockaddr*)&addr, len);
	if(ret < 0) {
		std::cerr << "bind error\n";
		close(z_sockfd);
		return false;
	}
	ret = listen(z_sockfd, 10);
	if(ret < 0) {
		std::cerr << "listen error\n";
		close(z_sockfd);
		return false;
	}
	
	return true;
}

bool TcpSocket::Accept(TcpSocket &sock) {
	int fd;
	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);
	fd = accept(z_sockfd, (struct sockaddr*)&addr, &len);
	if(fd < 0) {
		std::cerr << "accept error\n";
		return false;
	}
	sock.SetSocketFd(fd);
	sock.SetNonBlock();
	
	return true;
}

bool TcpSocket::RecvPeek(std::string& buf) {
	buf.clear();
	char tmp[HEADER_LEN] = {0};
	int ret = recv(z_sockfd, tmp, HEADER_LEN, MSG_PEEK);
	/*	recv 返回值 大于 0：实际读出
					等于 0 ：连接断开
					小于零并且等于 EAGAIN ：成功
		没有拿到指定长度的数据，一直拿，直到拿到了指定长度的数据
	*/
	if(ret < 0) {
		if(errno == EAGAIN) return true;
		std::cerr << "recv error\n";
		return false;
	}
	buf.assign(tmp, ret);
	return true;
}

bool TcpSocket::Send(const std::string &kBuf) {
	int64_t slen = 0;
	while(slen < kBuf.size()) {
		int ret = send(z_sockfd, &kBuf[slen], kBuf.size()-slen, 0);
		if(ret < 0) {
			if(errno  == EAGAIN) {
				usleep(1000);
				continue;
			}
			std::cerr << "send error\n";
			return false;
		}
		slen += ret;
	}

	return true;
}

bool TcpSocket::Recv(std::string &buf, int len) {
	buf.resize(len);
	int nLen = 0, ret;
	while(nLen < len) {
		ret = recv(z_sockfd, &buf[0] + nLen, len - nLen, 0);
		if(ret < 0) {
			if(errno == EAGAIN) {
				usleep(1000);
				continue;
			}
			return false;
		}
		nLen += ret;
	}
	return true;
}

#endif
