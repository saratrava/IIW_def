server: server.c mylib.c mytrslib.c timeout_lib.c send_window_lib.c recv_window_lib.c
	gcc -Wall -Wextra -O2 server.c -o server.o -pthread mylib.c mytrslib.c timeout_lib.c send_window_lib.c recv_window_lib.c -lm -lrt

client: client.c mylib.c mytrslib.c timeout_lib.c send_window_lib.c recv_window_lib.c
	gcc -Wall -Wextra -O2 client.c -o client.o -pthread mylib.c mytrslib.c timeout_lib.c send_window_lib.c recv_window_lib.c -lm -lrt
