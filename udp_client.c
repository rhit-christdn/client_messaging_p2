/******************************************************************************
 * File: udp_client.c
 * Author: Wyatt Ronn and Dylan Christopherson
 * Due: 10/21/2025
 * 
 * Description:
 *   This program is a UDP client that communicates with a server
 *   using the RHP and RHMP. It demonstrates creating and sending 
 *   RHP messages with payloads of even and odd lengths, receiving 
 *   and validating RHP messages using a 16-bit Internet checksum, 
 *   creating and sending RHMP messages of different types, and 
 *   receiving and decoding RHMP responses including payloads and IDs.
 *
 ******************************************************************************/
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER "137.112.38.47" // Server's IP
#define PORT 2526 //Given port number
#define BUFSIZE 1024 //Buffer size

//Computes 16-bit internet checksum on a given buffer
unsigned short checksum(unsigned short *buf, int nwords) {
    unsigned long sum = 0;
    for (; nwords > 0; nwords--)
        sum += *buf++;
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    return (unsigned short)(~sum);
}

//Sends a RHP message with given payload to the given server
int send_rhp_message(int sock, struct sockaddr_in *serverAddr, const char *payload) {
    uint8_t buffer[BUFSIZE];
    memset(buffer, 0, BUFSIZE);

    int payload_len = strlen(payload);
    int index = 0;

	//RHP header infromation
    buffer[index++] = 12; //version
    buffer[index++] = (2902) & 0xFF; //source port LSB
    buffer[index++] = (2902 >> 8) & 0xFF; //source port MSP
    buffer[index++] = (0x1874) & 0xFF; //destination port LSB
    buffer[index++] = (0x1874 >> 8) & 0xFF; // destination port MSB
    buffer[index++] = (payload_len) & 0xF; //length LSB
    buffer[index++] = (payload_len >> 8) & 0xFF; //length MSP and type

	//padding for even length payloads
    if (payload_len % 2 == 0) {
        buffer[index++] = 0;
    }

	//copy payload into the buffer
    memcpy(buffer + index, payload, payload_len);
    index += payload_len;
	
	//Leave space for the checksum
    int checksum_index = index;
    buffer[index++] = 0;
    buffer[index++] = 0;

	//Calculate the checksum
    unsigned short csum = checksum((unsigned short *)buffer, index/2);
    memcpy(buffer + checksum_index, &csum, 2);

    printf("Sending RHP message: %s\n", payload);

	//sends message to socket
    if (sendto(sock, buffer, index, 0, (struct sockaddr *)serverAddr, sizeof(*serverAddr)) < 0) {
        perror("sendto failed");
        return -1;
    }

    return payload_len;
}

//Receives and interpret RHP response
int receive_rhp_response(int sock, struct sockaddr_in *serverAddr, int payload_len) {
    uint8_t buffer[BUFSIZE];
    socklen_t addrLen = sizeof(*serverAddr);

    int nBytes = recvfrom(sock, buffer, BUFSIZE, 0, (struct sockaddr *)serverAddr, &addrLen);

	//get header information out of buffer
    uint8_t version = buffer[0];
    uint16_t srcPort = buffer[1] | (buffer[2] << 8);
    uint16_t dstPort = buffer[3] | (buffer[4] << 8);
    uint16_t length = buffer[5] & 0xFF;
    length = ((buffer[6] & 0xF) << 8) | length;
    uint8_t type = buffer[6] & 0xF;

	//start index for payload
    int payload_start = (payload_len % 2 == 0) ? 8 : 7;

	//get the observed checksum
    uint16_t checksum2;
    memcpy(&checksum2, buffer + nBytes - 2, 2);

	//Recalculate the checksum
    buffer[nBytes-2] = 0;
    buffer[nBytes-1] = 0;
    unsigned short *buf16 = (unsigned short *)buffer;
    int nwords = nBytes / 2;
    unsigned short calc_checksum = checksum(buf16, nwords);

	//print all header information recieved
    printf("Message received:\n");
    printf(" RHP version: %d\n", version);
    printf(" RHP type: %d\n", type);
    printf(" srcPort: %d (0x%X)\n", srcPort, srcPort);
    printf(" dstPort: %d (0x%X)\n", dstPort, dstPort);
    printf(" length: %d\n", length);
    printf(" checksum: 0x%X\n", checksum2);
    printf(" checksum calculated: 0x%X\n", calc_checksum);

	//verifies the checksum
    if (calc_checksum == checksum2) {
        printf(" checksum passed\n");
        printf(" Payload: ");
        fwrite(buffer + payload_start, 1, length, stdout);
        printf("\n");
        return 1;
    } else {
		printf(" checksum failed\n");
		printf(" Payload: ");
        fwrite(buffer + payload_start, 1, length, stdout);
        printf("\n");
        return 0;
    }
}

