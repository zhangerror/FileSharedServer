#ifndef _Z_HTTP_HPP_
#define _Z_HTTP_HPP_
#include <unordered_map>
#include <boost/algorithm/string.hpp>
#include <sstream>
#include "TcpSocket.hpp"

class HttpRequest {	
public:
	int RequestParse(TcpSocket &sock);
	
public:
	std::string z_method;
	std::string z_path;
	std::unordered_map<std::string, std::string> z_param;
	std::unordered_map<std::string, std::string> z_headers;
	std::string z_body;
	
private:
	bool RecvHeader(TcpSocket &socket, std::string &header);
	bool FirstLineParse(std::string &line);
};

int HttpRequest::RequestParse(TcpSocket &sock) {
	// 1. 接收 http 头部
	std::string header;
	if(RecvHeader(sock, header) == false) return 400;
	// 2. 对整个头部进行按照 \r\n 进行分割得到一个 list
	std::vector<std::string> header_list;
	boost::split(header_list, header, boost::is_any_of("\r\n"), boost::token_compress_on); //第四个参数决定是否保留间隔符，默认不指定即不保留
	// 3. list[0] 首行，进行首行解析
	if(FirstLineParse(header_list[0]) == false) {
		return 400;
	}
	// 4. 头部分割解析
	//key: val\r\n
	size_t pos = 0;
	for(int i=1; i<header_list.size(); ++i) {
		pos = header_list[i].find(": ");
		if(pos == std::string::npos) {
			std::cerr<<"header parse error\n";
			return false;
		}
		std::string key = header_list[i].substr(0, pos);
		std::string val = header_list[i].substr(pos+1);
		z_headers[key] = val;
	}
	// 5. 请求信息校验
	
	// 6. 接收正文
	auto it = z_headers.find("Content-Length");
	if(it != z_headers.end()) {
		std::stringstream tmp;
		tmp << it->second;
		int64_t file_len;
		tmp >> file_len;
		sock.Recv(z_body, file_len);
	}
	return 200;
}
		// & : to really fix header in function Request Parse
bool HttpRequest::RecvHeader(TcpSocket &socket, std::string &header) {
	// 1. 探测性接收大量数据
	while(1) {
		std::string tmp;
		if(socket.RecvPeek(tmp) == false) return false;
		//std::cout<<"tmp:["<<tmp<<"]\n";
		// 2. 判断是否包含整个头部 \r\n\r\n
		size_t pos = tmp.find("\r\n\r\n", 0);
		// 3. 判断当前接收的数据长度
		if(pos == std::string::npos && tmp.size() == HEADER_LEN) {
			return false;
		}
		// 4. 若包含整个头部，则直接获取头部
		else if(pos != std::string::npos) {
			size_t header_len = pos;
			socket.Recv(header, header_len);
			socket.Recv(tmp, 4);		// remove \r\n\r\n
			//std::cout<<"header : ["<<header<<"]\n";
			//std::cout<<"pos : ["<<pos<<"]\n";
			return true;
		}
	}
}
bool HttpRequest::FirstLineParse(std::string &line) {
	std::vector<std::string> first_line_list;
	boost::split(first_line_list, line, boost::is_any_of(" "), boost::token_compress_on);
	if(first_line_list.size() != 3) {
		std::cerr<<"parse first line error\n";
		return false;
	}
	z_method = first_line_list[0];
	size_t pos = first_line_list[1].find("?", 0);
	if(pos == std::string::npos) {
		z_path = first_line_list[1];
	} 
	else {
		z_path = first_line_list[1].substr(0, pos);
		std::string query_str = first_line_list[1].substr(pos+1); //key=value&key=value...
		std::vector<std::string> param_list;
		boost::split(param_list, query_str, boost::is_any_of("&"), boost::token_compress_on);

		for(auto it : param_list) {
			size_t param_pos = 0;
			param_pos = it.find("=");
			if(param_pos == std::string::npos) {
				std::cerr<<"parse param error\n";
				return false;
			}
			std::string key = it.substr(0, param_pos);
			std::string val = it.substr(param_pos+1);
			z_param[key] = val;
		}
	}

	return true;
}


class HttpResponse {
public:
	int z_status;
	std::string z_body;
	std::unordered_map<std::string, std::string> z_headers;
	
public:
	bool ErrorProcess(TcpSocket &socket) {
		return true;
	}
	bool NormalProcess(TcpSocket &socket);
	bool SetHeader(const std::string &kKey, const std::string &kVal) {
		z_headers[kKey] = kVal;
		return true;
	}
	
private:
	std::string GetDesc() {
		switch(z_status) {
		case 200 : return "OK";
		case 206 : return "Parial Content";
		case 400 : return "Bad Request";
		case 404 : return "Not Found";
		case 500 : return "Internal Server Error";
		}
		return "Unknown";
	}
};

bool HttpResponse::NormalProcess(TcpSocket &socket) {
	std::string line; 
	std::string header;
	std::stringstream tmp;

	tmp << "HTTP/1.1" << " " << z_status << " " << GetDesc();
	tmp << "\r\n";
	if(z_headers.find("Content-Length") == z_headers.end()) {
		std::string len = std::to_string(z_body.size());
		z_headers["Content-Length"] = len;
	}
	for(auto i : z_headers) {
		tmp << i.first << ": "<< i.second << "\r\n";
	}
	tmp << "\r\n";
	socket.Send(tmp.str());
	socket.Send(z_body);
	return true;
}

#endif
