#ifndef CC_NET_H
#define CC_NET_H

#include <stdint.h>
#include <stddef.h>

#ifdef CC_NET_STATIC
	#define CC_NET_DEFINE static
#else
	#define CC_NET_DEFINE extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

CC_NET_DEFINE int32_t cc_net_initialize(void);

CC_NET_DEFINE void cc_net_shutdown(void);

typedef struct {
	int32_t handle;
} cc_net_socket_t;

CC_NET_DEFINE cc_net_socket_t cc_net_nonblocking_udp_socket_open(uint16_t port);

CC_NET_DEFINE void cc_net_udp_socket_close(cc_net_socket_t socket);

typedef struct {
	uint32_t host;
	uint16_t port;
} cc_net_address_t;

CC_NET_DEFINE uint32_t cc_net_host(uint8_t a, uint8_t b, uint8_t c, uint8_t d);

CC_NET_DEFINE int32_t cc_net_udp_socket_send(cc_net_socket_t socket, cc_net_address_t destination, void* data, size_t size);

CC_NET_DEFINE int32_t cc_net_udp_socket_recv(cc_net_socket_t socket, void* data, size_t size);

CC_NET_DEFINE int32_t cc_net_udp_socket_recvfrom(cc_net_socket_t socket, cc_net_address_t* sender, void* data, size_t size);

#define CC_NET_BITS_PER_ACK_BITFIELD 32
typedef struct {
    uint32_t sequence;
    uint32_t ack;
    uint32_t ack_bitfield;
} cc_net_virtual_connection_packet_header_t;

#define CC_NET_MAX_SEQUENCE_VALUE 4096
#define CC_NET_MAX_SEQUENCE_BUFFER_COUNT 256
typedef struct {
	uint32_t data[CC_NET_MAX_SEQUENCE_BUFFER_COUNT];
	int32_t start;
	int32_t end;
} cc_net_sequence_buffer_t;

typedef struct {
    cc_net_socket_t socket;
	cc_net_address_t remote_address;
    int32_t local_sequence;
    uint32_t remote_sequence;

	cc_net_sequence_buffer_t sequence_buffer;
} cc_net_virtual_connection_t;

CC_NET_DEFINE cc_net_virtual_connection_t cc_net_virtual_connection(cc_net_socket_t socket, cc_net_address_t remote_address);

CC_NET_DEFINE cc_net_virtual_connection_packet_header_t cc_net_virtual_connection_prepare_header(cc_net_virtual_connection_t* connection);

CC_NET_DEFINE void cc_net_virtual_connection_consume_header(cc_net_virtual_connection_t* connection, cc_net_virtual_connection_packet_header_t header);

#ifdef __cplusplus
}
#endif

#ifdef CC_NET_IMPLEMENTATION

#if defined(_WIN32)
	#define CC_NET_PLATFORM_WINDOWS
#elif defined(__APPLE__) || defined(__linux__)
	#define CC_NET_PLATFORM_UNIX
#else
	#error "cc_net: unknown platform"
#endif

#if defined(CC_NET_PLATFORM_WINDOWS)
#include <Winsock2.h>
#pragma comment(lib, "Ws2_32.lib")
#elif defined(CC_NET_PLATFORM_UNIX)
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#else
	#error "cc_net: unknown platform"
#endif

CC_NET_DEFINE int32_t cc_net_initialize(void)
{
	int32_t result = 0;
#ifdef CC_NET_PLATFORM_WINDOWS
	WSADATA wsa_data;
	result = (WSAStartup(MAKEWORD(2, 2), &wsa_data) == 0);
#endif
	return (result);
}

CC_NET_DEFINE void cc_net_shutdown(void)
{
#ifdef CC_NET_PLATFORM_WINDOWS
	WSACleanup();
#endif
	return;
}

