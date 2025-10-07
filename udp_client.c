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

//Computes the checksum of the given buffer
unsigned short checksum(unsigned short *buf, int nwords) {
    unsigned long sum = 0;
	//sum all of the buffer
    for (; nwords > 0; nwords--)
        sum += *buf++;
    while (sum >> 16) //put carry into lower 16
        sum = (sum & 0xFFFF) + (sum >> 16);
    return (unsigned short)(~sum); // return 1's complement
}

//Defines rhp_header as based on project description
#pragma pack(push,1) // ensures the header doesn't chnage size to accomidate for spacing
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
     * just send rhponses 
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

	//create header
	char sendbuf[BUFSIZE];
    struct rhp_header header;

	//fill in info for the header
    header.version = 12;
    header.srcPort = htons(0x2902);
    header.dstPort = htons(0x1874);
    header.length_type = htons(((strlen(MESSAGE) & 0x0FFF) << 4) | 0);
    header.buffer  = 0x00;

	// copy header and payload for the send buffer
    int offset = 0;
    memcpy(sendbuf + offset, &header, sizeof(header));
    offset += sizeof(header);
    memcpy(sendbuf + offset, MESSAGE, strlen(MESSAGE));
    offset += strlen(MESSAGE);

    if (offset % 2 != 0) {                      // ensure even length
        sendbuf[offset++] = 0x00;
    }

	//computes the checksum
    unsigned short cksum = checksum((unsigned short*)sendbuf, offset/2);
    memcpy(sendbuf + offset, &cksum, 2);
    offset += 2;
	
	printf("Sending RHP message: %s\n", MESSAGE);
	
    /* send a message to the server */
    if (sendto(clientSocket, sendbuf, offset, 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("sendto failed");
        return 0;
    }

    /* Receive message from server */
    nBytes = recvfrom(clientSocket, buffer, BUFSIZE, 0, NULL, NULL);
	
	//print out all of the return values for the header
	struct rhp_header *rhp = (struct rhp_header *)buffer;
    printf("Received from server:\n");
    printf(" RHP version: %d\n", rhp->version);
	printf(" RHP type: %d\n", ntohs(rhp->length_type) & 0x000F);
    printf(" srcPort: %d (0x%X)\n", rhp->srcPort, rhp->srcPort);
    printf(" dstPort: %d (0x%X)\n", ntohs(rhp->dstPort), ntohs(rhp->dstPort));
    printf(" length: %d\n", (rhp->length_type >> 4) & 0x0FFF);
	
	//checks to see if the checksum passes
	unsigned short recv_cksum = *(uint16_t*)(buffer + nBytes - 2);
    if (checksum((unsigned short*)buffer, (nBytes-2)/2) == recv_cksum) {
        printf(" checksum passed\n");
    } else {
        printf(" checksum failed\n");
    }
	
	printf(" checksum: 0x%X\n", recv_cksum);
	
    close(clientSocket);
    return 0;
}