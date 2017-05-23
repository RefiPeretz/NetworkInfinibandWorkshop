###Workshop in communication networks in Hebrew university of jerusalem 2017
EX1 - See ex1.pdf for ex. instructions.

Yafim Kazak, fimak, 307797191
Refael Peretz, refi950 - 305079030

Files:
Stream.cpp/.hpp - Socket wrapper class
Connector.cpp/.hpp - Socket Tcp Connect wrapper
Acceptor.cpp/.hpp - Socket Tcp Accept wrapper

MultiStreamTcpClient.cpp 	- Spawn multiple clients and simple messages to the server. Multiple sockets one thread.
The client send varying size form byte to 1 MB and from 1 socket to 10  sockets, each iteration the message splits between
the sockets. In the end the program yields CSV metrics file.
the current amount of sockets.

USEAGE: <port> <number of messages> <server name>
NonBlockingTcpServer.cpp 	- Server which is able to handle mutliclients without multithreading.
USEAGE: <server port>

MultiThreadTcpClient - Spwan multiple clients using multithreading each thread is a client which send multiple messages
to a server.
The client send varying size form byte to 1 MB and from 1 thread to 8  threads, each operates a different socket and
splits between them the data.
the sockets. In the end the program yields CSV metrics file.
USAGE: <port> <number of messages per thread> po
MultiThreadServer - A server which is able to sereve multiple clients using multithread system.
USAGE: MultiThreadTcpClient <port>

multiStreamTestRunner.c - A server/client using inifiniband hardware. When active a server across client, the client
send messages from size of byte to 1MB and form 1 QP to 10. For each iteration the QPs split between them the data
when the client terminate it yields a metrics CSV.
USAGE SERVER MODE: MultiStreamIB <Number of threads choose 1> <port> <number of QPs>
USAGE CLIENT MODE: MultiStreamIB <Number of threads choose 1> <port> <number
of QPs> <server
 to
connect to>

multithreadIB.c - A server/client using inifiniband hardware. When active a server across client, the client
send messages from size of byte to 1MB and form 1 thread to 8.Each thread is actually a whole client with 1 QOP
For each iteration the threads split between them the data we need to send.
when the client terminate it yields a metrics CSV.
USAGE SERVER MODE: multithreadIB <Number of threads choose 1> <port>
USAGE CLIENT MODE: multithreadIB <Number of threads choose 1> <port> <server
to connect to>


##Important Links for InfiniBand support using Verbs API:
- Explains the structure and needed functions in comparison to TCP and PingPong example
  https://blog.zhaw.ch/icclab/infiniband-an-introduction-simple-ib-verbs-program-with-rdma-write/
  Uses TCP out of band and not CM


- Dissect of the pingpong example almost line by line: 
https://arxiv.org/pdf/1105.1827.pdf

- Source of verbs API, the "man" folder contain DOC's & "examples" contain "golden retrievers"
https://kernel.googlesource.com/pub/scm/libs/infiniband/libibverbs/+/libibverbs-1.1.7/

- http://www.csm.ornl.gov/workshops/openshmem2013/documents/presentations_and_tutorials/Tutorials/Verbs%20programming%20tutorial-final.pdf
