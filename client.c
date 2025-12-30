#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio_ext.h>
#include <sys/stat.h>
#include <fcntl.h>

// struct packet
// {
//     int opcode;
//     union
//     {
//         struct
//         {
//             char buffer[512];
//             int n_flag;
//             int pack_no;
//         }data_pack;

//         struct
//         {
//             char ack[20];
//             int packet_no;
//         }ack_pack;
//     }body;
// };

// TFTP OpCodes
typedef enum
{
    RRQ = 1,  // Read Request
    WRQ = 2,  // Write Request
    DATA = 3, // Data Packet
    ACK = 4,  // Acknowledgment
    ERROR = 5 // Error Packet
} tftp_opcode;

// TFTP Packet Structure
struct packet
{
    uint16_t opcode; // Operation code (RRQ/WRQ/DATA/ACK/ERROR)
    union
    {
        struct
        {
            char filename[256];
            char mode[8]; // Typically "octet"
        } request;        // RRQ and WRQ
        struct
        {
            uint16_t pack_no;
            int n_flag;
            char buffer[512];
        } data_pack; // DATA
        struct
        {
            char ack[20];
            uint16_t packet_no;
        } ack_pack; // ACK
        struct
        {
            uint16_t error_code;
            char error_msg[512];
        } error_packet; // ERROR
    } body;
};

int validate_ip(char *ip)
{
    int i = 0, char_count = 0, dot_count = 0;
    while (ip[i] != '\0')
    {
        if (ip[i] == '.')
        {
            if (char_count > 3 || char_count == 0)
            {
                return -1;
            }
            char_count = 0;
            dot_count++;
        }
        else
        {
            char_count++;
        }
        i++;
    }
    if (dot_count != 3)
    {
        return -1;
    }

    return 0;
}

