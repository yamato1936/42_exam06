# 42 Exam Rank 06 — mini_serv
A minimal TCP chat server written in C using select() for I/O multiplexing.
## Overview
This project implements a multi-client TCP chat server listening on 127.0.0.1.

## Features
- Multiple simultaneous clients
- Sequential client IDs
- Connection and disconnection notifications
- Line-based message broadcasting
- Handling partial TCP reads and writes
## Build & Run
```bash
cc -Wall -Wextra -Werror mini_serv.c -o mini_serv
./mini_serv 8080
```


Connect using another terminal:
```bash
nc 127.0.0.1 8080
```

