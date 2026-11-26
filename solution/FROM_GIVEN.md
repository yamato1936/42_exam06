# 配布コードからの変更手順

```bash
cp subject/give_main.c mini_serv.c
diff -u subject/give_main.c solution/mini_serv_from_given.c
```

## 変更・追加順

1. `#include <sys/select.h>` を追加。`extract_message` と `str_join` は変更しない。
2. `main` の直前に fd_set、ID、受信・送信バッファと `fatal`, `broadcast`, `notice`, `receive_client`, `send_client` を追加。
3. `main()` を `main(int ac, char **av)` に変更し、引数の個数を検証。
4. socket/bind/listen の printf と exit(0) を fatal() に置換。
5. 固定ポート 8081 を atoi(av[1]) に変更。127.0.0.1 はそのまま。
6. 単発の accept とそのエラー表示を削除し、select ループを追加。
7. read set に server があれば accept、client があれば recv。write set に client があれば送信待ちデータを send。

## 検証コマンド

```bash
cc -Wall -Wextra -Werror solution/mini_serv_from_given.c -o mini_serv
./mini_serv 8081
# 別々の端末で2回実行
nc 127.0.0.1 8081
```

接続通知、複数行、分割受信、切断通知、遅い受信側への送信待ち保持を確認すること。

注意: この解答は行単位の受信バッファを使う。文字列連結のため、大きな送信待ちキューではコピーコストが増える。公式テスターによる合格は未確認。
