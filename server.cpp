// BackendServer.cpp

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>

#define SERVER_A 1
#define SERVER_B 2
#define SERVER_C 3

// Set the backend server's name for this instance
#ifndef BACKEND_SERVER_NAME // if not defined in the make file
#define BACKEND_SERVER_NAME SERVER_A // Change this to SERVER_A, SERVER_B, or SERVER_C
#endif
 
// Set the server port and data file based on server name
#if BACKEND_SERVER_NAME == SERVER_A
    #define BACKEND_SERVER_PORT 41108
    #define DATA_FILE "dataA.csv"
#elif BACKEND_SERVER_NAME == SERVER_B
    #define BACKEND_SERVER_PORT 42108
    #define DATA_FILE "dataB.csv"
#else
    #define BACKEND_SERVER_PORT 43108
    #define DATA_FILE "dataC.csv"
#endif

#define MAXBUFLEN 1000      // Define maximum buffer length for socket communication

using namespace std;       // Use the standard namespace

// Global data structures to hold department data and names
map<string, set<unsigned long long>> departmentData;
vector<string> departmentNames;

// Function to load department data from file
struct StudentRecord {
    std::string department;
    int id;
    std::vector<int> scores; // Store course scores
};

typedef std::map<int, StudentRecord> StudentMap;

StudentMap studentMap; // Declaration of the student records map
//std::vector<std::string> departmentNames;

void loadFile() {
    std::ifstream file(DATA_FILE);
    if (!file.is_open()) {    // Check if file was opened successfully
        cerr << "Unable to open " << DATA_FILE << endl;  // Print error message to standard error
        exit(1);
    }

    std::string line;
    std::set<std::string> departmentSet;

    // Skip the header row
    std::getline(file, line);

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        StudentRecord record;
        std::string score, id_str;

        // Read department and ID
        std::getline(iss, record.department, ',');
        std::getline(iss, id_str, ',');
        record.id = std::stoi(id_str);

        // Read scores
        while (std::getline(iss, score, ',')) {
            if (score.empty()) {
                // Handle empty items (no score)
                continue;
            } else {
                record.scores.push_back(std::stoi(score));
            }
        }

        // Insert the record into the map
        studentMap[record.id] = record;
        departmentSet.insert(record.department);
    }

    // Transfer distinct department names from set to vector
    departmentNames.assign(departmentSet.begin(), departmentSet.end());
}
// Helper function to calculate GPA
double calculateGPA(const StudentRecord& record) {
    if (record.scores.empty()) return 0.0;

    double sum = 0.0;
    for (int score : record.scores) {
        sum += score;
    }
    return sum / record.scores.size();
}

// Function to calculate percentage rank in department
double calculatePercentageRank(const StudentMap& records, int studentId) {
    // Find the student record
    auto it = records.find(studentId);
    if (it == records.end()) return -1; // Student not found

    double studentGPA = calculateGPA(it->second);
    int countHigherOrEqual = 0;
    int departmentCount = 0;

    // Compare GPA with other students in the same department
    for (const auto& entry : records) {
        if (entry.second.department == it->second.department) {
            departmentCount++;
            if (calculateGPA(entry.second) <= studentGPA) {
                countHigherOrEqual++;
            }
        }
    }

    return static_cast<double>(countHigherOrEqual) / departmentCount * 100.0;
}

struct DepartmentStats {
    double mean;
    double variance;
    double maxGPA;
    double minGPA;
};

// Function to calculate department statistics
DepartmentStats calculateDepartmentStats(const StudentMap& records, const std::string& department) {
    DepartmentStats stats = {0.0, 0.0, -1.0, std::numeric_limits<double>::max()};
    double sum = 0.0;
    std::vector<double> gpas;
    int count = 0;

    // Calculate sum and populate GPA list
    for (const auto& entry : records) {
        if (entry.second.department == department) {
            double gpa = calculateGPA(entry.second);
            gpas.push_back(gpa);
            sum += gpa;
            count++;
            stats.maxGPA = std::max(stats.maxGPA, gpa);
            stats.minGPA = std::min(stats.minGPA, gpa);
        }
    }

    if (count == 0) {
        stats.minGPA = -1.0; // Adjust if no students found in department
        return stats;
    }

    // Calculate mean
    stats.mean = sum / count;

    // Calculate variance
    double sumVariance = 0.0;
    for (double gpa : gpas) {
        sumVariance += (gpa - stats.mean) * (gpa - stats.mean);
    }
    stats.variance = sumVariance / count;

    return stats;
}

