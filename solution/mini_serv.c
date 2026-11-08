#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

fd_set live;
char *out[FD_SETSIZE];
size_t len[FD_SETSIZE], pos[FD_SETSIZE];
int id[FD_SETSIZE], first[FD_SETSIZE], server, maxfd, next_id;

void fatal(void) {
    write(2, "Fatal error\n", 12);
    exit(1);
}

void queue(int fd, char *s, int n) {
    if (n <= 0) return;
    char *p = realloc(out[fd], len[fd] + n);
    if (!p) fatal();
    out[fd] = p;
    for (int i = 0; i < n; i++) p[len[fd] + i] = s[i];
    len[fd] += n;
}

void relay(int from, char *s, int n) {
    for (int fd = 0; fd <= maxfd; fd++)
        if (fd != server && fd != from && FD_ISSET(fd, &live))
            queue(fd, s, n);
}

void message(int fd, char *s, int n) {
    int start = 0;
    for (int i = 0; i < n; i++) {
        if (first[fd]) {
            char prefix[64];
            relay(fd, prefix, sprintf(prefix, "client %d: ", id[fd]));
            first[fd] = 0;
        }
        if (s[i] == '\n') {
            relay(fd, s + start, i - start + 1);
            start = i + 1;
            first[fd] = 1;
        }
    }
    relay(fd, s + start, n - start);
}

void disconnect(int fd) {
    char buf[64];
    relay(fd, buf, sprintf(buf, "server: client %d just left\n", id[fd]));
    FD_CLR(fd, &live);
    close(fd);
    free(out[fd]);
    out[fd] = NULL;
    len[fd] = pos[fd] = 0;
}

int main(int ac, char **av) {
    if (ac != 2) {
        write(2, "Wrong number of arguments\n", 26);
        return 1;
    }
    server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0 || server >= FD_SETSIZE) fatal();
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(2130706433);
    addr.sin_port = htons(atoi(av[1]));
    if (bind(server, (void *)&addr, sizeof(addr)) < 0 || listen(server, 128) < 0)
        fatal();
    FD_ZERO(&live);
    FD_SET(server, &live);
    maxfd = server;
    while (1) {
        fd_set rd = live, wr;
        FD_ZERO(&wr);
        for (int fd = 0; fd <= maxfd; fd++)
            if (fd != server && FD_ISSET(fd, &live) && pos[fd] < len[fd])
                FD_SET(fd, &wr);
        if (select(maxfd + 1, &rd, &wr, NULL, NULL) < 0) fatal();
        for (int fd = 0; fd <= maxfd; fd++) {
            if (fd == server && FD_ISSET(fd, &rd)) {
                int c = accept(server, NULL, NULL);
                if (c < 0) continue;
                if (c >= FD_SETSIZE) { close(c); continue; }
                if (c > maxfd) maxfd = c;
                id[c] = next_id++;
                first[c] = 1;
                FD_SET(c, &live);
                char buf[64];
                relay(c, buf, sprintf(buf, "server: client %d just arrived\n", id[c]));
            }
            else if (fd != server && FD_ISSET(fd, &live)) {
                if (FD_ISSET(fd, &rd)) {
                    char buf[4096];
                    int n = recv(fd, buf, sizeof(buf), 0);
                    if (n <= 0) { disconnect(fd); continue; }
                    message(fd, buf, n);
                }
                if (FD_ISSET(fd, &wr) && pos[fd] < len[fd]) {
                    size_t n = len[fd] - pos[fd];
                    if (n > 4096) n = 4096;
                    int k = send(fd, out[fd] + pos[fd], n, MSG_DONTWAIT | MSG_NOSIGNAL);
                    if (k > 0) pos[fd] += k;
                    if (pos[fd] == len[fd]) {
                        free(out[fd]);
                        out[fd] = NULL;
                        len[fd] = pos[fd] = 0;
                    }
                }
            }
        }
    }
}