// UDP Client
int main()
{
    int choice;
    char ip[20];
    char filename[25];
    char arr[20];
    int sfd, nsfd;
    socklen_t len;
    struct sockaddr_in server, client;

    do
    {
        printf("1. Connect\n");
        printf("2. Put\n");
        printf("3. Get\n");
        printf("4. Mode\n");
        printf("5. Exit\n");

        scanf("%d", &choice);

        switch (choice)
        {
            case 1:
            {
                printf("Enter the IP address : ");
                scanf("%s", ip);
                __fpurge(stdin);
                if (validate_ip(ip) == -1)
                {
                    printf("ERROR : Enter the correct IP Address\n");
                }

                sfd = socket(AF_INET, SOCK_DGRAM, 0);

                server.sin_family = AF_INET;
                server.sin_port = 4000;
                server.sin_addr.s_addr = inet_addr(ip);
                memset(&(server.sin_zero), '\0', 8);

                len = sizeof(struct sockaddr_in);
                break;
            }
            case 2:
            {
                printf("Enter the file name : ");
                scanf("%s", filename);
                __fpurge(stdin);
                int fd;
                fd = open(filename, O_RDONLY);
                if (fd == -1)
                {
                    perror(filename);
                    break;
                }
                sendto(sfd, "2", 2, 0, (struct sockaddr *)&server, sizeof(struct sockaddr_in));
                sendto(sfd, filename, 25, 0, (struct sockaddr *)&server, sizeof(struct sockaddr_in));

                recvfrom(sfd, arr, 20, 0, (struct sockaddr *)&server, &len);

                if (strcmp(arr, "Success") != 0)
                {
                    break;
                }

                struct packet P;
                int ret = 1;
                P.body.data_pack.pack_no = 0;
                P.body.data_pack.n_flag = 512;
                int packet_no = 0;
                while (ret > 0)
                {
                    memset(&P, 0, sizeof(P));
                    ret = read(fd, P.body.data_pack.buffer, 512);
                    printf("Ret : %d\n", ret);
                    if (ret == 0)
                    {
                        break;
                    }
                    packet_no++;
                    P.body.data_pack.pack_no = packet_no;
                    P.opcode = DATA;

                    if (ret < 512)
                    {
                        P.body.data_pack.n_flag = ret;
                    }
                    int count = 0;
                    do
                    {
                        printf("Sending : %d\n", P.body.data_pack.pack_no);
                        sendto(sfd, &P, sizeof(P), 0, (struct sockaddr *)&server, sizeof(struct sockaddr_in));
                        recvfrom(sfd, &P, sizeof(P), 0, (struct sockaddr *)&server, &len);
                        printf("Received : %d", P.body.ack_pack.packet_no);
                        if (P.opcode == ACK)
                        {
                            printf("%s\n", P.body.ack_pack.ack);
                            if (strcmp(P.body.ack_pack.ack, "Failure") == 0)
                            {
                                count = 1;
                                printf("Packet Corrupt!\n");
                            }
                        }
                    } while (count == 1);
                }
                break;
            }
            case 3:
            {
                printf("Enter the file name : ");
                scanf("%s", filename);
                __fpurge(stdin);

                sendto(sfd, "3", 2, 0, (struct sockaddr *)&server, sizeof(struct sockaddr_in));
                sendto(sfd, filename, 25, 0, (struct sockaddr *)&server, sizeof(struct sockaddr_in));

                recvfrom(sfd, arr, 20, 0, (struct sockaddr *)&server, &len);

                if (strcmp(arr, "Success") != 0)
                {
                    break;
                }

                int fd;
                fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0666);
                if (fd == -1)
                {
                    perror(filename); // Print error if the destination file cannot be opened
                    sendto(sfd, "Failure", 20, 0, (struct sockaddr *)&server, sizeof(struct sockaddr_in));
                    continue;
                }
                sendto(sfd, "Success", 20, 0, (struct sockaddr *)&server, sizeof(struct sockaddr_in));
                int ret;
                int bytes, packet_count = 0;
                struct packet P;

                while (1)
                {
                    memset(&P, 0, sizeof(P));
                    bytes = recvfrom(sfd, &P, sizeof(P), 0, (struct sockaddr *)&server, &len);
                    printf("Received : %d\n", P.body.data_pack.pack_no);
                    if (bytes <= 0)
                    {
                        perror("recvfrom");
                    }
                    packet_count++;
                    if (P.opcode == DATA)
                    {
                        if (P.body.data_pack.n_flag)
                        {
                            ret = P.body.data_pack.n_flag;
                        }
                        else
                        {
                            ret = 512;
                        }

                        printf("Packet No. : %d\n", packet_count);

                        if (P.body.data_pack.pack_no != packet_count)
                        {
                            // printf("1");
                            P.opcode = ACK;
                            P.body.ack_pack.packet_no = P.body.data_pack.pack_no;
                            strcpy(P.body.ack_pack.ack, "Failure");
                            printf("Sending1 : %d\n", P.body.ack_pack.packet_no);
                            sendto(sfd, &P, sizeof(P), 0, (struct sockaddr *)&server, sizeof(struct sockaddr_in));
                            continue;
                        }
                        printf("Ret : %d\n", ret);
                        write(fd, P.body.data_pack.buffer, ret);
                        P.opcode = ACK;
                        P.body.ack_pack.packet_no = P.body.data_pack.pack_no;
                        strcpy(P.body.ack_pack.ack, "Success");
                        printf("Sending2 : %d\n", P.body.ack_pack.packet_no);
                        P.body.data_pack.pack_no = packet_count;
                        sendto(sfd, &P, sizeof(P), 0, (struct sockaddr *)&server, sizeof(struct sockaddr_in));
                        perror("sendto");
                        if (ret < 512)
                        {
                            break;
                        }
                    }
                }
                break;
            }
        }
    } while (choice != 5);

    // sendto(sfd, "Hello World", 20, 0, (struct sockaddr *)&server, sizeof(struct sockaddr_in));

    // recvfrom(sfd, arr, 20, 0, (struct sockaddr *)&client, &len);

    // printf("Data : %s\n", arr);

    // close(sfd);

    return 0;
}