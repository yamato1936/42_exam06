# Exam06 mini_serv — 配布コードからの書き換え手順

対象: [give_main.c](../subject/give_main.c) → [mini_serv_from_given.c](mini_serv_from_given.c)

試験では配布された main.c を mini_serv.c にコピーし、以下の順番で編集する。既存の2つの文字列関数は一切書き直さない。

```bash
cp subject/give_main.c mini_serv.c
```

## STEP 1 — ヘッダーを1行追加

`#include <errno.h>` の直後に追加。

```c
#include <sys/select.h> // ADDED
```

select と fd_set を使用するため。

## STEP 2 — 既存関数は変更しない

- extract_message: 最初の改行までを切り出し、残りを受信バッファに保持する。戻り値は 1 (成功)、0 (改行なし)、-1 (確保失敗)。
- str_join: 既存文字列と追加文字列を連結し、古いバッファを解放する。

## STEP 3 — main の直前に以下を順番に追加

### 3.1 グローバル変数

```c
// ADDED: client state and select sets.
fd_set live;
int server, maxfd, next_id;
int id[FD_SETSIZE];
char *in[FD_SETSIZE], *out[FD_SETSIZE];
```

live は接続中の FD 集合。id は FD とクライアント番号の対応。in は未完成の受信行、out は未送信のデータ。next_id は単調増加する ID。グローバル変数はゼロ初期化される。

### 3.2 fatal

```c
void fatal(void)
{
    write(2, "Fatal error\n", 12);
    exit(1);
}
```

問題指定のエラーメッセージを stderr に出して終了する。

### 3.3 broadcast

```c
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
```

サーバー自身と送信元を除いた全クライアントの out に追記する。ここで send しないので、読み取りの遅いクライアントも切断しない。

### 3.4 notice

```c
void notice(int fd, char *format)
{
    char msg[64];
    sprintf(msg, format, id[fd]);
    broadcast(fd, msg);
}
```

接続・切断の通知を共通化する。

### 3.5 receive_client

```c
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
```

recv が正なら受信文字列を in に連結する。extract_message を繰り返して完成した行だけを配信し、各行に prefix を付ける。0 以下なら FD を削除して通知・close・free を行う。

### 3.6 send_client

```c
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
```

送信できたバイト数だけ out から除く。部分送信の場合は残りを保持する。送信できなかった場合も out を捨てない。

## STEP 4 — main の先頭を置換

**変更前**

```c
int main() {
	int sockfd, connfd, len;
	struct sockaddr_in servaddr, cli;
```

**変更後**

```c
int main(int ac, char **av) {
    int sockfd, connfd;
    struct sockaddr_in servaddr;

    if (ac != 2) {
        write(2, "Wrong number of arguments\n", 26);
        return 1;
    }
```


cli と len は不要になる。ポートをコマンドライン引数から取得するため argc/argv を追加する。

## STEP 5 — socket のエラー処理を置換

**変更前**

```c
// socket create and verification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) { 
		printf("socket creation failed...\n"); 
		exit(0); 
	} 
	else
		printf("Socket successfully created..\n"); 
	bzero(&servaddr, sizeof(servaddr));
```

**変更後**

```c
// socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0 || sockfd >= FD_SETSIZE)
        fatal();
    bzero(&servaddr, sizeof(servaddr));
```


成功メッセージの printf と exit(0) を消す。FD_SETSIZE の上限も確認する。

## STEP 6 — ポート指定を1行変更

**変更前**

```c
servaddr.sin_port = htons(8081);
```

**変更後**

```c
servaddr.sin_port = htons(atoi(av[1]));
```


IP の設定はそのまま残す。

## STEP 7 — bind / listen のエラー処理を置換

**変更前**

```c
// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) { 
		printf("socket bind failed...\n"); 
		exit(0); 
	} 
	else
		printf("Socket successfully binded..\n");
	if (listen(sockfd, 10) != 0) {
		printf("cannot listen\n"); 
		exit(0); 
	}
```

**変更後**

```c
// Binding newly created socket to given IP and verification
    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
        fatal();
    if (listen(sockfd, 10) != 0)
        fatal();
```


余分な printf を削除し fatal に統一する。

## STEP 8 — 単発 accept を削除して select ループを追加

**削除するコード（main の末尾の閉じ括弧は残す）**

```c
len = sizeof(cli);
	connfd = accept(sockfd, (struct sockaddr *)&cli, &len);
	if (connfd < 0) { 
        printf("server acccept failed...\n"); 
        exit(0); 
    } 
    else
        printf("server acccept the client...\n");
```

**同じ位置に追加するコード（main の末尾の閉じ括弧は既存のものを利用）**

```c
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
```

select の処理順序:

1. live を rd にコピーし、out がある FD だけ wr に登録。
2. select で準備完了の FD を取得。
3. server が読み取り可能なら accept → ID を振る → live に登録 → 到着通知。
4. client が読み取り可能なら receive_client。
5. client がまだ接続中かつ書き込み可能なら send_client。
6. 繰り返し。

## STEP 9 — コンパイル・差分確認

```bash
cc -Wall -Wextra -Werror mini_serv.c -o mini_serv
diff -u subject/give_main.c mini_serv.c
diff -u solution/mini_serv_from_given.c mini_serv.c
./mini_serv 8081
```

別々の端末で nc 127.0.0.1 8081 を実行。2人以上で接続し、到着通知・複数行・分割受信・切断通知を確認する。引数なしの場合は Wrong number of arguments が stderr に出ることも確認する。

## 覚える順番

fatal → broadcast → notice → receive_client → send_client → select ループ。

socket / bind / listen と文字列関数は配布済みなので丸暗記不要。変更点は引数・エラー処理・ポート・単発 accept の置換。

## 注意

本手順は現在の解答コードに対応する。公式テスターでの合格、コンパイル・実行は未確認。行単位のバッファリングと文字列連結を使用しているため、大量の未送信データではコピーコストが増える。Linux の MSG_DONTWAIT / MSG_NOSIGNAL を使用する。
