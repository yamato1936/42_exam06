#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}


fd_set live;
int sockfd, maxfd, next_id, id[FD_SETSIZE];
char *in[FD_SETSIZE], *out[FD_SETSIZE];

void fatal(void)
{
	write(2, "Fatal error\n", 12);
	exit(1);
}

void broadcast(int from, char *msg)
{
	for (int fd = 0; fd <= maxfd; fd++)
		if (fd != sockfd && fd != from && FD_ISSET(fd, &live))
		{
			out[fd] = str_join(out[fd], msg);
			if (!out[fd])
				fatal();
		}
}

int main(int ac, char **av) {
	int connfd;
	socklen_t len;
	struct sockaddr_in servaddr, cli;

	if (ac != 2) {
		write(2, "Wrong number of arguments\n", 26);
		return 1;
	} 

	// socket create and verification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1 || sockfd >= FD_SETSIZE) {
		fatal();
	}
	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(av[1])); 

	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) {
		fatal();
	}
	if (listen(sockfd, 10) != 0) {
		fatal();
	}
	maxfd = sockfd;
	FD_ZERO(&live);
	FD_SET(sockfd, &live);
	while (1)
	{
		fd_set rd = live, wr;
		FD_ZERO(&wr);
		for (int fd = 0; fd <= maxfd; fd++)
			if (out[fd])
				FD_SET(fd, &wr);
		if (select(maxfd + 1, &rd, &wr, NULL, NULL) < 0)
			fatal();
		for (int fd = 0; fd <= maxfd; fd++)
		{
			char buf[4097], *msg;
			if (FD_ISSET(fd, &wr))
			{
				int n = send(fd, out[fd], strlen(out[fd]), MSG_DONTWAIT | MSG_NOSIGNAL);
				if (n > 0)
				{
					int i = 0;
					while ((out[fd][i] = out[fd][i + n]) != 0)
						i++;
					if (!out[fd][0])
					{
						free(out[fd]);
						out[fd] = NULL;
					}
				}
			}
			if (!FD_ISSET(fd, &rd))
				continue;
			if (fd == sockfd)
			{
				len = sizeof(cli);
				connfd = accept(sockfd, (struct sockaddr *)&cli, &len);
				if (connfd < 0)
					continue;
				if (connfd >= FD_SETSIZE)
				{
					close(connfd);
					continue;
				}
				if (connfd > maxfd)
					maxfd = connfd;
				FD_SET(connfd, &live);
				id[connfd] = next_id++;
				sprintf(buf, "server: client %d just arrived\n", id[connfd]);
				broadcast(connfd, buf);
			}
			else
			{
				int n = recv(fd, buf, sizeof(buf) - 1, 0);
				if (n <= 0)
				{
					FD_CLR(fd, &live);
					sprintf(buf, "server: client %d just left\n", id[fd]);
					broadcast(fd, buf);
					close(fd);
					free(in[fd]);
					free(out[fd]);
					in[fd] = out[fd] = NULL;
					continue;
				}
				buf[n] = 0;
				in[fd] = str_join(in[fd], buf);
				if (!in[fd])
					fatal();
				while ((n = extract_message(&in[fd], &msg)) > 0)
				{
					sprintf(buf, "client %d: ", id[fd]);
					broadcast(fd, buf);
					broadcast(fd, msg);
					free(msg);
				}
				if (n < 0)
					fatal();
			}
		}
	}
}
