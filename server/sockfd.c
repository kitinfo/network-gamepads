#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int sock_open(char* host, int port_i){
	int fd=-1, status;
	char port[10];
	struct addrinfo hints;
	struct addrinfo* info;
	struct addrinfo* addr_it;

	memset(&hints, 0, sizeof(hints));
	snprintf(port, 9, "%d", port_i);

	hints.ai_family=AF_UNSPEC;
	hints.ai_socktype=SOCK_STREAM;
	hints.ai_flags=AI_PASSIVE;

	status=getaddrinfo(host, port, &hints, &info);
	if(status!=0){
		fprintf(stderr, "sock_open/gai: %s\n", gai_strerror(status));
		return -1;
	}

	for(addr_it=info;addr_it!=NULL;addr_it=addr_it->ai_next){
		fd=socket(addr_it->ai_family, addr_it->ai_socktype, addr_it->ai_protocol);
		if(fd<0){
			continue;
		}

		status=bind(fd, addr_it->ai_addr, addr_it->ai_addrlen);
		if(status<0){
			close(fd);
			continue;
		}

		break;
	}

	freeaddrinfo(info);

	if(!addr_it){
		fprintf(stderr, "Failed to create listening socket\n");
		return -1;
	}

	status=listen(fd, 30);
	if(status<0){
		perror("sock_open/listen");
		close(fd);
		return -1;
	}

	return fd;
}

int sock_close(int fd){
	return close(fd);
}
