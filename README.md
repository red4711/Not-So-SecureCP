# Not-So-SecureCP
An implementation of the program Secure Copy Protocol (SCP) which is widely used by many UNIX user to transfer files between system over a network. This program uses an implementation of a server and client where the client can request files from the server. The program fully transfers all of the data of the file to the client's system except special marker that is placed by the operating system. 

Usage: <br />
Make the binary files using the provided makefile <br />
Start the server along with a port number as argument | example: server 1234 <br />
Run the client passing the host IP, port number, and requested file/directory | example: client 127.0.0.1 1234 ~/file <br />

![DEMO] (./doc/demo.gif)