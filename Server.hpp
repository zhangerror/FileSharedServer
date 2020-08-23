#ifndef _Z_SERVER_HPP_
#define _Z_SERVER_HPP_
#include "TcpSocket.hpp"
#include "Epoll.hpp"
#include "HTTP.hpp"
#include "ThreadPool.hpp"
#include <boost/filesystem.hpp>
#include <iomanip>
#include <string>
#include <algorithm>
#include <fstream>

#define URL_ROOT "./WWW"

/*	服务端的整体流程
 *	
 *	处理请求：连接，文件上传，文件下载，目录浏览
 */
class Server {
public:
	bool Start(int port);
	static void ThreadHandler(int sockfd);
	static int64_t ConvertStringToDigit(const std::string kVal);
	static bool HttpProcess(HttpRequest &req, HttpResponse &rsp);
	static bool CGIProcess(HttpRequest &req, HttpResponse &rsp);
	static bool Download(std::string &path, int64_t start, int64_t fsize, std::string &body);
	static bool RangeDownload(HttpRequest &req, HttpResponse &rsp);
	static std::string GetYMDHMSTimeStringByTimeStamp(int64_t timeStamp);
	static void ListShow(HttpRequest &req, HttpResponse &rsp);
	
private:
	TcpSocket	z_listen_sock;		/* 监听套接字 */
	ThreadPool	z_pool;				/* 线程池 */
	Epoll		z_epoll;			/* 多路转接 */
};

/* Function：	Start
 * Description：服务器启动入口
 * Input：		port  - 监听连接请求的端口号
 * Return：		false - 处理出错
 */
bool Server::Start(int port) {
	bool ret;
	ret = z_listen_sock.SocketInit(port);
	if(ret == false) return false;

	ret = z_epoll.Init();
	if(ret == false) return false;

	ret = z_pool.PoolInit();
	if(ret == false) return false;

	ret = z_epoll.Add(z_listen_sock);
	if(ret == false) return false;

	while(1) {
		std::vector<TcpSocket> list;
		ret = z_epoll.Wait(list);
		if(ret == false) continue;		/* 出错或当前没有事件触发，重新获取 */
		
		for(int i=0; i<list.size(); ++i) {
			if(list[i].GetSocketFd() == z_listen_sock.GetSocketFd()) {
				TcpSocket cli_sock;
				ret = z_listen_sock.Accept(cli_sock);
				if(ret == false) continue;		/* 当前未成功建立连接 */
				z_epoll.Add(cli_sock);
			}
			else {
				ThreadTask tTask(list[i].GetSocketFd(), ThreadHandler);
				z_pool.TaskPush(tTask);
				z_epoll.Delete(list[i]);
			}
		}
	}
	
	return true;
}

/* Function：	ThreadHandler
 * Description：业务请求处理
 * Input：		sockfd - 事件对应的描述符
 * Return：		无
 */
void Server::ThreadHandler(int sockfd) {
	TcpSocket socket;
	socket.SetSocketFd(sockfd);
	HttpRequest req;
	HttpResponse rsp;
	/* 从套接字中取出请求数据并进行解析 */
	int status = req.RequestParse(socket);
	if(status != 200) {
		/* 解析失败直接响应错误 */
		rsp.z_status = status;
		rsp.ErrorProcess(socket);
		socket.Close();
		return ;
	}

	/* 根据请求进行处理，将处理结果填充到 rsp 中，并响应给客户端 */
	HttpProcess(req, rsp);
	rsp.NormalProcess(socket);

	socket.Close();
}

/* Function：	ConvertStringToDigit
 * Description：将字符串转换为数字
 * Input：		kVal - 被转换的字符串
 * Return：		dig - 转换后的数字
 */
int64_t Server::ConvertStringToDigit(const std::string kVal) {
	std::stringstream tmp;
	tmp << kVal;
	
	int64_t dig = 0;
	tmp >> dig;
	
	return dig;
}

/* Function：	HttpProcess
 * Description：具体的业务处理
 * Input：		req - 请求信息
 * 				rsp - 响应信息
 * Return：		true - 业务处理成功
 * 				false - 业务处理失败
 */
