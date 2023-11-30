/*
** client.c -- a stream socket client demo
*/
#include <iostream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sstream>

using namespace std;
#include <arpa/inet.h>

#define PORT "45108" // the port client will be connecting to

#define MAXDATASIZE 2048 // max number of bytes we can get at once

// get sockaddr, IPv4 or IPv6: //code from beej's guide
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main()
{
    //code from beej's guide
	int sockfd, numbytes;
	char buf[1024];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

    //const char* serverAddress = "localhost";

    //code from beej's guide
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

    //code from beej's guide
    if ((rv = getaddrinfo("127.0.0.1", PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can //code from beej's guide
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			perror("client: connect");
			close(sockfd);
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

    struct sockaddr_in localAddress;
    socklen_t addressLength = sizeof(localAddress);
    getsockname(sockfd, (struct sockaddr*)&localAddress, &addressLength);
    //cout << "Client's dynamic port: " << ntohs(localAddress.sin_port) << endl;
    int dyport = ntohs(localAddress.sin_port);


    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	//printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

    // Infinite loop to keep the connection alive
    cout << "Client is up and running." << endl;
    const char* clientType = "STUDENT"; // Or "ADMIN" for admin client
    if (send(sockfd, clientType, strlen(clientType), 0) == -1) {
        perror("send client type");
        // Handle error
    }

    while (1) {
        std::string queryDepartment;  // Variable to store the user's query for department name
        std::string queryStudentID;   // Variable to store the user's query for student ID
        std::string queryMessage;     // Variable to store the full query message
        int studentID;

        std::cout << "Department name: ";
        std::getline(std::cin, queryDepartment);

        std::cout << "Student ID: ";
        std::getline(std::cin, queryStudentID);

        if (!queryStudentID.empty()) {
            std::istringstream iss(queryStudentID);
            if (!(iss >> studentID) || !(iss.eof())) {
                // If input is not an integer or if there's additional input after the integer
                std::cout << "Error: Student ID must be an integer. Please try again." << std::endl;
                continue; // Go to the next iteration of the loop (end of the request)
            }
            queryMessage = queryDepartment + " " + queryStudentID;
        } else {
            std::cout << "Error: Student ID must be an integer. Please try again." << std::endl;
            continue;
        }


        if (send(sockfd, queryMessage.c_str(), queryMessage.length(), 0) == -1) {
            perror("send");
            break;
        }
        cout <<"Client has sent " << queryDepartment <<" and Student " << queryStudentID << " to Main Server using TCP over port "<< dyport << endl;

        numbytes = recv(sockfd, buf, sizeof(buf) -1, 0);
        if (numbytes > 0) {
            buf[numbytes] = '\0';
            cout << buf << endl;
            }
        else if (numbytes == 0) {
            cout << "Server closed the connection." << endl;
            //break;
        }
        else {
            perror("recv");
            //break;
        }
        cout<<"-----Start a new request-----"<<endl;
        //sleep(5);
    }
	close(sockfd);

	return 0;
}

