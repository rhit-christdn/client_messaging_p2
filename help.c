#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER "137.112.38.47"
#define PORT 2526
#define BUFSIZE 1024
#define STUDENT_ID_LAST4 2902

// Internet checksum (16-bit)
unsigned short checksum(unsigned short *buf, int nwords) {
    unsigned long sum = 0;
    for (; nwords > 0; nwords--)
        sum += *buf++;
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    return (unsigned short)(~sum);
}

int main() {
    int sock;
    struct sockaddr_in serverAddr;
    uint8_t buffer[BUFSIZE];
    socklen_t addrLen = sizeof(serverAddr);
    char *payload = "hello";

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket failed");
        return 1;
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr(SERVER);

    memset(buffer, 0, BUFSIZE);

    int payload_len = strlen(payload);
    int index = 0;

    // --- Build header ---
    buffer[index++] = 12; // version
    buffer[index++] = (STUDENT_ID_LAST4 >> 8) & 0xFF; // srcPort high byte
	buffer[index++] = STUDENT_ID_LAST4 & 0xFF;        // srcPort low byte
	buffer[index++] = (0x1874 >> 8) & 0xFF;             // dstPort high byte
	buffer[index++] = 0x1874 & 0xFF;                    // dstPort low byte
	
	uint16_t len_type = ( ((payload_len & 0x0FFF) << 4) | 0x0 ); // type = 0
	buffer[index++] = (len_type >> 8) & 0xFF;
	buffer[index++] = len_type & 0xFF;


    // --- Copy payload ---
    memcpy(buffer + index, payload, payload_len);
    index += payload_len;

    // --- Insert 1-byte buffer if packet length is odd (before checksum) ---
    if (index % 2 != 0)
        buffer[index++] = 0;

    // --- Reserve 2 bytes for checksum ---
    int checksum_index = index;
    buffer[index++] = 0;
    buffer[index++] = 0;

    // --- Compute checksum over entire packet ---
    unsigned short *buf16 = (unsigned short *)buffer;
    int nwords = index / 2;
    unsigned short csum = checksum(buf16, nwords);
    printf("%d\n", csum);
    memcpy(buffer + checksum_index, &csum, 2);
	
	printf("Buffer contents:\n");
    for (int i = 0; i < index; i++) {
        printf("buffer[%d] = %3d (0x%02X)\n", i, buffer[i], buffer[i]);
    }
	
    printf("Sending RHP message: %s\n", payload);

    // --- Send packet ---
    if (sendto(sock, buffer, index, 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("sendto failed");
        return 1;
    }

    // --- Receive response ---
    int nBytes = recvfrom(sock, buffer, BUFSIZE, 0, (struct sockaddr *)&serverAddr, &addrLen);
    if (nBytes < 8) {
        printf("Received message too short\n");
        return 1;
    }

    int idx = 0;
    uint8_t version = buffer[idx++];
    uint16_t resp_srcPort, resp_dstPort;
    memcpy(&resp_srcPort, buffer + idx, 2); idx += 2;
    memcpy(&resp_dstPort, buffer + idx, 2); idx += 2;
    uint16_t resp_len_type;
    memcpy(&resp_len_type, buffer + idx, 2); idx += 2;

    resp_srcPort = ntohs(resp_srcPort);
    resp_dstPort = ntohs(resp_dstPort);
    resp_len_type = ntohs(resp_len_type);

    uint16_t resp_length = (resp_len_type >> 4) & 0x0FFF;
    uint8_t resp_type = resp_len_type & 0x0F;

    // --- Determine where payload starts ---
    int header_len = 7;
    int payload_start = header_len;

    // --- Extract checksum from packet ---
    uint16_t resp_checksum;
    memcpy(&resp_checksum, buffer + nBytes - 2, 2);
    resp_checksum = ntohs(resp_checksum);

    // --- Verify checksum ---
    buffer[nBytes - 2] = 0;
    buffer[nBytes - 1] = 0;
    buf16 = (unsigned short *)buffer;
    nwords = nBytes / 2;
    unsigned short calc_checksum = checksum(buf16, nwords);
    calc_checksum = ntohs(calc_checksum);

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
        printf(" Payload: %.*s\n", resp_length, buffer + payload_start);
    } else {
        printf(" checksum failed, discarding packet\n");
    }

    close(sock);
    return 0;
}
