# H_CCW17
Exercises from Hebrew U Workshop in communication networks

Files:
Stream.cpp/.hpp - Socket wrapper class
Connector.cpp/.hpp - Socket Tcp connect wrapper
Acceptor.cpp/.hpp - Socket Tcp Accept wrapper

MultiStreamTcpClient.cpp 	- Spawn multiple clients and simple messages to the server.
SingleStreamTcpClient.cpp - Simple tcp client with one connection.
SingleStreamTcpServer.cpp - Simple tcp server that can support once socket.
VaryingSizeTcpClient.cpp - Enables to send messages with varying increasing size.

Parts(with status):
General - Missing support for InfiniBand (QP) and measure throughput.
Part 1 - SingleStreamTcpClient / SingleStreamTcpServer
Part 2 - 
Part 3 - MultiStreamTcpClient, VaryingSizeTcpClient || Missing support for InfiniBand
Part 4 -


Important Links:
Explains the structure and needed functions in comperison to TCP and PingPong example
https://blog.zhaw.ch/icclab/infiniband-an-introduction-simple-ib-verbs-program-with-rdma-write/
##Uses TCP out of band and not CM

