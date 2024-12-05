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
#include <curl/curl.h> // Thư viện libcurl để gửi yêu cầu HTTP

// Khai báo URL ở đầu chương trình
const std::string API_URL = "http://localhost:5555/otps"; // Đường dẫn API dễ chỉnh sửa

// Hàm tách chuỗi thành mảng các dòng
std::vector<std::string> splitLines(const std::string& input) {
    std::vector<std::string> lines;
    std::istringstream stream(input);
    std::string line;

    while (std::getline(stream, line)) {
        lines.push_back(line);
    }

    return lines;
}

// Hàm tách chuỗi thành mảng các giá trị
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
    write(uart, "\r", 1); // Gửi ký tự kết thúc dòng
    usleep(delayMicroseconds); // Chờ một khoảng thời gian để SIM900A xử lý
}

std::string removeSpecialCharacters(const std::string& input) {
    std::string result;
    for (char c : input) {
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '+' || c == ':' || c == '/') {
            result += c;
        }
    }
    return result; 
}

// Hàm gửi JSON tới API
void postToAPI(const std::string& json) {
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, API_URL.c_str());  // Sử dụng URL khai báo ở đầu
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

bool readResponse(int uart) {
    char buffer[256];
    int bytesRead = 0;
    std::string response = "";

    // Đọc phản hồi từ SIM900A và lưu vào response
    while ((bytesRead = read(uart, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytesRead] = '\0'; // Kết thúc chuỗi
        response += buffer;
    }

    // Tách chuỗi thành các dòng
    std::vector<std::string> lines = splitLines(response);

    // Kiểm tra xem có tin nhắn nào không
    if (lines.empty() || response.find("+CMGR:") == std::string::npos) {
        return false; // Không có tin nhắn
    }

    // Tách từng dòng thành mảng các giá trị và trích xuất thông tin
    std::vector<std::vector<std::string>> allValues;
    for (const auto& line : lines) {
        allValues.push_back(splitValues(line));
    }

    // Trích xuất thông tin "mobile", "date", "sms"
    if (allValues.size() > 2) {
        std::string json;
        json = "{\n\"mobile\": \"" + removeSpecialCharacters(allValues[1][1]) + "\",\n\"date\": \"" + removeSpecialCharacters(allValues[1][3]) + " " + removeSpecialCharacters(allValues[1][4]) + "\",\n\"sender\": \"" + removeSpecialCharacters(allValues[2][0]) + "\",\n\"receiver\": \"0833613720\"\n}";
        
        std::cout << json << std::endl;
        
        // Gọi hàm gửi JSON tới API
        postToAPI(json);

        return true; // Có tin nhắn mới
    }

    return false; // Không có tin nhắn
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

        bool newMessage = readResponse(uart);  // Kiểm tra có tin nhắn mới không

        if (newMessage) {
            // Tin nhắn đã được hiển thị, không làm gì thêm
        } else {
            // Không có tin nhắn mới, không in ra gì
            usleep(1000000);  // Chờ 1 giây trước khi kiểm tra lại
        }

        // Xóa toàn bộ tin nhắn trong bộ nhớ (gửi lệnh luôn sau khi kiểm tra)
        sendATCommand(uart, "AT+CMGD=1,4", 500000); // Xóa tất cả tin nhắn

        // Chờ 10 giây trước khi lặp lại
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    // Đóng cổng UART
    close(uart);
    return 0;
}


