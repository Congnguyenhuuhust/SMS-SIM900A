#include <iostream>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <fstream>
#include <curl/curl.h>

#define UART_DEVICE "/dev/ttyS2"
#define BAUD_RATE B9600
#define API_BASE_URL "http://192.168.1.29:5555/otpwebhook"  // Địa chỉ URL cho REST API
#define USERNAME "congnh"  // Basic Auth username
#define PASSWORD "66668888"  // Basic Auth password

using namespace std;

// Open UART
int openUART() {
    int uart_fd = open(UART_DEVICE, O_RDWR | O_NOCTTY);
    if (uart_fd == -1) {
        cerr << "Error opening UART device." << endl;
        return -1;
    }

    struct termios options;
    tcgetattr(uart_fd, &options);
    options.c_cflag = BAUD_RATE | CS8 | CLOCAL | CREAD;
    options.c_iflag = IGNPAR;
    options.c_oflag = 0;
    options.c_lflag = 0;
    tcflush(uart_fd, TCIFLUSH);
    tcsetattr(uart_fd, TCSANOW, &options);

    return uart_fd;
}

// Send AT command to SIM900A
void sendATCommand(int uart_fd, const string& command) {
    string at_command = command + "\r";
    write(uart_fd, at_command.c_str(), at_command.length());
    usleep(500000);  // Delay to ensure command is processed 
}

// Read response from SIM900A
string readResponse(int uart_fd) {
    char buffer[256];
    int n = read(uart_fd, buffer, sizeof(buffer) - 1);
    if (n > 0) {
        buffer[n] = '\0';
        return string(buffer);
    }
    return "";
}

// Send SMS function
void sendSMS(int uart_fd, const string& recipient, const string& content) {
    sendATCommand(uart_fd, "AT+CMGF=1");  // Set text mode
    sendATCommand(uart_fd, "AT+CMGS=\"" + recipient + "\"");

    usleep(200000); // Wait for prompt 
    write(uart_fd, content.c_str(), content.length());
    write(uart_fd, "\x1A", 1);  // End message with Ctrl+Z
    usleep(500000);
    cout << "Sent sms to " << recipient << ": " << content << endl;
}

// Write received message to log file
void logReceivedMessage(const string& message) {
    ofstream logFile("sms.log", ios::app);
    if (logFile.is_open()) {
        logFile << message << endl;
        logFile.close();
    } else {
        cerr << "Unable to open log file." << endl;
    }
}

// Send message content to REST API
void sendToApi(const string& messageContent) {
    CURL* curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if(curl) {
        // Construct the full URL with message content as a query parameter
        string apiKey = "secret_key_123";
        string url = std::string(API_BASE_URL) + "?otp=" + curl_easy_escape(curl, messageContent.c_str(), 0) + "&api_key=" + apiKey;

        // Basic authentication
        string auth = string(USERNAME) + ":" + string(PASSWORD);
        curl_easy_setopt(curl, CURLOPT_USERPWD, auth.c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << endl;
        }

        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
}

// Check for received messages
void checkReceivedMessages(int uart_fd) {
    while (true) {
        sendATCommand(uart_fd, "AT+CMGR=1");  // Always read message at index 1
        usleep(1000000);  // Wait 3 seconds for response
        string response = readResponse(uart_fd);

        if (response.find("+CMGR:") != string::npos) {
            // Locate the start of the phone number by finding the first quote after "+CMGR:"
            size_t phoneStart = response.find("\"", response.find("\"", response.find("+CMGR:") + 8) + 1) + 1;
            size_t senderInfoEnd = response.find("\n", phoneStart) + 1; 
            string senderInfo = "From: " + response.substr(phoneStart, senderInfoEnd - phoneStart);

            // Find message content
            size_t messageContentStart = senderInfoEnd;
            size_t messageContentEnd = response.find("\n", messageContentStart);
            string messageContent = response.substr(messageContentStart, messageContentEnd - messageContentStart);

            // Display the filtered sender information and message content
            cout << senderInfo;
            cout << messageContent << endl;

            // Log the message to sms.log
            logReceivedMessage(messageContent);

            // Send the message content to REST API
            sendToApi(messageContent);

            // Delete all messages
            sendATCommand(uart_fd, "AT+CMGD=1,4");  // Delete all messages
        } else {
            usleep(1000000);  // 1-second delay for empty message
        }
    }
}

int main() { 
    int uart_fd = openUART();
    if (uart_fd == -1) return 1;

    string recipient, content;
    cout << "Recipient: ";
    getline(cin, recipient);
    cout << "Content: ";
    getline(cin, content);

    sendSMS(uart_fd, recipient, content);
    checkReceivedMessages(uart_fd);

    close(uart_fd);
    return 0;
}