bool Server::HttpProcess(HttpRequest &req, HttpResponse &rsp) {
	std::string realpath = URL_ROOT + req.z_path;
	if(!boost::filesystem::exists(realpath)) {
		std::cerr << "realpath: ["<<realpath<<"]\n";
		rsp.z_status = 404;
		return false;
	}
	bool ret;
	/* 文件上传 */
	if((req.z_method == "GET" && req.z_param.size() != 0) 
		|| req.z_method == "POST") {
		ret = CGIProcess(req, rsp);
		if(ret == false) {
			rsp.z_status = 500;
			return false;
		}
	}
	else {
		/* 查看目录 */
		if(boost::filesystem::is_directory(realpath)) {	
			ListShow(req, rsp);
		}
		/* 文件下载 */
		else {
			ret = RangeDownload(req, rsp);
			if(ret == false) {
				rsp.z_status = 500;
				return false;
			}
		}
	}
	
	return true;
}

/* Function：	CGIProcess
 * Description：具体的文件上传处理
 * 				请求头部通过环境变量传递给子进程，
 * 				正文通过管道传递，
 * 				需要两个管道，一个传递数据给子进程，一个从子进程拿数据
 * Input：		req - 请求信息
 * 				rsp - 响应信息
 * Return：		true  - 文件上传成功
 * 				false - 文件上传失败
 */
bool Server::CGIProcess(HttpRequest &req, HttpResponse &rsp) {
	int pipe_in[2], pipe_out[2];
	int ret = pipe(pipe_in);
	if(ret < 0) {
		std::cerr << "create pipe error\n";
		return false;
	}
	ret = pipe(pipe_out);
	if(ret < 0) {
		std::cerr << "create pipe error\n";
		close(pipe_in[0]);
		close(pipe_in[1]);
		return false;
	}
	
	int pid = fork();
	if (pid < 0) {
		return false;
	}
	else if(pid == 0) {
		close(pipe_in[0]);	/* 关闭读端：父进程读，子进程写 */
		close(pipe_out[1]);	/* 关闭写端：子进程读，父进程写 */
		dup2(pipe_in[1], 1);	/* 将写端重定向到标准输出 */
		dup2(pipe_out[0], 0);	/* 将读端重定向到标准输入 */
		setenv("METHOD", req.z_method.c_str(), 1);
		setenv("PATH", req.z_path.c_str(), 1);
		for(auto it:req.z_headers) {
			std::string str = it.first;
			std::transform(str.begin(), str.end(), str.begin(), ::toupper);
			setenv(str.c_str(), it.second.c_str(), 1);
		}
		std::string filepath = URL_ROOT + req.z_path;
		execl(filepath.c_str(), filepath.c_str(), NULL);
		exit(0);
	}
	close(pipe_in[1]);
	close(pipe_out[0]);
	
	write(pipe_out[1], &req.z_body[0], req.z_body.size());
	char buf[1024] = {0};
	while(1) {
		ret = read(pipe_in[0], buf, 1024);
		if(ret == 0) {
			break;
		}
		buf[ret] = '\0';
		rsp.z_body += buf;
	}
	close(pipe_in[0]);
	close(pipe_out[1]);
	rsp.z_status = 200;
	
	return true;
}

/* Function：	Download
 * Description：具体的、正常的文件下载处理
 * Input：		path  - 请求路径
 * 				start - 数据开始位置
 * 				fsize - 数据长度
 * 				body  - 响应主体
 * Return：		true  - 文件下载成功
 * 				false - 文件下载失败
 */
bool Server::Download(std::string &path, int64_t start, int64_t fsize, std::string &body) {
	body.resize(fsize);
	std::ifstream file(path);
	if(!file.is_open()) {
		std::cerr<<"open file error\n";
		return false;
	}
	file.seekg(start, std::ios::beg);
	file.read(&body[0], fsize);
	if(!file.good()) {
		std::cerr<<"read file error\n";
		file.close();
		return false;
	}
	file.close();
	
	return true;
}

/* Function：	RangeDownload
 * Description：具体的文件下载处理，实现断点续传
 * Input：		req - 请求信息
 * 				rsp - 响应信息
 * Return：		true  - 文件下载成功
 * 				false - 文件下载失败
 */
