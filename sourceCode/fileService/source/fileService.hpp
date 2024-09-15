#include <brpc/server.h>
#include <butil/logging.h>

#include "log.hpp"
#include "etcd.hpp"
#include "file.pb.h"
#include "base.pb.h"
#include "util.hpp"

namespace hjb
{
    // 语音识别服务类
    class FileServiceImpl : public FileService
    {
    private:
        std::string _storagePath;

    public:
        FileServiceImpl(const std::string &storagePath)
            : _storagePath(storagePath)
        {
            umask(0);
            mkdir(storagePath.c_str(), 0775);
            if (_storagePath.back() != '/')
                _storagePath.push_back('/');
        }

        ~FileServiceImpl()
        {
        }

        // 单文件下载
        void GetSingleFile(::google::protobuf::RpcController *cntl_base,
                           const ::hjb::GetSingleFileReq *request,
                           ::hjb::GetSingleFileResp *response,
                           ::google::protobuf::Closure *done)
        {
            // 以 ARII 方式自动释放 done 对象
            brpc::ClosureGuard rpc_guard(done);

            response->set_requestid(request->requestid());

            // 取出请求中的文件ID（单文件也就是文件名）
            std::string fid = request->fileid();
            std::string filename = _storagePath + fid;

            // 读取文件数据
            std::string body;
            if (!hjb::readFile(filename, body))
            {
                response->set_success(false);
                response->set_errmsg("读取文件数据失败");
                ERROR("{} 读取文件数据失败", request->requestid());
                return;
            }

            // 组织响应
            response->set_success(true);
            response->mutable_filedata()->set_fileid(fid);
            response->mutable_filedata()->set_filecontent(body);
        }

        // 多文件下载
        void GetMultiFile(google::protobuf::RpcController *cntl_base,
                          const ::hjb::GetMultiFileReq *request,
                          ::hjb::GetMultiFileResp *response,
                          ::google::protobuf::Closure *done)
        {
            // 以 ARII 方式自动释放 done 对象
            brpc::ClosureGuard rpc_guard(done);

            response->set_requestid(request->requestid());

            // 循环取出请求中的文件ID，读取文件数据进行填充
            for (int i = 0; i < request->fileidlist_size(); i++)
            {
                std::string fid = request->fileidlist(i);
                std::string filename = _storagePath + fid;
                std::string body;
                if (!hjb::readFile(filename, body))
                {
                    response->set_success(false);
                    response->set_errmsg("读取文件数据失败");
                    ERROR("{} 读取文件数据失败", request->requestid());
                    return;
                }

                // 获取到文件数据插入映射结构中
                FileDownloadData data;
                data.set_fileid(fid);
                data.set_filecontent(body);
                response->mutable_filedata()->insert({fid, data});
            }
            response->set_success(true);
        }

        // 单文件上传
        void PutSingleFile(google::protobuf::RpcController *cntl_base,
                           const ::hjb::PutSingleFileReq *request,
                           ::hjb::PutSingleFileResp *response,
                           ::google::protobuf::Closure *done)
        {
            // 以 ARII 方式自动释放 done 对象
            brpc::ClosureGuard rpc_guard(done);

            response->set_requestid(request->requestid());

            // 为文件生成一个唯一uudi作为文件名以及文件ID
            std::string fid = hjb::uuid();
            std::string filename = _storagePath + fid;

            // 取出请求中的文件数据进行文件数据写入
            if (!hjb::writeFile(filename, request->filedata().filecontent()))
            {
                response->set_success(false);
                response->set_errmsg("写入文件数据失败");
                ERROR("{} 写入文件数据失败", request->requestid());
                return;
            }

            // 组织响应
            response->set_success(true);
            response->mutable_fileinfo()->set_fileid(fid);
            response->mutable_fileinfo()->set_filesize(request->filedata().filesize());
            response->mutable_fileinfo()->set_filename(request->filedata().filename());
        }

        // 多文件上传
        void PutMultiFile(google::protobuf::RpcController *cntl_base,
                          const ::hjb::PutMultiFileReq *request,
                          ::hjb::PutMultiFileResp *response,
                          ::google::protobuf::Closure *done)
        {
            // 以 ARII 方式自动释放 done 对象
            brpc::ClosureGuard rpc_guard(done);

            response->set_requestid(request->requestid());

            for (int i = 0; i < request->filedata_size(); i++)
            {
                std::string fid = hjb::uuid();
                std::string filename = _storagePath + fid;

                if (!hjb::writeFile(filename, request->filedata(i).filecontent()))
                {
                    response->set_success(false);
                    response->set_errmsg("写入文件数据失败");
                    ERROR("{} 写入文件数据失败", request->requestid());
                    return;
                }

                auto info = response->add_fileinfo();
                info->set_fileid(fid);
                info->set_filesize(request->filedata(i).filesize());
                info->set_filename(request->filedata(i).filename());
            }
            response->set_success(true);
        }
    };

    class FileServer
    {
    private:
        std::shared_ptr<brpc::Server> _brpcServer;
        EtcdRegClient::ptr _regClient;

    public:
        using ptr = std::shared_ptr<FileServer>;

        FileServer(const EtcdRegClient::ptr &regClient,
                   const std::shared_ptr<brpc::Server> &server)
            : _regClient(regClient), _brpcServer(server)
        {
        }

        ~FileServer()
        {
        }

        // 搭建RPC服务器并启动服务器
        void start()
        {
            // 阻塞等待服务器运行
            _brpcServer->RunUntilAskedToQuit();
        }
    };

    class FileServerBuild
    {
    public:
        // 构造服务注册客户端对象
        void makeRegClient(const std::string &regHost,
                           const std::string &serviceName,
                           const std::string &accessHost)
        {
            _regClient = std::make_shared<EtcdRegClient>(regHost);
            _regClient->reg(serviceName, accessHost);
        }

        // 添加并启动rpc服务
        void makeRpcServer(uint16_t port, int32_t timeout, uint8_t threads)
        {
            // 添加rpc服务
            _brpcServer = std::make_shared<brpc::Server>();
            FileServiceImpl *FileService = new FileServiceImpl("./data");
            if (_brpcServer->AddService(FileService, brpc::ServiceOwnership::SERVER_OWNS_SERVICE) == -1)
            {
                ERROR("添加Rpc服务失败");
                abort();
            }

            // 启动rpc服务
            brpc::ServerOptions options;
            options.idle_timeout_sec = timeout;
            options.num_threads = threads;
            if (_brpcServer->Start(port, &options) == -1)
            {
                ERROR("服务启动失败");
                abort();
            }
        }

        FileServer::ptr build()
        {
            if (!_regClient)
            {
                ERROR("未初始化服务注册模块");
                abort();
            }

            if (!_brpcServer)
            {
                ERROR("未初始化RPC服务器模块");
                abort();
            }

            return std::make_shared<FileServer>(_regClient, _brpcServer);
        }

    private:
        EtcdRegClient::ptr _regClient;
        std::shared_ptr<brpc::Server> _brpcServer;
    };
}