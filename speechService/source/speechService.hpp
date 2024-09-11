#include <brpc/server.h>
#include <butil/logging.h>

#include "speechAsr.hpp"
#include "log.hpp"
#include "etcd.hpp"
#include "speech.pb.h"

namespace hjb
{
    // 语音识别服务类
    class SpeechServiceImpl : public SpeechService
    {
    private:
        SpeechClient::ptr _client;

    public:
        SpeechServiceImpl(const SpeechClient::ptr &client)
            : _client(client)
        {
        }

        ~SpeechServiceImpl()
        {
        }

        // 语音识别业务处理
        void SpeechRecognition(google::protobuf::RpcController *cntl_base,
                               const ::hjb::SpeechReq *request,
                               ::hjb::SpeechRsp *response,
                               ::google::protobuf::Closure *done)
        {
            // 以 ARII 方式自动释放 done 对象
            brpc::ClosureGuard rpc_guard(done);

            // 从请求中获取语音数据并调用sdk进行业务处理
            std::string res = _client->recognize(request->content());
            if (res.empty())
            {
                ERROR("{}->语音识别失败！", request->requestid());

                // 编辑响应
                response->set_requestid(request->requestid());
                response->set_success(false);
                response->set_errmsg("语音识别失败");

                return;
            }

            // 编辑成功响应
            response->set_requestid(request->requestid());
            response->set_success(true);
            response->set_res(res);
        }
    };

    // 语音识别rpc服务器
    class SpeechServer
    {
    private:
        SpeechClient::ptr _speechClient;
        std::shared_ptr<brpc::Server> _brpcServer;
        EtcdRegClient::ptr _regClient;

    public:
        using ptr = std::shared_ptr<SpeechServer>;

        SpeechServer(const SpeechClient::ptr client,
                     const EtcdRegClient::ptr &regClient,
                     const std::shared_ptr<brpc::Server> &server)
            : _speechClient(client), _regClient(regClient), _brpcServer(server)
        {
        }

        ~SpeechServer()
        {
        }

        // 搭建RPC服务器并启动服务器
        void start()
        {
            // 阻塞等待服务器运行
            _brpcServer->RunUntilAskedToQuit();
        }
    };

    // 语音识别rpc服务器建造类
    class SpeechServerBuild
    {
    public:
        // 构造语音识别客户端对象
        void makeSpeechClient(const std::string &appId,
                              const std::string &apiKey,
                              const std::string &secretKey)
        {
            _speechClient = std::make_shared<SpeechClient>(appId, apiKey, secretKey);
        }

        // 构造服务注册客户端对象
        void makeRegClient(const std::string &regHost,
                           const std::string &serviceName,
                           const std::string &accessHost)
        {
            _regClient = std::make_shared<EtcdRegClient>(regHost);
            _regClient->reg(serviceName, accessHost);
        }

        // 构造RPC服务器对象
        void makeRpcServer(uint16_t port, int32_t timeout, uint8_t threads)
        {
            if (!_speechClient)
            {
                ERROR("未初始化语音识别模块");
                abort();
            }

            // 添加rpc服务
            _brpcServer = std::make_shared<brpc::Server>();
            SpeechServiceImpl *speechService = new SpeechServiceImpl(_speechClient);
            if (_brpcServer->AddService(speechService, brpc::ServiceOwnership::SERVER_OWNS_SERVICE) == -1)
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

        // 生成语音识别模块服务器
        SpeechServer::ptr build()
        {
            if (!_speechClient)
            {
                ERROR("未初始化语音识别模块");
                abort();
            }

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

            return std::make_shared<SpeechServer>(_speechClient, _regClient, _brpcServer);
        }

    private:
        SpeechClient::ptr _speechClient;
        EtcdRegClient::ptr _regClient;
        std::shared_ptr<brpc::Server> _brpcServer;
    };
}