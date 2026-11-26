#include <errno.h>
#include <sys/select.h> // ADDED
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
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


// ADDED: client state and select sets.
fd_set live;
int server, maxfd, next_id;
int id[FD_SETSIZE];
char *in[FD_SETSIZE], *out[FD_SETSIZE];

void fatal(void)
{
    write(2, "Fatal error\n", 12);
    exit(1);
}

// ADDED: keep pending data even for slow clients.
void broadcast(int from, char *msg)
{
    for (int fd = 0; fd <= maxfd; fd++)
        if (fd != server && fd != from && FD_ISSET(fd, &live))
        {
            out[fd] = str_join(out[fd], msg);
            if (!out[fd])
                fatal();
        }
}

void notice(int fd, char *format)
{
    char msg[64];
    sprintf(msg, format, id[fd]);
    broadcast(fd, msg);
}

void receive_client(int fd)
{
    char buf[4097], prefix[64], *msg;
    int n = recv(fd, buf, sizeof(buf) - 1, 0);
    int ret;

    if (n <= 0)
    {
        FD_CLR(fd, &live);
        notice(fd, "server: client %d just left\n");
        close(fd);
        free(in[fd]);
        free(out[fd]);
        in[fd] = out[fd] = NULL;
        return;
    }
    buf[n] = 0;
    in[fd] = str_join(in[fd], buf);
    if (!in[fd])
        fatal();
    // Reuse the supplied extract_message: one complete line at a time.
    while ((ret = extract_message(&in[fd], &msg)) > 0)
    {
        sprintf(prefix, "client %d: ", id[fd]);
        broadcast(fd, prefix);
        broadcast(fd, msg);
        free(msg);
    }
    if (ret < 0)
        fatal();
}

void send_client(int fd)
{
    int n = send(fd, out[fd], strlen(out[fd]), MSG_DONTWAIT | MSG_NOSIGNAL);
    if (n > 0)
    {
        if ((size_t)n == strlen(out[fd]))
        {
            free(out[fd]);
            out[fd] = NULL;
        }
        else
        {
            char *rest = str_join(NULL, out[fd] + n);
            if (!rest)
                fatal();
            free(out[fd]);
            out[fd] = rest;
        }
    }
}

// CHANGED: supplied main, with argument and error handling.
int main(int ac, char **av) {
    int sockfd, connfd;
    struct sockaddr_in servaddr;

    if (ac != 2) {
        write(2, "Wrong number of arguments\n", 26);
        return 1;
    }

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0 || sockfd >= FD_SETSIZE)
        fatal();
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
    servaddr.sin_port = htons(atoi(av[1]));

    // Binding newly created socket to given IP and verification
    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
        fatal();
    if (listen(sockfd, 10) != 0)
        fatal();

    // ADDED: replace the single accept with a select event loop.
    server = maxfd = sockfd;
    FD_ZERO(&live);
    FD_SET(server, &live);
    while (1)
    {
        fd_set rd = live, wr;
        FD_ZERO(&wr);
        for (int fd = 0; fd <= maxfd; fd++)
            if (FD_ISSET(fd, &live) && out[fd])
                FD_SET(fd, &wr);
        if (select(maxfd + 1, &rd, &wr, NULL, NULL) < 0)
            fatal();
        for (int fd = 0; fd <= maxfd; fd++)
        {
            if (!FD_ISSET(fd, &live))
                continue;
            if (fd == server)
            {
                if (!FD_ISSET(fd, &rd))
                    continue;
                connfd = accept(sockfd, NULL, NULL);
                if (connfd < 0)
                    continue;
                if (connfd >= FD_SETSIZE)
                {
                    close(connfd);
                    continue;
                }
                id[connfd] = next_id++;
                if (connfd > maxfd)
                    maxfd = connfd;
                FD_SET(connfd, &live);
                notice(connfd, "server: client %d just arrived\n");
            }
            else
            {
                if (FD_ISSET(fd, &rd))
                    receive_client(fd);
                if (FD_ISSET(fd, &live) && FD_ISSET(fd, &wr) && out[fd])
                    send_client(fd);
            }
        }
    }
}
