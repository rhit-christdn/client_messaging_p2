/************* UDP CLIENT CODE *******************/

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

#define SERVER "137.112.38.47"
#define MESSAGE "hello"
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

#pragma pack(push,1)
struct rhp_header {
    uint8_t  version;
    uint16_t srcPort;
    uint16_t dstPort;
    uint16_t length_type;
    uint8_t  buffer;
};
#pragma pack(pop)
	
int main() {
    int clientSocket, nBytes;
    char buffer[BUFSIZE];
    struct sockaddr_in clientAddr, serverAddr;

    /*Create UDP socket*/
    if ((clientSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("cannot create socket");
        return 0;
    }

    /* Bind to an arbitrary return address.
     * Because this is the client side, we don't care about the address 
     * since no application will initiate communication here - it will 
     * just send responses 
     * INADDR_ANY is the IP address and 0 is the port (allow OS to select port) 
     * htonl converts a long integer (e.g. address) to a network representation 
     * htons converts a short integer (e.g. port) to a network representation */
    memset((char *) &clientAddr, 0, sizeof (clientAddr));
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    clientAddr.sin_port = htons(0);

    if (bind(clientSocket, (struct sockaddr *) &clientAddr, sizeof (clientAddr)) < 0) {
        perror("bind failed");
        return 0;
    }

    /* Configure settings in server address struct */
    memset((char*) &serverAddr, 0, sizeof (serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr(SERVER);
    memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);

	char sendbuf[BUFSIZE];
    struct rhp_header header;

    header.version = 12;
    header.srcPort = htons(2902);
    header.dstPort = htons(0x1874);
    header.length_type = htons(((strlen(MESSAGE) & 0x0FFF) << 4) | 0);
    header.buffer  = 0x00;

    int offset = 0;
    memcpy(sendbuf + offset, &header, sizeof(header));
    offset += sizeof(header);
    memcpy(sendbuf + offset, MESSAGE, strlen(MESSAGE));
    offset += strlen(MESSAGE);

    if (offset % 2 != 0) {                      // ensure even length
        sendbuf[offset++] = 0x00;
    }

    unsigned short cksum = checksum((unsigned short*)sendbuf, offset/2);
    memcpy(sendbuf + offset, &cksum, 2);
    offset += 2;
	
	printf("Sending RHP message: %s\n", MESSAGE);
	
    /* send a message to the server */
    if (sendto(clientSocket, MESSAGE, strlen(MESSAGE), 0,
            (struct sockaddr *) &serverAddr, sizeof (serverAddr)) < 0) {
        perror("sendto failed");
        return 0;
    }

    /* Receive message from server */
    nBytes = recvfrom(clientSocket, buffer, BUFSIZE, 0, NULL, NULL);
	
	unsigned short recv_cksum = *(uint16_t*)(buffer + nBytes - 2);
    if (checksum((unsigned short*)buffer, (nBytes-2)/2) == recv_cksum) {
        printf("Checksum passed.\n");
    } else {
        printf("Checksum failed.\n");
    }
	
	struct rhp_header *resp = (struct rhp_header *)buffer;
    printf("Received from server: %s\n", buffer);
    printf(" RHP version: %d\n", resp->version);
    printf(" srcPort: %d (0x%X)\n", ntohs(resp->srcPort), ntohs(resp->srcPort));
    printf(" dstPort: %d (0x%X)\n", ntohs(resp->dstPort), ntohs(resp->dstPort));
    printf(" length: %d\n", ntohs((resp->length_type >> 4) & 0x0FFF));
    printf(" type: %d\n", resp->length_type & 0x0F);
    printf(" checksum: 0x%X\n", recv_cksum);
	
    close(clientSocket);
    return 0;
}