#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <vector>
#include <string>
#include <map>
#include <fcntl.h>
#include <unordered_map>
#include <iostream>
#include <string>
#include <sstream>
#include <sys/wait.h>
#include <signal.h>


#define MAIN_SERVER_UDP_PORT 44108                // Define main server port
//#define MAIN_SERVER_TCP_PORT "45108"
#define MAIN_SERVER_TCP_PORT 45108

#define BACKEND_SERVER_IP "127.0.0.1"         // Define IP address for backend servers (localhost in this case)
#define MAXBUFLEN 20000                        // Define maximum buffer length for receiving data
#define BACKLOG 10	 // how many pending connections queue will hold

using namespace std;

std::unordered_map<std::string, int> department_backend_mapping;  // Map to hold the department and corresponding backend server mapping

// Function to split a string by delimiter
std::vector<std::string> splitString(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::istringstream iss(str);
    std::string token;
    while (std::getline(iss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

//code from beej's guide
void sigchld_handler(int s)
{
    (void)s; // quiet unused variable warning

    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}


// get sockaddr, IPv4 or IPv6: //code from beej's guide
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main() {
    //code from beej's guide
//    int sockfdTCP, new_fd;  // listen on sock_fd, new connection on new_fd
//    struct addrinfo hints, *servinfo, *p;
//    struct sockaddr_storage their_addr_tcp; // connector's address information
//    socklen_t sin_size;
//    struct sigaction sa;
//    int yes=1;
//    char s[INET6_ADDRSTRLEN];
//    int rv;
//
//    //code from beej's guide
//    memset(&hints, 0, sizeof hints);
//    hints.ai_family = AF_UNSPEC;
//    hints.ai_socktype = SOCK_STREAM;
//    //hints.ai_flags = AI_PASSIVE; // use my IP
//
//    if ((rv = getaddrinfo("127.0.0.1", MAIN_SERVER_TCP_PORT, &hints, &servinfo)) != 0) {
//        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
//        return 1;
//    }
//
//    // loop through all the results and bind to the first we can//code from beej's guide
//    for(p = servinfo; p != NULL; p = p->ai_next) {
//        if ((sockfdTCP = socket(p->ai_family, p->ai_socktype,
//                                p->ai_protocol)) == -1) {
//            perror("server: socket");
//            continue;
//        }
//
//        if (setsockopt(sockfdTCP, SOL_SOCKET, SO_REUSEADDR, &yes,
//                       sizeof(int)) == -1) {
//            perror("setsockopt");
//            exit(1);
//        }
//
//        if (::bind(sockfdTCP, p->ai_addr, p->ai_addrlen) == -1) {
//            close(sockfdTCP);
//            perror("server: bind");
//            continue;
//        }
//
//        break;
//    }
//
//    freeaddrinfo(servinfo); // all done with this structure
//
//    if (p == NULL)  {
//        fprintf(stderr, "server: failed to bind\n");
//        exit(1);
//    }
//
//    if (listen(sockfdTCP, BACKLOG) == -1) {
//        perror("listen");
//        exit(1);
//    }
//
//    sa.sa_handler = sigchld_handler; // reap all dead processes
//    sigemptyset(&sa.sa_mask);
//    sa.sa_flags = SA_RESTART;
//    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
//        perror("sigaction");
//        exit(1);
//    }


    int sockfdTCP,new_fd;  // listen on sock_fd
    struct sockaddr_in addr;
    struct sockaddr_storage their_addr_tcp; // connector's address information
    socklen_t sin_size;
    int yes = 1;

    // Create the socket
    sockfdTCP = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfdTCP == -1) {
        perror("Error creating TCP socket");
        exit(1);
    }

    // Set socket options
    if (setsockopt(sockfdTCP, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        perror("Error setting TCP socket options");
        close(sockfdTCP);
        exit(1);
    }

    // Set up the address structure
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(MAIN_SERVER_TCP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY; // Bind to any address

    // Bind the socket
    if (::bind(sockfdTCP, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("Error binding TCP socket");
        close(sockfdTCP);
        exit(1);
    }

    // Start listening on the socket
    if (listen(sockfdTCP, BACKLOG) == -1) {
        perror("Error listening on TCP socket");
        close(sockfdTCP);
        exit(1);
    }

    int numbytes;                             // Variable to store number of bytes received

    std::cout << "Main server is up and running." << std::endl;  // Notify user that main server is running

    int sockfdUDP = socket(AF_INET, SOCK_DGRAM, 0);  // Create a UDP socket
    int flags = fcntl(sockfdUDP, F_GETFL, 0);       // Fetch current socket flags
    fcntl(sockfdUDP, F_SETFL, flags | O_NONBLOCK);  // Set socket to non-blocking mode

    if (sockfdUDP == -1) {  // Check for socket creation error
        perror("Main Server: Error creating socket");
        return 1;
    }

    sockaddr_in mainServerAddress{};                   // Structure to hold the main server's address information
    mainServerAddress.sin_family = AF_INET;            // Using IPv4
    mainServerAddress.sin_port = htons(MAIN_SERVER_UDP_PORT);  // Set port number after converting to network byte order
    mainServerAddress.sin_addr.s_addr = INADDR_ANY;    // Allow any incoming address

    if (::bind(sockfdUDP, (struct sockaddr*)&mainServerAddress, sizeof(mainServerAddress)) == -1) {  // Bind the socket to the defined address and port
        perror("Main Server: Error binding socket to port 33108");
        close(sockfdUDP);
        return 1;
    }

    std::vector<int> backendPorts = {41108, 42108, 43108};  // List of backend server ports
    std::vector<int> backendMapping = {0, 1, 2};  // Mapping to represent backend servers as A, B, or C

    for (size_t i = 0; i < backendPorts.size(); i++) {  // Iterate over each backend server to request department data

        sockaddr_in backendAddress{};                 // Structure to hold the backend server's address information
        backendAddress.sin_family = AF_INET;          // Using IPv4
        backendAddress.sin_port = htons(backendPorts[i]);  // Set port number after converting to network byte order
        inet_pton(AF_INET, BACKEND_SERVER_IP, &(backendAddress.sin_addr));  // Convert IP address to binary form

        std::string request = "send_departments";     // Message to request departments from backend servers
        sendto(sockfdUDP, request.c_str(), request.size(), 0, (struct sockaddr*)&backendAddress, sizeof(backendAddress));  // Send request to backend server

        char buf[MAXBUFLEN];                          // Buffer to receive the departments from the backend server
        struct sockaddr_storage their_addr_udp;           // Structure to hold the sender's address information
        socklen_t addr_len = sizeof(their_addr_udp);      // Variable to hold the length of the address
        int count = 0;                                // Counter to track the number of departments received

        int total_wait_time = 0;                      // Variable to track total wait time
        int wait_duration = 100000;                   // Duration to wait (0.1 seconds)
        int max_wait_time = 2000000;                  // Maximum time to wait (2 seconds)
        bool receivedFromThisBackend = false;         // Flag to check if data has been received from this backend server

        while (count < 50 && total_wait_time < max_wait_time) {  // Loop until we have received all departments or timed out
            numbytes = recvfrom(sockfdUDP, buf, MAXBUFLEN-1, 0, (struct sockaddr*)&their_addr_udp, &addr_len);  // Receive data from backend server
            if (numbytes > 0) {  // If data is received
                buf[numbytes] = '\0';  // Null-terminate the received data
                department_backend_mapping[buf] = backendMapping[i];  // Map the received department to the corresponding backend server
                //unsigned short port = ntohs(((struct sockaddr_in*)&their_addr_udp)->sin_port);  // Extract the sender's port number

                if (!receivedFromThisBackend) {  // If data is received for the first time from this backend
                    std::cout << "Main server has received the department list from Backend server "
                              << char('A' + backendMapping[i])  // Convert 0, 1, 2 to A, B, C respectively
                              << " using UDP over port " << MAIN_SERVER_UDP_PORT << std::endl;
                    receivedFromThisBackend = true;  // Set the flag to true
                }
                count++;  // Increment the counter
            } else if (errno == EWOULDBLOCK || errno == EAGAIN) {  // If no data is available to read
                usleep(wait_duration);  // Wait for the specified duration
                total_wait_time += wait_duration;  // Update the total wait time
            } else {  // If there's an error in recvfrom
                perror("recvfrom");
                break;
            }
        }
    }

    std::map<int, std::vector<std::string>> reverse_mapping;  // Create a map to display results by server
    for (const auto& entry : department_backend_mapping) {  // Iterate over the department-backend mapping
        reverse_mapping[entry.second].push_back(entry.first);  // Reverse the mapping
    }

    for (const auto& entry : reverse_mapping) {  // Iterate over the reversed mapping
        std::cout << "Server " << char('A' + entry.first) << ": ";  // Display server name
        std::string separator;
        for (const std::string& dept : entry.second) {  // Iterate over the departments for this server
            std::cout << separator <<dept;  // Display the department name
            separator = ", ";
        }
        std::cout << std::endl;  // Print a new line for clarity
    }

    int clientId = 0;

    while (true) {  // Infinite loop to continuously take queries from the user
        sin_size = sizeof their_addr_tcp;
        new_fd = accept(sockfdTCP, (struct sockaddr *) &their_addr_tcp, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }
        int clientType = -1; //0 for student, 1 for admin
        bool queryForDepartment = true;
        string clientPort;
        // Retrieve and print the client's dynamically assigned port
        if (their_addr_tcp.ss_family == AF_INET) {
            clientPort = to_string(ntohs(((struct sockaddr_in*)&their_addr_tcp)->sin_port));
        } else { // AF_INET6 for IPv6
            clientPort = to_string(ntohs(((struct sockaddr_in6*)&their_addr_tcp)->sin6_port));
        }

        //inet_ntop(their_addr_tcp.ss_family,
        //          get_in_addr((struct sockaddr *) &their_addr_tcp), s, sizeof s);
        //printf("server: got connection from %s\n", s);

        if (!fork()) { // this is the child process
            close(sockfdTCP); // child doesn't need the listener

            // ... after accepting connection ...
            char clientTypeBuffer[10]; // Buffer to store client type message
            int bytesReceived = recv(new_fd, clientTypeBuffer, sizeof(clientTypeBuffer) - 1, 0);
            if (bytesReceived > 0) {
                clientTypeBuffer[bytesReceived] = '\0'; // Null-terminate the string
                if (strcmp(clientTypeBuffer, "ADMIN") == 0) {
                    // Handle admin client
                    clientType = 1;
                } else if (strcmp(clientTypeBuffer, "STUDENT") == 0) {
                    // Handle student client
                    clientId++; // client ID increase 1 for every new client
                    clientType = 0;
                }
            }


            char buffer[1024];
            int bytes_received;
            while ((bytes_received = recv(new_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
                buffer[bytes_received] = '\0';  // Null-terminate received string

                std::vector<std::string> requestParts = splitString(buffer, ' ');
                std::string queryDepartment = requestParts[0];

                //std::string response;
                if (requestParts.size() == 2) { // Request format: "Department StudentID"
                    int studentId = std::stoi(requestParts[1]);
                    if (clientType == 0) { // clientType == 0 is student client
                        cout << "Main server has received the request on Student " << studentId << " in "
                             << queryDepartment << " from student client " << clientId << " using TCP over port "
                             << MAIN_SERVER_TCP_PORT << endl;
                    }
                    if (clientType == 1){ // clientType == 0 is admin client
                        cout << "Main server has received the request on Student " << studentId << " in "
                             << queryDepartment << " from admin using TCP over port "
                             << MAIN_SERVER_TCP_PORT << endl;
                        queryForDepartment = false;
                    }
                }
                else if (requestParts.size() == 1) { // Request format: "Department"
                    cout << "Main server has received the request on Department " << queryDepartment << " from Admin using TCP over port " << MAIN_SERVER_TCP_PORT <<endl;
                }

                string queryMessage = "";
                queryMessage += buffer;

                auto it = department_backend_mapping.find(queryDepartment);  // Check if the entered department exists in our mapping
                if (it == department_backend_mapping.end()) {  // If the department doesn't exist
                    cout << queryDepartment << " does not show up in server A, B, C" << std::endl;  // Notify the user
                    if(clientType==0){
                        cout<< "Main Server has sent \" "<< queryDepartment <<" : Not found\" to client "<< clientId << " using TCP over port "
                                                                  << MAIN_SERVER_TCP_PORT << endl;
                    }
                    else if(clientType==1){
                        cout<< "Main Server has sent \" "<< queryDepartment <<" : Not found\" to client Admin using TCP over port "
                                                      << MAIN_SERVER_TCP_PORT << endl;
                    }
                    string s = queryDepartment + ": Not found";
                    send(new_fd, s.c_str(), s.length(), 0);
                    continue;
                }
                int backendServer = it->second;  // Get the backend server corresponding to the department
                sockaddr_in backendAddress{};    // Structure to hold the backend server's address information
                backendAddress.sin_family = AF_INET;  // Using IPv4
                backendAddress.sin_port = htons(41108 + 1000 * backendServer);  // Set the port number after converting to network byte order
                inet_pton(AF_INET, BACKEND_SERVER_IP, &(backendAddress.sin_addr));  // Convert IP address to binary form

                sendto(sockfdUDP, queryMessage.c_str(), queryMessage.size(), 0, (struct sockaddr*)&backendAddress, sizeof(backendAddress));  // Send the query to the backend server
                std::cout << "Main Server has sent request to server " << char('A' + backendServer) << " using UDP over port " << MAIN_SERVER_UDP_PORT << std::endl;  // Notify the user about the request being sent

                usleep(100000);  // Wait for 0.1 seconds to ensure that the backend server has processed the request

                char buf[MAXBUFLEN];  // Buffer to receive data from the backend server
                struct sockaddr_storage their_addr_udp;  // Structure to hold the sender's address information
                socklen_t addr_len = sizeof(their_addr_udp);  // Variable to hold the length of the address
                numbytes = recvfrom(sockfdUDP, buf, MAXBUFLEN-1, 0, (struct sockaddr*)&their_addr_udp, &addr_len);  // Receive data from backend server
                int notFoundFlag = 0;
                if (numbytes > 35) {
                    buf[numbytes] = '\0';  // Null-terminate the received data
                } else {
                    buf[numbytes] = '\0';  // Null-terminate the received data
                    std::cout << "Main server has received \"" <<buf << "\" from server " << char('A' + backendServer) << std::endl;
                    notFoundFlag = 1;
                }
                string response="";
                response += buf;
                send(new_fd, response.c_str(), response.length(), 0);
                if(clientType == 0){
                    if(notFoundFlag == 0){ // if the student is not found, then skip the following print
                        cout << "Main server has received searching result of Student " << stoi(requestParts[1]) << " from sever "<< char('A' + backendServer) << std::endl;
                        cout << "Main Server has sent result(s) to student client " << clientId << " using TCP over port "
                             << MAIN_SERVER_TCP_PORT << endl;
                    }
                    else{ //if the student is not found, print the following
                        cout << "Main Server has sent message to student client " << clientId << " using TCP over port "
                             << MAIN_SERVER_TCP_PORT << endl;
                    }
                }
                else{
                    if(queryForDepartment){
                        cout << "Main Server has sent department statistics to Admin client using TCP over port "
                        << MAIN_SERVER_TCP_PORT << endl;
                    }
                    else{
                        if(notFoundFlag == 0){ // if the student is not found, then skip the following print
                            cout << "Main server has received searching result of Student " << stoi(requestParts[1]) << " from sever "<< char('A' + backendServer) << std::endl;
                            cout << "Main Server has sent result(s) to student client " << clientId << " using TCP over port "
                                 << MAIN_SERVER_TCP_PORT << endl;
                        }
                        else{
                            cout << "Main Server has sent message to Admin client using TCP over port "
                                 << MAIN_SERVER_TCP_PORT << endl;
                        }
                    }
                }

            }
            close(new_fd);
        }


    }

    close(sockfdUDP);  // Close the socket
    return 0;  // Exit the program
}