std::vector<std::string> splitString(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::istringstream iss(str);
    std::string token;
    while (std::getline(iss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

// Function to format student record response
std::string formatStudentResponse(int studentId, double gpa, double rank,string department) {
    std::ostringstream ss;
    ss << "The academic record for Student " << studentId << " in "<< department << " is:\n";
    ss << "Student GPA: " << gpa << "\n";
    ss << "Percentage Rank: " << rank ;
    return ss.str();
}

// Function to format department statistics response
std::string formatDepartmentStatsResponse(const DepartmentStats& stats, const std::string& department) {
    std::ostringstream ss;
    ss << "The academic statistics for Department " << department << " are:\n";
    ss << "Department GPA Mean: " << stats.mean << "\n";
    ss << "Department GPA Variance: " << stats.variance << "\n";
    ss << "Department Max GPA: " << stats.maxGPA << "\n";
    ss << "Department Min GPA: " << stats.minGPA ;
    return ss.str();
}

int main() {
    // Print initial server status message
    cout << "Server " << char('A' + BACKEND_SERVER_NAME - 1) << " is up and running using UDP on port " << BACKEND_SERVER_PORT << endl;

    loadFile();  // Load department data from file

    // Create a UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {  // Check if socket creation was successful
        perror(("Backend Server " + string(1, char('A' + BACKEND_SERVER_NAME - 1)) + ": Error creating socket").c_str());  // Print error message
        return 1;  // Return with error code 1
    }

    // Define the server's address structure
    sockaddr_in myAddress{};
    myAddress.sin_family = AF_INET;  // Use IPv4
    myAddress.sin_port = htons(BACKEND_SERVER_PORT);  // Convert port number to network byte order
    myAddress.sin_addr.s_addr = INADDR_ANY;  // Allow any incoming address

    // Bind the socket to the server's address
    if (::bind(sockfd, (struct sockaddr *) &myAddress, sizeof(myAddress)) == -1) {
        perror(("Backend Server " + string(1, char('A' + BACKEND_SERVER_NAME - 1)) + ": Error binding socket").c_str());  // Print error message
        close(sockfd);  // Close the socket
        return 1;  // Return with error code 1
    }

    char buf[MAXBUFLEN];  // Buffer to store incoming messages
    sockaddr_storage their_addr;  // Structure to store the client's address
    socklen_t addr_len = sizeof(their_addr);  // Variable to hold the length of the client's address
    int numBytes = recvfrom(sockfd, buf, MAXBUFLEN - 1, 0, (struct sockaddr *) &their_addr, &addr_len);  // Receive a message from a client
    buf[numBytes] = '\0';  // Null-terminate the received message

    // Check if the received message is a request for department names
    if (strcmp(buf, "send_departments") == 0) {
        for (const string &dept: departmentNames) {  // Loop through each department name
            sendto(sockfd, dept.c_str(), dept.size(), 0, (struct sockaddr *) &their_addr,
                   addr_len);  // Send the department name to the client
        }
        // Print status message indicating the department list was sent
        cout << "Server " << char('A' + BACKEND_SERVER_NAME - 1) << " has sent a department list to Main Server"
             << endl;


        while (true) {  // Infinite loop to keep the server running and listen to more requests
            int numBytes = recvfrom(sockfd, buf, MAXBUFLEN - 1, 0, (struct sockaddr *) &their_addr, &addr_len);
            if (numBytes == -1) {
                // Handle receive error
                continue;
            }
            buf[numBytes] = '\0';  // Null-terminate the received message

            // Print status message indicating the server received a request
            // cout << "Server " << char('A' + BACKEND_SERVER_NAME - 1) << " has received a request for " << buf << endl;


            std::vector<std::string> requestParts = splitString(buf, ' ');
            std::string response;
            if (requestParts.size() == 2) { // Request format: "Department StudentID"
                std::string department = requestParts[0];
                int studentId = std::stoi(requestParts[1]);
                cout << "Server " << char('A' + BACKEND_SERVER_NAME - 1) << " has received a student academic record query for Student " << studentId << " in " << department << endl;
                if (studentMap.find(studentId) == studentMap.end()) {
                    cout << "Student " << studentId << " does not show up in " << department << endl;
                    cout << "Server " << char('A' + BACKEND_SERVER_NAME - 1) << " has sent \"Student " << studentId << " not found\" to Main Server" << endl;
                    std::ostringstream ss;
                    ss << "Student " << studentId << ": Not found";
                    response = ss.str();

                }
                else {
                    double studentGPA = calculateGPA(studentMap[studentId]);
                    double rank = calculatePercentageRank(studentMap, studentId);
                    response = formatStudentResponse(studentId, studentGPA, rank, department);
                    std::ostringstream ss;
                    ss << "Server " << char('A' + BACKEND_SERVER_NAME - 1) << " calculated following academic statistics for Student " << studentId << " in "<< department << ":\n";
                    ss << "Student GPA: " << studentGPA << "\n";
                    ss << "Percentage Rank: " << rank << "\n";
                    cout << ss.str();
                }
                sendto(sockfd, response.c_str(), response.length(), 0, (struct sockaddr *) &their_addr, addr_len);
            }
            else if (requestParts.size() == 1) { // Request format: "Department"
                std::string department = requestParts[0];
                cout << "Server " << char('A' + BACKEND_SERVER_NAME - 1) << " has received a department academic statistics query for "  << department << endl;
                DepartmentStats stats = calculateDepartmentStats(studentMap, department);
                std::string response = formatDepartmentStatsResponse(stats, department);
                sendto(sockfd, response.c_str(), response.length(), 0, (struct sockaddr *) &their_addr, addr_len);
                std::ostringstream ss;
                ss << "Server " << char('A' + BACKEND_SERVER_NAME - 1) << " calculated following academic statistics for " << department << ":\n";
                ss << "Department GPA Mean: " << stats.mean << "\n";
                ss << "Department GPA Variance: " << stats.variance << "\n";
                ss << "Department Max GPA: " << stats.maxGPA << "\n";
                ss << "Department Min GPA: " << stats.minGPA << "\n";
                cout << ss.str();
            }
            cout << "Server " << char('A' + BACKEND_SERVER_NAME - 1) << " has sent the result to Main Server"  << endl;
        }
    }


    close(sockfd);  // Close the socket
    return 0;  // Return success code
}
