gcc -o myserver sds.c ae.c ae_epoll.c anet.c  zmalloc.c server.c  adlist.c  networking.c -DHAVE_EPOLL
gcc -o myclient client.c
