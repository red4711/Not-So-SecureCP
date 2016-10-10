#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <wait.h>

#define PROTOCOL_SIZE 255

void error_exit(char *mgs){
	perror(mgs);
	exit(1);
}

void read_all(int fd, void *buffer, long long file_size){
	int rt;
	void *ptr = buffer;
	while(file_size > 0){
		if ((rt = read(fd, ptr, file_size)) < 0)
			error_exit("ERROR : read_all");
		if (rt == 0)
			return;
		file_size -= rt;
		ptr += rt;
	}
}

void write_all(int fd, void *buffer, long long buffer_size){
	int rt;
	void *ptr = buffer;
	while(buffer_size > 0){
		if ((rt = write(fd, ptr, buffer_size)) < 0)
			error_exit("ERROR : send_all");
		if (rt == 0)
			return;
		buffer_size -= rt;
		ptr += rt;
	}
}

int initalize_socket(){
	int socketfd;
	if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		error_exit("ERROR : server socket open");
	return socketfd;
}

struct hostent * name_to_hostent(char *hostname){
	struct hostent *he;
	if ((he = gethostbyname(hostname)) < 0)
		error_exit("ERROR : gethostbyname");
	return he;
}

struct sockaddr_in create_server(int port, struct hostent *he){
	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr = *((struct in_addr *) he->h_addr_list[0]);
	memset(&(server.sin_zero), '\0', 8);
	return server;
}

void connect_server(int socketfd, struct sockaddr_in *server){
	if (connect(socketfd, (struct sockaddr *) server, sizeof(struct sockaddr)) < 0)
		error_exit("ERROR : connect");
}

void send_command(int socket, const char *inputs){
	if (write(socket, inputs, PROTOCOL_SIZE) < 0)
		error_exit("ERROR : send_command -> write");
}

void split_protocol(char **dst, char *inputs){
	for (char *tok = strtok(inputs, " "); tok; tok = strtok(NULL, " "))
		*dst++ = tok;
	dst = NULL;
}


void mkdir_path(char *path){
	char *args[4];
	switch(fork()){
	case -1:
		error_exit("ERROR : mkdir_path -> fork()");
	case 0:
		args[0] = "mkdir";
		args[1] = "-p";
		args[2] = path;
		args[3] = NULL;
		execvp(args[0], args);
		error_exit("ERROR : execvp");
	default:
		while(wait(NULL) > 0);
	}
}

void cpy_file_from_socket(int socket, char *path, long long file_size){
	int file_fd;
	char file[file_size];
	read_all(socket, file, file_size);
	if ((file_fd = open(path, O_CREAT | O_WRONLY, 0666)) < 0)
		error_exit("ERROR : cpy_file_from_socket -> open");
	write_all(file_fd, file, file_size);
	close(file_fd);
}

void receive_do_protocol(int socket){
	char protocol[PROTOCOL_SIZE];
	while(1){
		char *splitted_protocol[4];
		if (read(socket, protocol, PROTOCOL_SIZE) < 0 )
			error_exit("ERROR : receive_do_protocol -> read");
		split_protocol(splitted_protocol, protocol);
		if (strcmp(splitted_protocol[0], "MKDIR") == 0)
			mkdir_path(splitted_protocol[1]);
		else if (strcmp(splitted_protocol[0], "FILE") == 0)
			cpy_file_from_socket(socket, splitted_protocol[1], atoi(splitted_protocol[2]));
		else if (strcmp(splitted_protocol[0], "END") == 0){
			close(socket);
			return;
		}
	}
}

void start_client(char *hostname, int port, const char *inputs){
	int socketfd;

	socketfd = initalize_socket();
	struct sockaddr_in server = create_server(port, name_to_hostent(hostname));
	connect_server(socketfd, &server);

	send_command(socketfd, inputs);
	receive_do_protocol(socketfd);
}

int main(int argc, char **argv){
	char buffer[PROTOCOL_SIZE];
	if (argc != 4)
		printf("Usage: client hostname port dir\n"), exit(1);
	strcpy(buffer, argv[3]);
	start_client(argv[1], atoi(argv[2]), buffer);
}
