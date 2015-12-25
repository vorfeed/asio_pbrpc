#include <asio_pbrpc/asio_pbrpc.h>
#include "rpc.pb.h"

using namespace asio_pbrpc;

int main(int argc, char* argv[]) {
  boost::asio::io_service ios;
  Executor executor;
  std::shared_ptr<AsyncRPCClient> client(std::make_shared<AsyncRPCClient>(ios, executor));
  if (!client->SyncConnect("127.0.0.1", 6666)) {
    return -1;
  }
  std::thread t([&ios] {
    boost::asio::io_service::work work(ios);
    ios.run();
  });

  OneService::Stub one_stub(&*client);

  EchoRequest echo_request;
  echo_request.set_message("one echo from async client");
  EchoResponse echo_response;
  ClientRPCController rpc_controller;
  std::cout << "async rpc client send one echo message '" << echo_request.message() <<
      "' to server" << std::endl;
  one_stub.Echo(&rpc_controller, &echo_request, &echo_response,
      google::protobuf::NewCallback<EchoResponse*>([](EchoResponse* echo_response) {
    std::cout << "async rpc client receive one echo message '" << echo_response->response() <<
        "' from server" << std::endl;
  }, &echo_response));
  client->Wait();
  if (rpc_controller.Failed()) {
    std::cerr << "async rpc client call one echo message failed: " <<
        rpc_controller.ErrorText() << std::endl;
    return -1;
  }

  DiscardRequest discard_request;
  discard_request.set_message("one discard from async client");
  DiscardResponse discard_response;
  rpc_controller.Reset();
  std::cout << "async rpc client send one discard message '" << discard_request.message() <<
      "' to server" << std::endl;
  one_stub.Discard(&rpc_controller, &discard_request, &discard_response,
      google::protobuf::NewCallback<EchoResponse*>([](EchoResponse* echo_response) {
    std::cout << "async rpc client receive one discard message from server" << std::endl;
  }, &echo_response));
  client->Wait();
  if (rpc_controller.Failed()) {
    std::cerr << "async rpc client call one discard message failed: " <<
        rpc_controller.ErrorText() << std::endl;
    return -1;
  }

  AnotherService::Stub another_stub(&*client);

  echo_request.set_message("another echo from async client");
  rpc_controller.Reset();
  std::cout << "async rpc client send another echo message '" << echo_request.message() <<
      "' to server" << std::endl;
  another_stub.Echo(&rpc_controller, &echo_request, &echo_response,
      google::protobuf::NewCallback<EchoResponse*>([](EchoResponse* echo_response) {
    std::cout << "async rpc client receive another echo message '" << echo_response->response() <<
      "' from server" << std::endl;
  }, &echo_response));
  client->Wait();
  if (rpc_controller.Failed()) {
    std::cerr << "async rpc client call another echo message failed: " <<
        rpc_controller.ErrorText() << std::endl;
    return -1;
  }

  ios.stop();
  t.join();

  return 0;
}
