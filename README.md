# Asio Protobuf RPC

A C++ RPC Library implementation with Protobuf and Boost Asio

Include Blocking Client, Async Client and Async Future Client


## Features

Blocking Client

* Use a SyncSend and SyncReceive, hang on asio write_some and read_some

Async Client

* Use a AsyncSend and AsyncReceive, implemented by asio async_write_some and async_read_some

Async Future Client

* Will get a future after calling asio async_write_some and async_read_some, wait until communication finished

RPC Server

* An Async Server implemented by asio, support multiple services

* A message include three parts: a message length, a method id and a protobuf message


## Example

Client

* create a RPC client

```c++
boost::asio::io_service ios;
Executor executor;
FutureRPCClient client(ios, executor);
```

* connect to server, async is also supported

```c++
if (!client.SyncConnect("127.0.0.1", 6666)) {
  return -1;
}
```

* start workers

```c++
std::thread t([&ios] {
  boost::asio::io_service::work work(ios);
  ios.run();
});
```

* make a RPC call

```c++
OneService::Stub one_stub(&client);
EchoRequest echo_request;
echo_request.set_message("one echo from future client");
EchoResponse echo_response;
ClientRPCController rpc_controller;
one_stub.Echo(&rpc_controller, &echo_request, &echo_response, nullptr);
```

* wait and check return state

```c++
client.Wait();
if (rpc_controller.Failed()) {
  return -1;
}
```

* release

```c++
ios.stop();
t.join();
```

Server

* extern google protobuf service

```c++
class OneServiceImpl : public asio_pbrpc::OneService {
 public:
  OneServiceImpl() = default;

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
```

* create a RPC server

```c++
RPCServer server(6666);
```

* register multiple services

```c++
server.RegisterService(std::make_shared<OneServiceImpl>());
server.RegisterService(std::make_shared<AnotherServiceImpl>());
```

* start server

```c++
server.Start();
```

* when finished, you can stop the server

```c++
server.Stop();
```


## Dependings

* cmake

* boost

* protobuf


================================
by Xiaojie Chen (swly@live.com)

