#define CC_NET_IMPLEMENTATION
#include "../cc_net.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    int32_t id;
    int32_t value;
} simple_packet_t;

void client(uint32_t server_address, uint16_t server_port, uint16_t client_port)
{
    cc_net_socket_t sock = cc_net_nonblocking_udp_socket_open(client_port);
    cc_net_address_t server = {server_address, server_port};

    simple_packet_t packet;
    size_t packet_size = sizeof(simple_packet_t);

    for (int32_t i = 0; i < 10; ++i) {
        packet.id = 0;
        packet.value = i;
		printf("[client] sending packed with id:value of %d:%d\n", packet.id, packet.value);
        cc_net_udp_socket_send(sock, server, (void*)&packet, packet_size);
    }

    packet.id = 1;
	packet.value = 0;
	printf("[client] sending packed with id:value of %d:%d\n", packet.id, packet.value);
    cc_net_udp_socket_send(sock, server, (void*)&packet, packet_size);

    cc_net_udp_socket_close(sock);

	printf("[client] exiting...\n");
    return;
}

void server(uint16_t server_port)
{
	printf("[server] starting...\n");
    cc_net_socket_t sock = cc_net_nonblocking_udp_socket_open(server_port);

    simple_packet_t packet;
    size_t packet_size = sizeof(simple_packet_t);

    int32_t is_running = 1;
    while (is_running) {
        int32_t recv_data_size = cc_net_udp_socket_recv(sock, (void*)&packet, packet_size);
        if (recv_data_size < 0) {
            printf("[server] error: cc_net_udp_socket_recv");
			is_running = 0;
        }
        else if ((size_t)recv_data_size == packet_size) {
            printf("[server] recv'd packet with id:value of %d:%d\n", packet.id, packet.value);
			/*
            if (packet.id == 2) {
				printf("[server] exiting...\n");
                is_running = 0;
            }
			*/
        }
    }

    cc_net_udp_socket_close(sock);
    return;
}

int main(int argc, char* argv[])
{
    uint32_t localhost = cc_net_host(127, 0, 0, 1);
    uint16_t client_port = 53333;
    uint16_t server_port = 35555;

    if (argc < 2) {
        printf("Usage: %s [server | client]\n", argv[0]);
		return (1);
    }

    cc_net_initialize();

    if (strcmp(argv[1], "client") == 0) {
        client(localhost, server_port, client_port);
    }
    else if (strcmp(argv[1], "server") == 0) {
        server(server_port);
    }
    else {
        printf("Usage: %s [server | client]\n", argv[0]);
		return (1);
    }

    cc_net_shutdown();

    return (0);
}
