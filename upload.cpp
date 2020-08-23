#include <iostream>
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <vector>
#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

#define URL_ROOT "./WWW/"

class Boundary {
public:
	int64_t z_addr;
	int64_t z_len;
	std::string z_name;
	std::string z_filename;
};

bool GetHeader(const std::string &kKey, std::string &kVal) {
	char *ptr = getenv(kKey.c_str());
	if(ptr == NULL) return false;
	kVal = ptr;
	return true;
}

bool HeaderParse(std::string &header, Boundary &file) {
	std::vector<std::string> list;
	boost::split(list, header, boost::is_any_of("\r\n"), boost::token_compress_on);
	
	for(int i=0; i<list.size(); ++i) {
		std::string sep = ": ";
		size_t pos = list[i].find(sep);
		if(pos == std::string::npos) return false;
		
		std::string key = list[i].substr(0, pos);
		std::string val = list[i].substr(pos+sep.size());
		if(key != "Content-Disposition") continue;
		
		std::string name_field = "fileupload";
		std::string filename_sep = "filename=\"";
		pos = val.find(name_field);
		if(pos == std::string::npos) continue;
		pos = val.find(filename_sep);
		if(pos == std::string::npos) return false;
		pos += filename_sep.size();
		size_t n_pos = val.find("\"", pos);
		if(n_pos == std::string::npos) return false;
		
		file.z_filename = val.substr(pos, n_pos-pos);
		file.z_name = "fileupload";
	};
	
	return true;
}

bool BoundaryParse(std::string &body, std::vector<Boundary> &list) {
	std::string boundary_helper = "boundary=";
	std::string tmp;
	if(GetHeader("CONTENT-TYPE", tmp) == false) return false;

	/*	每次pos指向的都是头部的起始位置或者数据的起始位置
		每次n_pos指向的都是头部的结尾位置或数据的结尾位置 */
	size_t pos, n_pos;
	pos = tmp.find(boundary_helper);
	if(pos == std::string::npos) return false;
	
	std::string boundary = tmp.substr(pos + boundary_helper.size());
	std::string dash = "--";
	std::string craf = "\r\n";
	std::string tail = "\r\n\r\n";
	std::string f_boundary = dash + boundary + craf;
	std::string m_boundary = craf + dash + boundary;
	
	pos = body.find(f_boundary);
	if(pos != 0) {	/* 如果当前 boundary 的位置不是起始位置 */
		std::cerr << "first boundary error\n";
		return false;
	}
	pos += f_boundary.size();	/* 指向第一块头部起始位置 */
	while(pos < body.size()) {
		n_pos = body.find(tail, pos);	/* 找寻头部结尾 */
		if(n_pos == std::string::npos) return false;

		std::string header = body.substr(pos, n_pos - pos);
		
		pos = n_pos + tail.size();	/* 数据的起始地址 */
		/* 找\r\n--boundary，即数据的结束位置，n_pos 移至\r\n */
		n_pos = body.find(m_boundary, pos);	
		if(n_pos == std::string::npos) return false;

		int64_t offset = pos;
		/* 下一个 boundary 的起始地址，数据的起始地址 */
		int64_t length = n_pos - pos;
		n_pos += m_boundary.size();		/* 移至 \r\n */
		pos = body.find(craf, n_pos);
		if(pos == std::string::npos) return false;

		/* pos 指向下一个 m_boundary 的头部起始地址 */
		/* 若没有 m_boundary 了，则指向数据的结尾 pos = body.size(); */
		pos += craf.size();	
		
		Boundary file;
		file.z_len = length;
		file.z_addr = offset;
		
		/* 解析头部 */
		bool ret = HeaderParse(header, file);
		if(ret == false) return false;
		list.push_back(file);
	}
	return true;
}

bool StorageFile(std::string &body, std::vector<Boundary> &list) {
	for(int i=0; i<list.size(); ++i) {
		if(list[i].z_name != "fileupload") continue;
		std::string realpath = URL_ROOT + list[i].z_filename;
		std::ofstream file(realpath);
		if(!file.is_open()) {
			std::cerr<<"open file "<<realpath<<" failed\n";
			return false;
		}
		file.write(&body[list[i].z_addr], list[i].z_len);
		if(!file.good()) {
			std::cerr<<"write file error\n";
			return false;
		}
		file.close();
	}
	return true;
}		

int main(int argc, char *argv[], char *env[]) {
	std::string body;
	char *cont_len = getenv("CONTENT-LENGTH");
	std::string fail = "<html><div style='height: 10%; margin: 0 auto;text-align: center;'>--- <b>Failed</b> ---</div></html>";
	std::string suc = "<html><div style='height: 10%; margin: 0 auto;text-align: center;'>--- <b>Success</b> ---</div></html>";
	if(cont_len != NULL) {
		std::stringstream tmp;
		tmp << cont_len;
		int64_t fsize;
		tmp >> fsize;
		
		body.resize(fsize);
		int rlen = 0, ret = 0;
		while(rlen < fsize) {
			ret = read(0, &body[0], fsize-rlen);
			if(ret <= 0) exit(-1);
			rlen += ret;
		}
		
		std::vector<Boundary> list;
		ret = BoundaryParse(body, list);
		if(ret == false) {
			std::cerr<<"boundary parse error\n";
			std::cout<<fail;
			return -1;
		}
		
		ret = StorageFile(body, list);
		if(ret == false) {
			std::cerr<<"storage parse error\n";
			std::cout<<fail;
			return -1;
		}
		std::cout<<suc;
	}
	
	return 0;
}