//sends an RHMP message with a specified type
int send_rhmp_message(int sock, struct sockaddr_in *serverAddr, int type) {
    uint8_t buffer[BUFSIZE];
    uint8_t buffer2[BUFSIZE];
    memset(buffer, 0, BUFSIZE);
    memset(buffer2, 0, BUFSIZE);

	//Create RHMP header information
    int index2 = 0;
    uint16_t commID = 0x312;
    uint16_t rhmp_len = 0; 
    buffer2[index2++] = commID & 0xFF;
    buffer2[index2++] = ((commID >> 8) & 0x3F) | ((type & 0x3) << 6);
    buffer2[index2++] = ((rhmp_len & 0xF) << 4) | ((type >> 2) & 0xF);
    buffer2[index2++] = (rhmp_len >> 8) & 0x0F;

    int payload_len = index2;
	
	//create RHP header
    int index = 0;
    buffer[index++] = 12; 
    buffer[index++] = (2902) & 0xFF; 
    buffer[index++] = (2902 >> 8) & 0xFF;
    buffer[index++] = (0xECE) & 0xFF;
    buffer[index++] = (0xECE >> 8) & 0xFF;
    buffer[index++] = payload_len & 0xFF;
    buffer[index++] = ((payload_len >> 8) & 0x0F) | ((4 & 0x0F) << 4);
    buffer[index++] = 0;
	
	//put RHMP header into RHP payload
    memcpy(buffer + index, buffer2, payload_len);
    index += payload_len;

	//Calculate the checksum
    int checksum_index = index;
    buffer[index++] = 0;
    buffer[index++] = 0;
    unsigned short *buf16 = (unsigned short *)buffer;
    int nwords = index / 2;
    unsigned short csum = checksum(buf16, nwords);
    memcpy(buffer + checksum_index, &csum, 2);
	
	//print type of message
	if(type == 4){
		printf("Sending RHMP message: Message Request\n");
	}
	if(type == 16){
		printf("Sending RHMP message: ID Request\n");
	}

	//sends message through socket
    if (sendto(sock, buffer, index, 0, (struct sockaddr *)serverAddr, sizeof(*serverAddr)) < 0) {
        perror("sendto failed");
        return -1;
    }

    return payload_len;
}