CC_NET_DEFINE cc_net_socket_t cc_net_nonblocking_udp_socket_open(uint16_t port)
{
	cc_net_socket_t result;

	result.handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (result.handle <= 0) {
		goto cc_net_nonblocking_udp_socket_open_error;
	}

	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	if (bind(result.handle, (const struct sockaddr*)&address, sizeof(struct sockaddr_in)) != 0) {
		goto cc_net_nonblocking_udp_socket_open_options_error;
	}

#if defined(CC_NET_PLATFORM_WINDOWS)
	u_long non_blocking_arg = 1;
	if (ioctlsocket(result.handle, FIONBIO, &non_blocking_arg) != 0) {
#elif defined(CC_NET_PLATFORM_UNIX)
	if (fcntl(result.handle, F_SETFL, O_NONBLOCK, 1) != 0) {
#else
	#error "cc_net: unknown platform"
#endif
		goto cc_net_nonblocking_udp_socket_open_options_error;
	}

	goto cc_net_nonblocking_udp_socket_open_success;

cc_net_nonblocking_udp_socket_open_options_error:
	cc_net_udp_socket_close(result);
cc_net_nonblocking_udp_socket_open_error:
	result.handle = -1;
cc_net_nonblocking_udp_socket_open_success:
	return (result);
}

CC_NET_DEFINE void cc_net_udp_socket_close(cc_net_socket_t socket)
{
	if (socket.handle > -1) {
#if defined(CC_NET_PLATFORM_WINDOWS)
		closesocket(socket.handle);
#elif defined(CC_NET_PLATFORM_UNIX)
		close(socket.handle);
#else
	#error "cc_net: unknown platform"
#endif
	}
	return;
}

CC_NET_DEFINE uint32_t cc_net_host(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
	uint32_t result = ((a << 24) | (b << 16) | (c << 8) | d);
	return (result);
}

CC_NET_DEFINE int32_t cc_net_udp_socket_send(cc_net_socket_t socket, cc_net_address_t destination, void* data, size_t size)
{
	if (socket.handle == -1) {
		return (-1);
	}

	struct sockaddr_in dst;
	dst.sin_family = AF_INET;
	dst.sin_addr.s_addr = htonl(destination.host);
	dst.sin_port = htons(destination.port);

	int32_t sent_data_size = sendto(socket.handle, (const void*)data, size, 0, (const struct sockaddr*)&dst, sizeof(struct sockaddr_in));

	return (sent_data_size);
}

CC_NET_DEFINE int32_t cc_net_udp_socket_recv(cc_net_socket_t socket, void* data, size_t size)
{
	if (socket.handle == -1) {
		return (-1);
	}

	int32_t recv_data_size = recv(socket.handle, data, size, 0);
	if (recv_data_size == -1 && errno == EAGAIN) {
		recv_data_size = 0;
	}

	return (recv_data_size);
}

CC_NET_DEFINE int32_t cc_net_udp_socket_recvfrom(cc_net_socket_t socket, cc_net_address_t* sender, void* data, size_t size)
{
	int32_t result = -1;

	if (socket.handle == -1) {
		return (result);
	}

	if (!sender) {
		return (cc_net_udp_socket_recv(socket, data, size));
	}

	struct sockaddr_in from;
	size_t from_size = sizeof(struct sockaddr_in);

	int32_t recv_data_size = recvfrom(socket.handle, data, size, 0, (struct sockaddr*)&from, (socklen_t*)&from_size);
	if (recv_data_size == -1 && errno == EAGAIN) {
		recv_data_size = 0;
	}
	result = recv_data_size;

	sender->host = ntohl(from.sin_addr.s_addr);
	sender->port = ntohs(from.sin_port);

	return (result);
}

CC_NET_DEFINE cc_net_virtual_connection_t cc_net_virtual_connection(cc_net_socket_t socket, cc_net_address_t remote_address)
{
	cc_net_virtual_connection_t result;
	result.socket = socket;
	result.remote_address = remote_address;
	result.local_sequence = 15;
	result.remote_sequence = 0;

	result.sequence_buffer.start = 0;
	result.sequence_buffer.end = 0;

	return (result);
}

static uint32_t cc_net__wrap(int32_t delta, uint32_t value, uint32_t max_value)
{
    uint32_t result;

    int32_t unwrapped_value = (int32_t)value + delta;
    if (unwrapped_value < 0) {
        result = value + delta + max_value;
    }
    else if (unwrapped_value >= max_value) {
        result = value + delta - max_value;
    }
    else {
        result = value + delta;
    }
    return (result);
}

#define CC_NET__WRAP_INC(value, max_value) (cc_net__wrap(1, (value), (max_value)))
#define CC_NET__WRAP_DEC(value, max_value) (cc_net__wrap(-1, (value), (max_value)))

static uint32_t cc_net__wrap_diff(uint32_t value0, uint32_t value1)
{
    uint32_t result;

    uint32_t min, max;
    if (value1 > value0) {
        max = value1;
        min = value0;
    }
    else {
        max = value0;
        min = value1;
    }

    uint32_t result0 = max - min;
    uint32_t result1 = CC_NET_MAX_SEQUENCE_VALUE - result0;
    if (result0 < result) {
        result = result0;
    }
    else {
        result = result1;
    }

    return (result);
}

static uint32_t cc_net__most_recent_sequence(uint32_t s0, uint32_t s1, uint32_t max)
{
    uint32_t half_max = max / 2;
    uint32_t result = ((s0 > s1) && ((s0 - s1) <= half_max)) || ((s1 > s0) && ((s1 - s0) > half_max));
    return (result);
}

#include <stdio.h>
static uint32_t cc_net__ack_bitfield(cc_net_virtual_connection_t* connection)
{
	uint32_t result = 0;

	uint32_t min_ack_sequence = cc_net__wrap(-(CC_NET_BITS_PER_ACK_BITFIELD - 1), connection->remote_sequence, CC_NET_MAX_SEQUENCE_VALUE);

	int32_t index = connection->sequence_buffer.start;
	while (index != connection->sequence_buffer.end) {
		uint32_t sequence = connection->sequence_buffer.data[index];
        if (cc_net__most_recent_sequence(sequence, min_ack_sequence, CC_NET_MAX_SEQUENCE_BUFFER_COUNT)) {
            //printf("index: %d shift-by: %d pre-result: 0x%x ", index, cc_net__wrap_diff(sequence, min_ack_sequence), result);
			result = result | (1 << cc_net__wrap_diff(sequence, min_ack_sequence));
            //printf("post-result: 0x%x\n", result);
		}
		index = CC_NET__WRAP_INC(index, CC_NET_MAX_SEQUENCE_BUFFER_COUNT);
	}
	return (result);
}

CC_NET_DEFINE cc_net_virtual_connection_packet_header_t cc_net_virtual_connection_prepare_header(cc_net_virtual_connection_t* connection)
{
    cc_net_virtual_connection_packet_header_t result = {};

    if (connection) {
        result.sequence = connection->local_sequence;
        result.ack = connection->remote_sequence;
        result.ack_bitfield = cc_net__ack_bitfield(connection);

        connection->local_sequence = CC_NET__WRAP_INC(connection->local_sequence, CC_NET_MAX_SEQUENCE_VALUE);
    }

    return (result);
}

static void cc_net__sequence_buffer_push(cc_net_sequence_buffer_t* buffer, uint32_t sequence)
{
	buffer->start = CC_NET__WRAP_DEC(buffer->start, CC_NET_MAX_SEQUENCE_BUFFER_COUNT);
	buffer->data[buffer->start] = sequence;
	if (buffer->start == buffer->end) {
		buffer->end = CC_NET__WRAP_DEC(buffer->end, CC_NET_MAX_SEQUENCE_BUFFER_COUNT);
	}
	return;
}

static void print_seq_buf(cc_net_sequence_buffer_t* buffer) {
    int32_t index = buffer->start;
	while (index != buffer->end) {
        //printf("sequence[%d:%d:%d] = %d\n", buffer->start, index, buffer->end, buffer->data[index]);
		index = CC_NET__WRAP_INC(index, CC_NET_MAX_SEQUENCE_BUFFER_COUNT);
	}
}

CC_NET_DEFINE void cc_net_virtual_connection_consume_header(cc_net_virtual_connection_t* connection, cc_net_virtual_connection_packet_header_t header)
{
    if (connection) {
        if (cc_net__most_recent_sequence(header.sequence, connection->remote_sequence, CC_NET_MAX_SEQUENCE_VALUE)) {
            connection->remote_sequence = header.sequence;
        }
        cc_net__sequence_buffer_push(&connection->sequence_buffer, header.sequence);
        print_seq_buf(&connection->sequence_buffer);
    }
    return;
}

#endif /* CC_NET_IMPLEMENTATION */

#endif /* CC_NET_H */

