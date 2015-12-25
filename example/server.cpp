#include <asio_pbrpc/asio_pbrpc.h>
#include "rpc.pb.h"

using namespace asio_pbrpc;

class OneServiceImpl : public asio_pbrpc::OneService {
 public:
  OneServiceImpl() {}

  void Echo(::google::protobuf::RpcController* controller,
      const ::asio_pbrpc::EchoRequest* request,
      ::asio_pbrpc::EchoResponse* response,
      ::google::protobuf::Closure* done) override {
    std::cout << "one service received echo message: " << request->message() << std::endl;
    response->set_response(request->message());
    done->Run();
  }

  void Discard(::google::protobuf::RpcController* controller,
      const ::asio_pbrpc::DiscardRequest* request,
      ::asio_pbrpc::DiscardResponse* response,
      ::google::protobuf::Closure* done) override {
    std::cout << "one service received discard message: " << request->message() << std::endl;
    done->Run();
  }
};

class AnotherServiceImpl : public asio_pbrpc::AnotherService {
 public:
  AnotherServiceImpl() {}

  void Echo(::google::protobuf::RpcController* controller,
      const ::asio_pbrpc::EchoRequest* request,
      ::asio_pbrpc::EchoResponse* response,
      ::google::protobuf::Closure* done) override {
    std::cout << "another service received echo message: " << request->message() << std::endl;
    response->set_response(request->message());
    done->Run();
  }
};

static std::promise<void> event;

int main(int argc, char* argv[]) {
  RPCServer server(6666);
  server.RegisterService(std::make_shared<OneServiceImpl>());
  server.RegisterService(std::make_shared<AnotherServiceImpl>());
  server.Start();
  signal(SIGPIPE, SIG_IGN);
  signal(SIGINT, [](int signal) { event.set_value(); });
  event.get_future().get();
  server.Stop();
  return 0;
}
