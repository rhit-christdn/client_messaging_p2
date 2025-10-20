/************* UDP CLIENT CODE *******************/

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#define SERVER "137.112.38.47"
#define PORT 2526
#define BUFSIZE 1024
#define STUDENT_ID_LAST4 2904

#define RESERVED_TYPE 0
#define MESSAGE_REQUEST_TYPE 4
#define MESSAGE_RESPONSE_TYPE 6
#define ID_REQUEST_TYPE 16
#define ID_RESPONSE_TYPE 24

#define RHP_TYPE 0
#define RHMP_TYPE 4


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

int sendMSG(struct sockaddr_in serverAddr, int clientSocket, const char* payload, int payload_len, uint8_t type) {
	//create header
	char buffer[BUFSIZE];
    memset(buffer, 0, BUFSIZE);

    int index = 0;

	//fill in info for the header
    buffer[index++] = 12;
    buffer[index++] = (STUDENT_ID_LAST4 >> 8) & 0xFF;
    buffer[index++] = STUDENT_ID_LAST4 & 0xFF;

    uint16_t dstport = 0x0ECE;

    if (type == RHP_TYPE) {
        uint16_t dstport = 0x1874;
    }
    buffer[index++] = (dstport >> 8) & 0xFF;
    buffer[index++] = dstport & 0xFF;

    uint16_t length_type = ((payload_len & 0x0FFF) << 4) | (type & 0x000F);
    buffer[index++] = (length_type >> 8) & 0xFF;
    buffer[index++] = length_type & 0xFF;


	// copy header and payload for the send buffer
    memcpy(buffer + index, &payload, payload_len);
    index += payload_len;

    if (index % 2 != 0) {                      // ensure even length
        buffer[index++] = 0x00;
    }

    int checkIndex = index; // position to insert checksum
    buffer[index++] = 0;
    buffer[index++] = 0;


	//computes the checksum
    unsigned short *buf16 = (unsigned short *)buffer;
    int nwords = index / 2;
    unsigned short csum = checksum(buf16, nwords);
    memcpy(buffer + checkIndex, &csum, 2);

    if (type == RHP_TYPE)
	printf("Sending RHP message: %s\n", payload);
    else
    printf("Sending RHMP message\n");
	
    /* send a message to the server */
    if (sendto(clientSocket, buffer, index, 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("sendto failed");
        return 0;
    }

    return 0;
}

int buildRHMPpayload(uint16_t comm_id, uint8_t type, char* out) {
    uint32_t hdr = ((uint32_t)(comm_id & 0x3FFF) << 18) | ((uint32_t)(type & 0x3F) << 12) | 0;
    hdr = htonl(hdr);
    memcpy(out, &hdr, sizeof(hdr));
    return sizeof(hdr);
}

int receiveMSG(int sock, struct sockaddr_in *serverAddr, int payload_len) {
    uint8_t buffer[BUFSIZE];
    socklen_t addrLen = sizeof(*serverAddr);
    
    /* Receive message from server */
    int nBytes = recvfrom(sock, buffer, BUFSIZE, 0, (struct sockaddr *)serverAddr, &addrLen);

    if (nBytes < 8) {
        perror("received message too short\n");
        return 0;
    }

    uint8_t version = buffer[0];
    uint16_t resp_srcPort = buffer[1] | (buffer[2] << 8);
    uint16_t resp_dstPort = buffer[3] | (buffer[4] << 8);
    uint16_t resp_length = buffer[5] & 0xFF;
    resp_length = ((buffer[6] & 0xFF) << 8) | resp_length;
    uint8_t type = buffer[6] & 0x0F;

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
    printf(" RHP type: %d\n", type);
    printf(" srcPort: %d (0x%X)\n", resp_srcPort, resp_srcPort);
    printf(" dstPort: %d (0x%X)\n", resp_dstPort, resp_dstPort);
    printf(" length: %d\n", resp_length);
    printf(" checksum: 0x%X\n", resp_checksum);
    printf(" checksum calculated: 0x%X\n", calc_checksum);
	
	//checks to see if the checksum passes
	unsigned short recv_cksum = ntohs(*(uint16_t*)(buffer + nBytes - 2));
    if (checksum((unsigned short*)buffer, (nBytes-2)/2) == recv_cksum) {
        printf(" checksum passed\n");
        printf(" checksum: 0x%X\n", recv_cksum);

        printf("\n-------------------\n\n");

        return 0; // indicate success
    } else {
        printf(" checksum failed\n");
        printf(" checksum: 0x%X\n", recv_cksum);

        printf("\n-------------------\n\n");

        return 1; // indicate failure
    }
}
	
int main() {
    int clientSocket, nBytes;
    char buffer[BUFSIZE];
    struct sockaddr_in clientAddr, serverAddr;

    /*Create UDP socket*/
    if ((clientSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("cannot create socket");
        return 0;
    }

    /* Configure settings in server address struct */
    memset((char*) &serverAddr, 0, sizeof (serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr(SERVER);

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

    sendMSG(serverAddr, clientSocket, "hi", 0); //odd length, RHP message
    while (receiveMSG(nBytes, clientSocket, buffer)){
        sendMSG(serverAddr, clientSocket, "hi", 0); // send until valid message received
    }

    sendMSG(serverAddr, clientSocket, "hello", 0); //even length, RHP message
    while (receiveMSG(nBytes, clientSocket, buffer)){
        sendMSG(serverAddr, clientSocket, "hello", 0); // send until valid message received
    }

    uint8_t rhmp_payload[4];
    buildRHMPpayload(0x312, MESSAGE_REQUEST_TYPE, 0, (char*)rhmp_payload);
    sendMSG(serverAddr, clientSocket, (char*)rhmp_payload, RHMP_TYPE); // RHMP message
    while (receiveMSG(nBytes, clientSocket, buffer)){
        sendMSG(serverAddr, clientSocket, (char*)rhmp_payload, RHMP_TYPE); // send until valid message received
    }

    uint8_t rhmp_payload1[4];
    buildRHMPpayload(0x312, ID_REQUEST_TYPE, 0, (char*)rhmp_payload1);
    sendMSG(serverAddr, clientSocket, (char*)rhmp_payload1, RHMP_TYPE); // RHMP message
    while (receiveMSG(nBytes, clientSocket, buffer)){
        sendMSG(serverAddr, clientSocket, (char*)rhmp_payload1, RHMP_TYPE); // send until valid message received
    }

    close(clientSocket);
    return 0;
}