#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <wait.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <dirent.h>
#include <libgen.h>

#define BACKLOG 10
#define PROTOCOL_SIZE 255

void error_exit(char *mgs){
	perror(mgs);
	exit(1);
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


void bind_listen(int socketfd, int port){
	struct sockaddr_in server;
	int one = 1;
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);
	if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) < 0)
		error_exit("ERROR : setsockopt");
	if (bind(socketfd, (struct sockaddr *) &server, sizeof(server)) < 0)
		error_exit("ERROR : bind");
	if (listen(socketfd, BACKLOG) < 0)
		error_exit("ERROR : bind");
}

int initalize_socket(int port){
	int socketfd;
	if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		error_exit("ERROR : server socket open");
	bind_listen(socketfd, port);
	return socketfd;
}

void send_mkdir_protocol(int socket, char *path){
	char buff[PROTOCOL_SIZE];
	sprintf(buff, "MKDIR %s", path);
	if (write(socket, buff, PROTOCOL_SIZE) < 0)
		error_exit("ERROR : send_mkdir_protocol -> write");
}

void send_cpfile_protocol(int socket, char *path, long long file_size){
	char buff[PROTOCOL_SIZE];
	sprintf(buff, "FILE %s %lld", path, file_size);
	if (write(socket, buff, PROTOCOL_SIZE) < 0)
		error_exit("ERROR : send_cpfile_protocol -> write");
}

void send_file(int socket, char *path, char *to_send_path, long long file_size){
	char file_buffer[file_size];
	int file_fd;
	if ((file_fd = open(path, O_RDONLY)) < 0)
		return;
	send_cpfile_protocol(socket, to_send_path, file_size);
	read_all(file_fd, file_buffer, file_size);
	write_all(socket, file_buffer, file_size);
}

void do_cp(int socket, char *path, char *to_send_path){
	struct stat request_stat;
	if (lstat(path, &request_stat) < 0)
		return;
	if (S_ISREG(request_stat.st_mode))
		send_file(socket, path, to_send_path, request_stat.st_size);
	else if (S_ISDIR(request_stat.st_mode)){
		send_mkdir_protocol(socket, to_send_path);
		DIR *dir;
		struct dirent *dirent;
		if ((dir = opendir(path)) == NULL)
			return;
		while((dirent = readdir(dir))){
			if (strcmp(dirent->d_name, "..") == 0 || strcmp(dirent->d_name, ".") == 0)
				continue;
			char next_path[PROTOCOL_SIZE], next_to_send_path[PROTOCOL_SIZE];
			sprintf(next_path, "%s/%s", path, dirent->d_name);
			sprintf(next_to_send_path, "%s/%s", to_send_path, dirent->d_name);
			do_cp(socket, next_path, next_to_send_path);
		}
	}
}


void receive_do_request(int socket){
	int rt_size, child_pid;
	char buff[PROTOCOL_SIZE], *path_basename;
	if ((rt_size = read(socket, buff, PROTOCOL_SIZE)) < 0)
		error_exit("ERROR : do_request -> read");
	buff[rt_size] = '\0';
	switch((child_pid = fork())){
	case -1:
		error_exit("ERROR : receive_do_request -> fork()");
	case 0:
		if ((path_basename = basename(buff)) == NULL)
			error_exit("ERROR : receive_do_request -> basename");
		do_cp(socket, buff, path_basename);
		sprintf(buff, "END");
		if (write(socket, buff, PROTOCOL_SIZE) < 0)
			error_exit("ERROR : receive_do_request -> write");
		exit(0);
	}
}

void start_server(int port){
	int socketfd, newsockfd, maxfd;
	fd_set fdset, cfdset;

	socketfd = initalize_socket(port);
	maxfd = socketfd;

	FD_ZERO(&fdset);
	FD_SET(socketfd, &fdset);

	while(1){
		cfdset = fdset;
		waitpid(-1,  NULL, WNOHANG);
		if (select(maxfd + 1, &cfdset, NULL, NULL, NULL) < 0)
			error_exit("ERROR : select");
		if (FD_ISSET(socketfd, &cfdset)){
			if ((newsockfd = accept(socketfd, NULL , NULL)) < 0)
				error_exit("ERROR : accept");
			FD_SET(newsockfd, &fdset);
			maxfd = (maxfd > newsockfd) ? maxfd : newsockfd;
			continue;
		}
		for(int i = 3; i <= maxfd; i++)
			if (i != socketfd && FD_ISSET(i, &cfdset)){
				char temp[32];
				if (recv(i, temp, sizeof(temp), MSG_PEEK | MSG_DONTWAIT) == 0){
					close(i);
					FD_CLR(i, &fdset);
				} else
					receive_do_request(i);
			}
	}
}

int main(int argc, char **argv){
	if (argc != 2){
		printf("USAGE : server port\n");
		return 1;
	}
	start_server(atoi(argv[1]));
}
