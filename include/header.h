#pragma once
#include <cstdio>
#include <stdlib.h>
#include <unordered_map>
#include <unordered_set>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <assert.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <signal.h>
#include <strings.h>
#include <errno.h>
#include <stdint.h>
#include <queue>
#include <sys/eventfd.h>

#define DEBUG 0