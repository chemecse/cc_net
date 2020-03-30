#define CC_NET_IMPLEMENTATION
#include "../cc_net.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h> /* sleep */

#define SERVER_PORT 35555
#define PACKET_SEND_SUCCESS 50

typedef struct {
    cc_net_virtual_connection_packet_header_t header;
    int32_t value;
} simple_packet_t;

void print_address(cc_net_address_t addr)
{
	printf("%d.%d.%d.%d:%d", (addr.host >> 24) & 0xFF, (addr.host >> 16) & 0xFF, (addr.host >> 8) & 0xFF, addr.host & 0xFF, (int)addr.port);
}

void client(void)
{
    cc_net_socket_t sock = cc_net_nonblocking_udp_socket_open(0);
    cc_net_address_t server = {cc_net_host(127, 0, 0, 1), SERVER_PORT};

	cc_net_virtual_connection_t conn = cc_net_virtual_connection(sock, server);

    simple_packet_t packet;
    size_t packet_size = sizeof(simple_packet_t);

	int32_t packet_value = 0;

	packet.header = cc_net_virtual_connection_prepare_header(&conn);
	packet.value = packet_value++;
	printf("[client] sending packet %d\n", packet.header.sequence);
	int32_t sent_data_size = cc_net_udp_socket_send(conn.socket, conn.remote_address, (void*)&packet, packet_size);
	if (sent_data_size != packet_size) {
		printf("[client] only sent %d / %d bytes\n", sent_data_size, (int)packet_size);
	}

	int32_t is_running = 1;
	while (is_running) {
		cc_net_address_t sender;
		int32_t recv_data_size = cc_net_udp_socket_recvfrom(sock, &sender, (void*)&packet, packet_size);
        if (recv_data_size < 0) {
            printf("[client] error: cc_net_udp_socket_recv %d\n", recv_data_size);
			is_running = 0;
        }
        else {
			if ((size_t)recv_data_size == packet_size) {
				cc_net_virtual_connection_consume_header(&conn, packet.header);

				if (sender.host != conn.remote_address.host || sender.port != conn.remote_address.port) {
					printf("[client] discarding packet from non-server entity\n");
					continue;
				}
				printf("[client] recv'd packet %d\n", packet.header.sequence);
				printf("[client]     ack: %d ack_bitfield: 0x%x\n", packet.header.ack, packet.header.ack_bitfield);
			}
			else if((size_t)recv_data_size == 0) {
				sleep(1);
			}

			cc_net_virtual_connection_packet_header_t header = cc_net_virtual_connection_prepare_header(&conn);

			packet.header = header;
			packet.value = packet_value++;
			if (packet_value > 10) {
				packet.value = 666;
				is_running = 0;
			}
			if (packet.value == 666 || rand() % 100 < PACKET_SEND_SUCCESS) {
				printf("[client] sending packet %d\n", packet.header.sequence);
				int32_t sent_data_size = cc_net_udp_socket_send(conn.socket, conn.remote_address, (void*)&packet, packet_size);
				if (sent_data_size != packet_size) {
					printf("[client] was unable to send all data\n");
				}
			}
			else {
				printf("[client] failed to sent packet\n");
			}
		}
	}

    cc_net_udp_socket_close(sock);

	printf("[client] exiting...\n");
    return;
}

void server(void)
{
	printf("[server] starting...\n");
    cc_net_socket_t sock = cc_net_nonblocking_udp_socket_open(SERVER_PORT);

    simple_packet_t packet;
    size_t packet_size = sizeof(simple_packet_t);

#define MAX_CLIENT_COUNT 256
	cc_net_virtual_connection_t clients[MAX_CLIENT_COUNT];
	int32_t client_count = 0;
	memset(clients, 0, sizeof(cc_net_virtual_connection_t) * MAX_CLIENT_COUNT);

    int32_t is_running = 1;
    while (is_running) {
		cc_net_address_t sender;
		int32_t recv_data_size = cc_net_udp_socket_recvfrom(sock, &sender, (void*)&packet, packet_size);
        if (recv_data_size < 0) {
            printf("[server] error: cc_net_udp_socket_recv %d\n", recv_data_size);
			is_running = 0;
        }
        else if ((size_t)recv_data_size == packet_size) {
			int32_t index = -1;
			for (int32_t i = 0; i < client_count; ++i) {
				if (clients[i].remote_address.host == sender.host && clients[i].remote_address.port == sender.port) {
					index = i;
					break;
				}
			}
			if (index < 0) {
				printf("[server] recv'd packet from ");
				print_address(sender);
				printf(" ... creating virtual connection\n");
				index = client_count;
				clients[index] = cc_net_virtual_connection(sock, sender);
				++client_count;
			}

			printf("[server] on conn %d\n", index);
			cc_net_virtual_connection_consume_header(&clients[index], packet.header);

			printf("[server] recv'd packet %d\n", packet.header.sequence);
			printf("[server]     ack: %d ack_bitfield: 0x%x\n", packet.header.ack, packet.header.ack_bitfield);

			if (packet.value == 666) {
				printf("[server] exiting...\n");
				is_running = 0;
			}
			else {
				cc_net_virtual_connection_packet_header_t header = cc_net_virtual_connection_prepare_header(&clients[index]);
				packet.header = header;
				packet.value += 1;
				if (rand() % 100 < PACKET_SEND_SUCCESS) {
					printf("[server] sending packet %d\n", packet.header.sequence);
					int32_t sent_data_size = cc_net_udp_socket_send(clients[index].socket, clients[index].remote_address, (void*)&packet, packet_size);
					if (sent_data_size != packet_size) {
						printf("[server] was unable to send all data\n");
					}
				}
				else {
					printf("[server] failed to send packet\n");
				}
			}
        }
	}

    cc_net_udp_socket_close(sock);
    return;
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        printf("Usage: %s [server | client]\n", argv[0]);
		return (1);
    }

    cc_net_initialize();

    if (strcmp(argv[1], "client") == 0) {
        client();
    }
    else if (strcmp(argv[1], "server") == 0) {
        server();
    }
    else {
        printf("Usage: %s [server | client]\n", argv[0]);
		return (1);
    }

    cc_net_shutdown();

    return (0);
}
