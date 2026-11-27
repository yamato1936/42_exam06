# Exam06 mini_serv — 配布コードから完成させる全手順

[問題文](../subject/subject.txt) / [配布コード](../subject/give_main.c) / [完成コード](mini_serv_from_given.c)

この文書だけを読んで、配布された main.c から提出用 mini_serv.c を完成させる。ゼロから書く mini_serv.c の解法を暗記する必要はない。配布コードの extract_message / str_join / socket / bzero / bind / listen / accept を最大限残す。

## 0. 最初にやること

### 試験本番

ターミナルで現在地と配布ファイルを確認する。

```bash
pwd
ls -la
```

問題文に指定された提出ディレクトリに移動してから、配布された main.c を mini_serv.c にコピーする。既に mini_serv.c という名前ならコピー不要。実際の配布ファイル名・提出場所は試験画面に従う。

```bash
cp main.c mini_serv.c
vim mini_serv.c
```

vim 以外の利用可能なエディタでもよい。

### GitHubで練習する場合（本番では不要）

```bash
git clone https://github.com/yamato1936/42_exam06.git
cd 42_exam06
cp subject/give_main.c mini_serv.c
vim mini_serv.c
```

すでに clone 済みなら git pull を使う。以下の STEP 1〜8 を実行する。変更不要な箇所は触らない。

## 1. ヘッダー1行だけ追加

`#include <sys/socket.h>` を探し、その下に次の1行を追加する。

```c
#include <sys/select.h>
```

既存ヘッダーは全て残す。select、fd_set、FD_ZERO / FD_SET / FD_ISSET / FD_CLR を使用するため。

## 2. extract_message と str_join は完全にそのまま

配布済みの2関数を削除・修正しない。

| 関数 | 動作 |
|---|---|
| extract_message(&in[fd], &msg) | 最初の改行までを msg に移し、残りを in[fd] に保持する |
| str_join(in[fd], buf) | 受信済み文字列に新しい文字列を連結する。成功時に元の文字列を free |
| str_join(out[fd], msg) | 送信待ち文字列の末尾に新しい通知・メッセージを追加する |

extract_message の戻り値 1 は成功、0 は改行なし、-1 は確保失敗。切り出した msg は呼び出し側で free する。

## 3. main の直前（str_join の閉じ括弧の後）に追加

次のコード全文を追加する。既存の main はまだ消さない。

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

### 何を覚えるか

- live: サーバーと現在接続しているクライアントのFD集合。
- sockfd: リスニング用FD。main からグローバルへ移す。
- maxfd: これまで登録した最大FD。切断後も大きい値のままで問題ない。
- next_id: 新規クライアントのID。0から増え続ける。
- id[fd]: FDとクライアント番号の対応。FDは再利用されてもIDは増える。
- in[fd]: 未完成の受信行。out[fd]: まだ送信していない文字列。
- fatal: stderr に Fatal error と改行を出し、終了コード1で終了。
- broadcast: サーバー自身と送信元を除いた全クライアントの out に追加。ここで send を直接呼ばない。

グローバル変数は自動的にゼロ初期化されるので、配列を初期化するコードは不要。

## 4. main 冒頭を変更

**変更前（この範囲を選択）**

```c
int main() {
	int sockfd, connfd, len;
	struct sockaddr_in servaddr, cli;
```

**変更後（これに置換）**

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

sockfd をグローバルにしたのでローカル宣言から削除する。connfd と cli は残す。len は accept の引数型に合わせて socklen_t に変更。引数が1つでなければ stderr に指定メッセージを出す。

## 5. socket のエラー処理を変更

socket の呼び出し行と bzero は残す。その間だけ置換する。

**変更前（この範囲を選択）**

```c
	if (sockfd == -1) { 
		printf("socket creation failed...\n"); 
		exit(0); 
	} 
	else
		printf("Socket successfully created..\n");
```

**変更後（これに置換）**

```c
	if (sockfd == -1 || sockfd >= FD_SETSIZE) {
		fatal();
	}
```

成功時に printf してはいけない。FD_SETSIZE を超えないことも確認する。

## 6. ポート番号だけ変更

**変更前（この範囲を選択）**

```c
	servaddr.sin_port = htons(8081);
```

**変更後（これに置換）**

```c
	servaddr.sin_port = htons(atoi(av[1]));
```

その上にある servaddr.sin_family と servaddr.sin_addr.s_addr の2行は変更しない。2130706433 は 127.0.0.1。

## 7. bind / listen のエラー処理だけ変更

**変更前（この範囲を選択）**

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

**変更後（これに置換）**

```c
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) {
		fatal();
	}
	if (listen(sockfd, 10) != 0) {
		fatal();
	}
```

呼び出しと条件式は元のまま。listen の backlog 10 もそのまま。ここまででサーバー初期化が完成する。

## 8. 単発 accept を select ループに置換（最重要）

### 8.0 削除する箇所

listen のエラー処理より後の、以下の部分だけ削除する。main の最後の閉じ括弧は残す。

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

削除した場所に、以下のコードをまとめて追加する。main の最後の閉じ括弧は既存のものを残す。

### 8.5 — 追加する select ループ全文

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

この直後に main の閉じ括弧が1つ必要。

## 9. コンパイル

```bash
cc -Wall -Wextra -Werror mini_serv.c -o mini_serv
```

エラーがなければ実行ファイルが作られる。エラーが出たら表示された行番号の少し前から確認する。

| エラー例 | 確認する場所 |
|---|---|
| unknown type name fd_set | STEP 1 のヘッダー |
| conflicting types / pointer-sign accept | STEP 4 の socklen_t len |
| undeclared identifier sockfd | STEP 3 のグローバル |
| expected declaration or statement at end of input | STEP 8 の閉じ括弧 |
| unused variable | 元の宣言や不要なコードが残っていないか |

