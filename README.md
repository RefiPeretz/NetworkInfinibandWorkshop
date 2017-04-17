#Workshop in communication networks in Hebrew university of jerusalem 2017
EX1 - See ex1.pdf for ex. instructions.

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


Important Links for InfiniBand support using Verbs API:
- Explains the structure and needed functions in comparison to TCP and PingPong example
  https://blog.zhaw.ch/icclab/infiniband-an-introduction-simple-ib-verbs-program-with-rdma-write/
  Uses TCP out of band and not CM


- Dissect of the pingpong example almost line by line: 
https://arxiv.org/pdf/1105.1827.pdf