bool Server::RangeDownload(HttpRequest &req, HttpResponse &rsp) {
	std::string realpath = URL_ROOT + req.z_path;
	int64_t data_len = boost::filesystem::file_size(realpath);
	int64_t last_mtime = boost::filesystem::last_write_time(realpath);
	std::string etag = std::to_string(data_len) + std::to_string(last_mtime);
	auto it = req.z_headers.find("Range");
	bool ret;
	if(it == req.z_headers.end()) {		/* 正常文件下载 */
		ret = Download(realpath, 0, data_len, rsp.z_body);
		if(ret == false) return false;
		rsp.z_status = 200;
	}
	else {								/* 客户端下载中断，断点续传 */
		std::string range = it->second;
		std::string unit = "bytes=";
		size_t pos = range.find(unit);
		if(pos == std::string::npos) return false;
		pos += unit.size();
		size_t pos2 = range.find("-", pos);
		if(pos2 == std::string::npos) return false;
		std::string start = range.substr(pos, pos2-pos);
		std::string end = range.substr(pos2+1);
		std::stringstream tmp;
		int64_t dig_start, dig_end;
		dig_start = ConvertStringToDigit(start);
		if(end.size() == 0) {
			dig_end = data_len - 1;
		}
		else {
			dig_end = ConvertStringToDigit(end);
		}
		int64_t range_len = dig_end - dig_start + 1;
		Download(realpath, dig_start, range_len, rsp.z_body);
		tmp << "bytes ";
		tmp << range.substr(pos + unit.size());
		tmp << "/";
		tmp << range_len;
		rsp.SetHeader("Content-Range", tmp.str());
		
		rsp.z_status = 206;
	}
	rsp.SetHeader("Content-Type", "application/octet-stream");
	rsp.SetHeader("Accept-Ranges", "bytes");
	rsp.SetHeader("ETag", etag);
	
	return true;
}

/* Function：	GetYMDHMSTimeStringByTimeStamp
 * Description：将时间戳转换为具体时间格式显示
 * Input：		timeStamp - 需要转换的时间戳
 * Return：		转换后的时间字符串
 */
std::string Server::GetYMDHMSTimeStringByTimeStamp(int64_t timeStamp) {
    timeStamp += 28800;
    struct tm *pt;
    pt = gmtime(&timeStamp);
    char str[100];
    strftime(str, sizeof(str), "%Y-%m-%d %H:%M:%S", pt);
    std::string timeStr(str);

    return timeStr;
}

/* Function：	ListShow
 * Description：具体的目录浏览处理
 * Input：		req - 请求信息
 * 				rsp - 响应信息
 * Return：		无
 */
void Server::ListShow(HttpRequest &req, HttpResponse &rsp) {
	std::string realpath = URL_ROOT + req.z_path;
	std::string req_path = req.z_path;

	std::stringstream tmp;
	tmp << std::setiosflags(std::ios::fixed)<<std::setprecision(2);
	tmp << "<html><head><style>";
	tmp << "*{margin: 0;}";
	tmp << ".main-window {height: 100%;width : 80%; margin: 0 auto;background-color : #e0e0e0;}";
	tmp << ".Upload {position : relative;height : 8%;width : 100%;background-color : #eaeaea;text-align: center; font-family: sans-serif;}";
	tmp << ".ListShow {position : relative;width : 100%;background-color : #e0e0e0;}";
	tmp << "</style></head>";
	tmp << "<body><div class='main-window'>";
	tmp << "<div class='Upload'>";
	tmp << "<form action='/upload' method='POST' enctype='multipart/form-data'>";
	tmp << "<div class='upload-btn'><br />";
	tmp << "<input type='file' name='fileupload'>";
	tmp << "<input type='submit' name='submit'>";
	tmp << "</div>";
	tmp << "</form>";
	tmp << "</div><hr /><hr />";
	tmp << "<div class='ListShow'><ol>";

	boost::filesystem::directory_iterator begin(realpath);
	boost::filesystem::directory_iterator end;
	for(; begin != end; ++begin) {
		std::string filepath = begin->path().string();
		std::string filename = begin->path().filename().string();
		std::string url = req_path + filename;
		int64_t mtime, ssize;
		std::string show_time;
		
		if(boost::filesystem::is_directory(filepath)) {
			tmp << "<li><strong><a href='";
			tmp << url << "/" << "'>";
			tmp << filename << "/";
			tmp << "</a><br /></strong>";
			tmp << "<small> filetype: directory";
			tmp << "</small></li>";
		}
		else {
			mtime = boost::filesystem::last_write_time(begin->path());
			show_time = GetYMDHMSTimeStringByTimeStamp(mtime);
			ssize = boost::filesystem::file_size(begin->path());
			tmp << "<li><strong><a href='";
			tmp << url << "'>";
			tmp << filename<<"</a><br /></strong>";
			tmp << "<small>modified: " << show_time;
			tmp << "<br /> filetype: application-ostream " << ssize/(float)1024 << "k bytes";
			tmp << "</small></li>";
		}
	}

	tmp << "</ol></div></div></body></html>";
	rsp.z_body = tmp.str();
	rsp.z_status = 200;
	rsp.SetHeader("Content-Type", "text/html");
}

#endif
