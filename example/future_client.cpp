#include <asio_pbrpc/asio_pbrpc.h>
#include "rpc.pb.h"

using namespace asio_pbrpc;

int main(int argc, char* argv[]) {
  boost::asio::io_service ios;
  Executor executor;
  FutureRPCClient client(ios, executor);
  if (!client.SyncConnect("127.0.0.1", 6666)) {
    return -1;
  }
  std::thread t([&ios] {
    boost::asio::io_service::work work(ios);
    ios.run();
  });

  OneService::Stub one_stub(&client);

  EchoRequest echo_request;
  echo_request.set_message("one echo from future client");
  EchoResponse echo_response;
  ClientRPCController rpc_controller;
  std::cout << "future rpc client send one echo message '" << echo_request.message() <<
      "' to server" << std::endl;
  one_stub.Echo(&rpc_controller, &echo_request, &echo_response, nullptr);
  client.Wait();
  if (rpc_controller.Failed()) {
    std::cerr << "future rpc client call one echo message failed: " <<
        rpc_controller.ErrorText() << std::endl;
    return -1;
  }
  std::cout << "future rpc client receive one echo message '" << echo_response.response() <<
      "' from server" << std::endl;

  DiscardRequest discard_request;
  discard_request.set_message("one discard from future client");
  DiscardResponse discard_response;
  rpc_controller.Reset();
  std::cout << "future rpc client send one discard message '" << discard_request.message() <<
      "' to server" << std::endl;
  one_stub.Discard(&rpc_controller, &discard_request, &discard_response, nullptr);
  client.Wait();
  if (rpc_controller.Failed()) {
    std::cerr << "future rpc client call one discard message failed: " <<
        rpc_controller.ErrorText() << std::endl;
    return -1;
  }
  std::cout << "future rpc client receive one discard message from server" << std::endl;

  AnotherService::Stub another_stub(&client);

  echo_request.set_message("another echo from future client");
  rpc_controller.Reset();
  std::cout << "future rpc client send another echo message '" << echo_request.message() <<
      "' to server" << std::endl;
  another_stub.Echo(&rpc_controller, &echo_request, &echo_response, nullptr);
  client.Wait();
  if (rpc_controller.Failed()) {
    std::cerr << "future rpc client call another echo message failed: " <<
        rpc_controller.ErrorText() << std::endl;
    return -1;
  }
  std::cout << "future rpc client receive another echo message '" << echo_response.response() <<
      "' from server" << std::endl;

  ios.stop();
  t.join();

  return 0;
}
