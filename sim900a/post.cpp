#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <cstring>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <sstream>
#include <ctime> 
#include <curl/curl.h> 

// URL
const std::string API_URL = "http://localhost:5555/otps"; 

// tách chuỗi thành mảng các dòng
std::vector<std::string> splitLines(const std::string& input) {
    std::vector<std::string> lines;
    std::istringstream stream(input);
    std::string line;

    while (std::getline(stream, line)) {
        lines.push_back(line);
    }

    return lines;
}

// tách chuỗi thành mảng các giá trị
std::vector<std::string> splitValues(const std::string& line) {
    std::vector<std::string> values;
    std::istringstream stream(line);
    std::string value;

    while (std::getline(stream, value, ',')) {
        values.push_back(value);
    }

    return values;
}

void sendATCommand(int uart, const char* command, int delayMicroseconds = 100000) {
    write(uart, command, strlen(command));
    write(uart, "\r", 1); 
    usleep(delayMicroseconds);
}

std::string removeSpecialCharacters(const std::string& input) {
    std::string result;
    for (char c : input) {
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '+' || c == ':' || c == '/'  || c == ' ') {
            result += c;
        }
    }
    return result; 
}

// gửi JSON tới API
void postToAPI(const std::string& json) {
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, API_URL.c_str()); 
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        res = curl_easy_perform(curl);

        if(res != CURLE_OK) {
            std::cerr << "Lỗi khi gọi API: " << curl_easy_strerror(res) << std::endl;
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
}

// lấy timestamp
std::string getCurrentTimestamp() {
    std::time_t now = std::time(nullptr);  
    return std::to_string(now);  
}

bool readResponse(int uart) {
    char buffer[256];
    int bytesRead = 0;
    std::string response = "";

    // Đọc phản hồi từ SIM900A và lưu vào response
    while ((bytesRead = read(uart, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytesRead] = '\0'; 
        response += buffer;
    }

    // Tách chuỗi thành các dòng
    std::vector<std::string> lines = splitLines(response);

    // Kiểm tra tin nhắn
    if (lines.empty() || response.find("+CMGR:") == std::string::npos) {
        return false; 
    }

    // Tách dòng thành mảng
    std::vector<std::vector<std::string>> allValues;
    for (const auto& line : lines) {
        allValues.push_back(splitValues(line));
    }

    // lấy thông tin sms
    if (allValues.size() > 2) {
        std::string json;
        std::string timestamp = getCurrentTimestamp(); // Lấy thời gian thực và chuyển thành chuỗi

        json = "{\n\"id\": \"" + timestamp + "\",\n\"Sender\": \"" + removeSpecialCharacters(allValues[1][1]) + "\",\n\"date\": \"" + removeSpecialCharacters(allValues[1][3]) + " " + removeSpecialCharacters(allValues[1][4]) + "\",\n\"sms\": \"" + removeSpecialCharacters(allValues[2][0]) + "\",\n\"receiver\": \"0833613720\"\n}";
        
        std::cout << json << std::endl;
        
        // gửi JSON
        postToAPI(json);

        return true; // Có sms
    }

    return false; // Không có sms
}

int main() {
    // Mở cổng UART2
    int uart = open("/dev/ttyS2", O_RDWR | O_NOCTTY | O_NDELAY);
    if (uart == -1) {
        std::cerr << "Không thể mở UART." << std::endl;
        return 1;
    }

    // Thiết lập UART
    struct termios options;
    tcgetattr(uart, &options);
    options.c_cflag = B9600 | CS8 | CLOCAL | CREAD; // 9600 baud, 8 bit dữ liệu, không parity, 1 stop bit
    options.c_iflag = IGNPAR;
    options.c_oflag = 0;
    options.c_lflag = 0;
    tcflush(uart, TCIFLUSH);
    tcsetattr(uart, TCSANOW, &options);

    sendATCommand(uart, "AT+CMGF=1");

    while (true) {
        // Đọc tin nhắn tại địa chỉ 1
        sendATCommand(uart, "AT+CMGR=1");

        bool newMessage = readResponse(uart);  // Kiểm tra có tin nhắn

        if (newMessage) {
            // Tin nhắn đã được hiển thị
        } else {
            // Không có tin nhắn mới
            usleep(1000000);  
        }

        // Xóa toàn bộ tin nhắn
        sendATCommand(uart, "AT+CMGD=1,4", 500000);

        // sleep 5s
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    close(uart);
    return 0;
}


