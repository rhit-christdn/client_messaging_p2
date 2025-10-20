#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER "137.112.38.47"
#define PORT 2526
#define BUFSIZE 1024

unsigned short checksum(unsigned short *buf, int nwords) {
    unsigned long sum = 0;
    for (; nwords > 0; nwords--)
        sum += *buf++;
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    return (unsigned short)(~sum);
}

int send_rhp_message(int sock, struct sockaddr_in *serverAddr, const char *payload) {
    uint8_t buffer[BUFSIZE];
    memset(buffer, 0, BUFSIZE);

    int payload_len = strlen(payload);
    int index = 0;


    buffer[index++] = 12;
    buffer[index++] = (2902) & 0xFF;
    buffer[index++] = (2902 >> 8) & 0xFF;
    buffer[index++] = (0x1874) & 0xFF;
    buffer[index++] = (0x1874 >> 8) & 0xFF;
    buffer[index++] = (payload_len) & 0xF;
    buffer[index++] = (payload_len >> 8) & 0xFF;

    if (payload_len % 2 == 0) {
        buffer[index++] = 0;
    }

    memcpy(buffer + index, payload, payload_len);
    index += payload_len;
	
    int checksum_index = index;
    buffer[index++] = 0;
    buffer[index++] = 0;

    unsigned short *buf16 = (unsigned short *)buffer;
    int nwords = index / 2;
    unsigned short csum = checksum(buf16, nwords);
    memcpy(buffer + checksum_index, &csum, 2);

    printf("Sending RHP message: %s\n", payload);

    if (sendto(sock, buffer, index, 0, (struct sockaddr *)serverAddr, sizeof(*serverAddr)) < 0) {
        perror("sendto failed");
        return -1;
    }

    return payload_len;
}

int receive_rhp_response(int sock, struct sockaddr_in *serverAddr, int payload_len) {
    uint8_t buffer[BUFSIZE];
    socklen_t addrLen = sizeof(*serverAddr);

    int nBytes = recvfrom(sock, buffer, BUFSIZE, 0, (struct sockaddr *)serverAddr, &addrLen);
    if (nBytes < 8) {
        printf("Received message too short\n");
        return 0;
    }

    uint8_t version = buffer[0];
    uint16_t resp_srcPort = buffer[1] | (buffer[2] << 8);
    uint16_t resp_dstPort = buffer[3] | (buffer[4] << 8);
    uint16_t resp_length = buffer[5] & 0xFF;
    resp_length = ((buffer[6] & 0xF) << 8) | resp_length;
    uint8_t resp_type = buffer[6] & 0xF;

    int payload_start = (payload_len % 2 == 0) ? 8 : 7;

    uint16_t resp_checksum;
    memcpy(&resp_checksum, buffer + nBytes - 2, 2);

    buffer[nBytes - 2] = 0;
    buffer[nBytes - 1] = 0;
    unsigned short *buf16 = (unsigned short *)buffer;
    int nwords = nBytes / 2;
    unsigned short calc_checksum = checksum(buf16, nwords);

    printf("Message received:\n");
    printf(" RHP version: %d\n", version);
    printf(" RHP type: %d\n", resp_type);
    printf(" srcPort: %d (0x%X)\n", resp_srcPort, resp_srcPort);
    printf(" dstPort: %d (0x%X)\n", resp_dstPort, resp_dstPort);
    printf(" length: %d\n", resp_length);
    printf(" checksum: 0x%X\n", resp_checksum);
    printf(" checksum calculated: 0x%X\n", calc_checksum);

    if (calc_checksum == resp_checksum) {
        printf(" checksum passed\n");
        printf(" Payload: ");
        fwrite(buffer + payload_start, 1, resp_length, stdout);
        printf("\n");
        return 1;
    } else {
		printf(" checksum failed\n");
        return 0;
    }
}

int main() {
    int sock;
    struct sockaddr_in serverAddr;
    char *payload = "hello";

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket failed");
        return 1;
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr(SERVER);

    int payload_len = strlen(payload);
    int check = 0;

    while (!check) {
		int sent_len = send_rhp_message(sock, &serverAddr, payload);
        check = receive_rhp_response(sock, &serverAddr, sent_len);
        printf("\n");
    }

    close(sock);
    return 0;
}

