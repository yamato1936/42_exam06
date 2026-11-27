# mini_serv — 配布コードを最大限残す解法

[配布コード](../subject/give_main.c) / [完成コード](mini_serv_from_given.c) / [問題文](../subject/subject.txt)

ゼロから書く mini_serv.c とは独立した解法。配布コードの extract_message、str_join、sockaddr_in、socket、bzero、IP、bind、listen、accept の呼び出しを再利用する。追加する補助関数は fatal と broadcast の2つのみ。送受信処理は main の select ループに直接書く。

```bash
cp subject/give_main.c mini_serv.c
```

## STEP 1 — ヘッダー1行追加

sys/socket.h の直後に追加。

```c
#include <sys/select.h>
```


## STEP 2 — 配布の2関数は何も変更しない

extract_message は受信文字列から改行までを取り出す（1: 成功、0: 改行なし、-1: malloc/calloc 失敗）。str_join は文字列を連結し、旧バッファを解放する。

## STEP 3 — main の直前にこれだけ追加

```c
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
```

live は接続済みのFD集合。in は未完成の受信行、out は送信待ちデータ。sockfd をグローバルに移すことで broadcast からサーバーFDを除外できる。グローバル変数はゼロ初期化。broadcast は送信元を除く接続中クライアントの out に追記する。

## STEP 4 — main の冒頭を変更

**変更前**

```c
int main() {
	int sockfd, connfd, len;
	struct sockaddr_in servaddr, cli;
```

**変更後**

```c
int main(int ac, char **av) {
	int connfd;
	socklen_t len;
	struct sockaddr_in servaddr, cli;

	if (ac != 2) {
		write(2, "Wrong number of arguments\n", 26);
		return 1;
	}
```


sockfd は STEP 3 へ移動した。connfd、cli はそのまま。accept の第3引数の型に合わせて len を socklen_t にする。

## STEP 5 — socket のエラー表示だけ変更

**変更前**

```c
	if (sockfd == -1) { 
		printf("socket creation failed...\n"); 
		exit(0); 
	} 
	else
		printf("Socket successfully created..\n");
```

**変更後**

```c
	if (sockfd == -1 || sockfd >= FD_SETSIZE) {
		fatal();
	}
```


socket の呼び出しと bzero はそのまま。成功時の printf は削除する。

## STEP 6 — 固定ポートを書き換える

**変更前**

```c
htons(8081)
```

**変更後**

```c
htons(atoi(av[1]))
```


127.0.0.1 の設定を変更しない。

## STEP 7 — bind と listen は残してエラー処理だけ変更

**変更前**

```c
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
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) {
		fatal();
	}
	if (listen(sockfd, 10) != 0) {
		fatal();
	}
```


bind と listen の条件式は配布のまま。listen の backlog 10 も維持する。

## STEP 8 — 単発 accept 部分を select ループに置換

main の末尾の閉じ括弧は残す。

**変更前**

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

**変更後**

```c
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
```


上のループを4ブロックとして覚える。

1. **準備**: live を rd にコピー。out があるクライアントだけ wr に登録し select を呼ぶ。
2. **send**: wr に含まれるFDのみ送信。送信できた n バイトだけを取り除く。未送信データを捨てず、空になったら free。文字列を前方へ詰めるループは終端 NUL もコピーするため、重なる領域に対する strcpy は不要。
3. **accept**: 配布の len = sizeof(cli) と accept の2行をそのまま再利用。IDを割り当てて到着通知。
4. **recv**: 切断なら FD_CLR、通知、close、free。受信できたら str_join、extract_message、行ごとの prefix と broadcast。最後に確保失敗を判定する。

注意: 送信元にも同じメッセージを送らない。送信できないだけで切断しない。select の集合は毎回作り直す。FD_SETSIZE 以上を FD_SET しない。

## STEP 9 — 差分確認とテスト

```bash
diff -u subject/give_main.c mini_serv.c
diff -u solution/mini_serv_from_given.c mini_serv.c
cc -Wall -Wextra -Werror mini_serv.c -o mini_serv
./mini_serv 8081
```

別々の端末で nc 127.0.0.1 8081 を実行。接続・複数行・分割受信・切断・再接続・遅い読み取り側を確認する。

同等のローカル実装で厳格コンパイル、引数なし、3クライアント、分割受信と別クライアントからの送信、複数行、切断、再接続時のID増加を確認済み。公式テスターおよび大規模な低速クライアント負荷試験は未実施。Linux の MSG_DONTWAIT / MSG_NOSIGNAL を使用する。文字列連結によるコピーコストは残る。

## 暗記対象

グローバル変数、fatal、broadcast、select ループ。既存の文字列処理・ソケット初期化・accept を書き直さない。完成コードと同じ空白やコメントにする必要はない。
