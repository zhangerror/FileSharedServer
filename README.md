# FileSharedServer

- 设计简单的 HTTP 服务器，实现浏览器客户端文件上传、文件下载及文件目录浏览，并且处理断点续传
- 设计详见：http://zhangerror.top/?p=2323

# 类及接口实现

- TcpSocket：拿到一个连接后，将这个连接进行 epoll 监听，如果有事件，则抛到线程池中
- ThreadPoll：获取请求，读取数据，对头部进行解析，把整个请求及正文全部放到 Request 对象中
- HttpRequest：请求信息
- HttpResponse：响应信息
- ThreadHandler：处理接口，传入 TCP 套接字，接收头部信息，解析请求，处理请求，将结果放到实例化 Response 对象，组织 http 响应数据，将结果响应给客户端
- 线程池（ThreadTask、ThreadPool）：线程安全的队列+大量线程
- Epoll：多路转接

# Environment

- OS：CentOS7 3.10.0-862.el7.x86_64
- g++：4.8.5
编译此版代码需要在当前目录下创建目录：WWW