練習時は完成コードと比較できる。

```bash
diff -u subject/give_main.c mini_serv.c
diff -u solution/mini_serv_from_given.c mini_serv.c
```

後者の差分が空なら完全一致。空白の差だけなら動作上は問題ない。

## 10. 動作確認（端末を4つ開く）

### 10.1 引数なし・エラー時

```bash
./mini_serv
echo $?
```

期待する表示: Wrong number of arguments、終了コード1。

### 10.2 端末A: サーバー

```bash
./mini_serv 8081
```

正常なら何も出力せず待機する。

### 10.3 端末B: client 0

```bash
nc 127.0.0.1 8081
```

### 10.4 端末C: client 1

```bash
nc 127.0.0.1 8081
```

Bに `server: client 1 just arrived` が表示される。C自身には表示されない。

### 10.5 端末D: client 2

```bash
nc 127.0.0.1 8081
```

BとCに `server: client 2 just arrived` が表示される。

### 10.6 メッセージ

Cで hello と入力してEnter。BとDの期待出力:

```text
client 1: hello
```

Cにはサーバーからのエコーがない（ターミナル自身の入力表示は別）。複数行を貼り付けて全行に prefix がつくことも確認。

### 10.7 切断と再接続

Cで Ctrl+C。BとDの期待出力:

```text
server: client 1 just left
```

Cで nc を再実行するとBとDに `server: client 3 just arrived` が表示される。FDが再利用されてもIDは3。

### 10.8 bind エラー

サーバーAを起動したまま別端末から同じポートで実行。

```bash
./mini_serv 8081
echo $?
```

期待出力は Fatal error、終了コード1。

## 11. 自動通信テスト（練習用・Python 3、外部ライブラリ不要）

以下はリポジトリのルートで実行する。試験の提出コードに Python は含めない。手動テストのサーバーは先に止める。

```bash
python3 - <<'PY'
import socket, subprocess, time

with socket.socket() as tmp:
    tmp.bind(("127.0.0.1", 0))
    port = tmp.getsockname()[1]
p = subprocess.Popen(["./mini_serv", str(port)])
clients = []
def connect():
    for _ in range(100):
        try:
            s = socket.create_connection(("127.0.0.1", port), timeout=1)
            s.settimeout(2)
            clients.append(s)
            return s
        except ConnectionRefusedError:
            time.sleep(0.02)
    raise RuntimeError("server not ready")
def expect(s, expected):
    expected = expected.encode()
    got = b""
    while len(got) < len(expected):
        chunk = s.recv(len(expected) - len(got))
        if not chunk:
            raise AssertionError("unexpected EOF")
        got += chunk
    assert got == expected, (got, expected)
try:
    a, b = connect(), connect()
    expect(a, "server: client 1 just arrived\n")
    c = connect()
    for s in (a, b):
        expect(s, "server: client 2 just arrived\n")
    b.sendall(b"hello\nworld\n")
    for s in (a, c):
        expect(s, "client 1: hello\nclient 1: world\n")
    b.sendall(b"part")
    time.sleep(0.05)
    c.sendall(b"other\n")
    for s in (a, b):
        expect(s, "client 2: other\n")
    b.sendall(b"ial\n")
    for s in (a, c):
        expect(s, "client 1: partial\n")
    b.close()
    for s in (a, c):
        expect(s, "server: client 1 just left\n")
    d = connect()
    for s in (a, c):
        expect(s, "server: client 3 just arrived\n")
    print("PASS: connect, multiline, fragmented lines, disconnect, reconnect")
finally:
    for s in clients:
        s.close()
    p.terminate()
    p.wait()
PY
```

このテストは基本通信のみ。低速クライアント・部分送信・メモリリークの網羅的検証や公式テスターの代替ではない。

## 12. 提出直前チェック

- [ ] 提出ファイル名が mini_serv.c
- [ ] 指定のディレクトリに配置
- [ ] cc -Wall -Wextra -Werror でコンパイル
- [ ] 引数エラー、Fatal error の文言・改行・終了コード
- [ ] 127.0.0.1 のみに bind
- [ ] printf、fcntl、独自の #define を追加していない
- [ ] select の後に accept / recv / send
- [ ] 送信元に配信しない
- [ ] 各行に client ID を付ける
- [ ] 遅い受信側の送信データを捨てない
- [ ] 切断時に FD_CLR / close / free / NULL
- [ ] 余計な実行ファイルやテストコードを提出しない

試験の提出コマンド・操作は実際の試験画面の指示に従う。

## 13. 30分で書く練習用の時間配分（目安）

| 時間 | 内容 |
|---|---|
| 0〜3分 | ファイル確認・コピー・ヘッダー |
| 3〜7分 | グローバル・fatal・broadcast |
| 7〜10分 | main の引数・socket・ポート・bind・listen |
| 10〜22分 | select、send、accept、recv |
| 22〜26分 | コンパイルエラー修正 |
| 26〜30分 | nc で確認、提出前チェック |

## 14. 完成コード全文（参照用）

STEP 1〜8を実施したときの最終形。既存のファイルに丸ごと追記するものではない。

```c
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
```

## 検証状況・制約

この文書の置換手順を機械的に適用すると、現在の GitHub の mini_serv_from_given.c と完全一致することを確認済み。今回の更新は Markdown のみで、Cソースの再コンパイル・通信テスト・公式テスター実行はしていない。上のテストを実際の環境で実行して確認すること。文字列連結のコピーコストや Linux 固有の send フラグは従来の解答と同じ。
