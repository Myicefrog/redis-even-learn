gcc -o server ae.c ae_epoll.c anet.c main.c  zmalloc.c -DHAVE_EPOLL
gcc -o client client.c