//Receive and interpret RHMP messgae
int receive_rhmp_response(int sock, struct sockaddr_in *serverAddr, int payload_len) {
    uint8_t buffer[BUFSIZE];
    socklen_t addrLen = sizeof(*serverAddr);

    int nBytes = recvfrom(sock, buffer, BUFSIZE, 0, (struct sockaddr *)serverAddr, &addrLen);

	//Gets RHP header information
    uint8_t version = buffer[0];
    uint16_t srcPort = buffer[1] | (buffer[2] << 8);
    uint16_t dstPort = buffer[3] | (buffer[4] << 8);
    uint16_t length = buffer[5] & 0xFF;
    length = ((buffer[6] & 0xF) << 8) | length;
    uint8_t type = (buffer[6] << 8) & 0xF;

    int payload_start = (payload_len % 2 == 0) ? 8 : 7;

	//Recalculate the checksum
    uint16_t checksum2;
    memcpy(&checksum2, buffer + nBytes - 2, 2);

    buffer[nBytes - 2] = 0;
    buffer[nBytes - 1] = 0;
    unsigned short *buf16 = (unsigned short *)buffer;
    int nwords = nBytes / 2;
    unsigned short calc_checksum = checksum(buf16, nwords);

	//print all RHP information received
    printf("Message received:\n");
    printf(" RHP version: %d\n", version);
    printf(" RHP type: %d\n", type);
    printf(" srcPort: %d (0x%X)\n", srcPort, srcPort);
    printf(" dstPort: %d (0x%X)\n", dstPort, dstPort);
    printf(" length: %d\n", length);
    printf(" checksum: 0x%X\n", checksum2);
    printf(" checksum calculated: 0x%X\n", calc_checksum);
		
	//print all RHMP information received
	uint16_t comm_id = buffer[payload_start] | ((buffer[payload_start+1] & 0x3F) << 8);
	uint8_t type2 = ((buffer[payload_start+1] >> 6) & 0x3) | ((buffer[payload_start+2] & 0xF) << 2);
	uint16_t length2 = ((buffer[payload_start+2] >> 4) & 0xF) | (buffer[payload_start+3] << 4);

	//checks the checksum
    if (calc_checksum == checksum2) {
	    printf(" checksum passed\n");
		printf("\n");
		printf(" RHMP Comm_ID: %d (0x%X)\n", comm_id, comm_id);
    	printf(" RHMP type: %d\n", type2);
		printf(" RHMP length: %d\n", length2);
		printf(" Payload: ");
		if(type2 == 6){
			fwrite(buffer + payload_start + 4, 1, length2, stdout);
		}
		if(type2 == 24){//I put the id in little endian because that is what was most common in this project
			uint32_t id = buffer[payload_start+4] | (buffer[payload_start+5] << 8) | (buffer[payload_start+6] << 16) | (buffer[payload_start+7] << 24);
			printf("%d (0x%X)\n", id, id);
		}
    	printf("\n");
        return 1;
    } else {
		printf(" checksum failed\n");
		printf("\n");
		printf(" RHMP Comm_ID: %d (0x%X)\n", comm_id, comm_id);
    	printf(" RHMP type: %d\n", type2);
		printf(" RHMP length: %d\n", length2);
		printf(" Payload: ");
		if(type2 == 6){
			fwrite(buffer + payload_start + 4, 1, length2, stdout);
		}
		if(type2 == 24){
			uint32_t id = buffer[payload_start+4] | (buffer[payload_start+5] << 8) | (buffer[payload_start+6] << 16) | (buffer[payload_start+7] << 24);
			printf("%d (0x%X)\n", id, id);
		}
    	printf("\n");
        return 0;
    }
}

//starting point for the code
int main() {
    int sock;
    struct sockaddr_in serverAddr;

	//creates socket
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket failed");
        return 1;
    }

	//configure address
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr(SERVER);

	//1. one RHP control message with the string “hi” (odd length)
    int c1 = 0;
    while (!c1) { // repeats until a valid response is acheived
		int sent_len = send_rhp_message(sock, &serverAddr, "hello");
        c1 = receive_rhp_response(sock, &serverAddr, sent_len);
        printf("\n");
    }
	
	//2. one RHP control message with the string “hello” (even length)
    int c2 = 0;
    while (!c2) {
		int sent_len = send_rhp_message(sock, &serverAddr, "hi");
        c2 = receive_rhp_response(sock, &serverAddr, sent_len);
        printf("\n");
    }
	
	//3. one RHMP message of type Message_Request
	int c3 = 0;
    while (!c3) {
		int sent_len = send_rhmp_message(sock, &serverAddr, 4);
        c3 = receive_rhmp_response(sock, &serverAddr, sent_len);
        printf("\n");
    }

	//4. one RHMP message of type ID_Request
	int c4 = 0;
    while (!c4) {
		int sent_len = send_rhmp_message(sock, &serverAddr, 16);
        c4 = receive_rhmp_response(sock, &serverAddr, sent_len);
        printf("\n");
    }
	
    close(sock);
    return 0;
}