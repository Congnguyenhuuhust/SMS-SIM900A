const express = require('express');
const basicAuth = require('express-basic-auth');
const fs = require('fs');
const path = require('path');

const app = express();
const port = 5555;
const logFile = path.join(__dirname, 'RPsms.log');

// Thiết lập Basic Authentication
app.use(basicAuth({
    users: { 'congnh': '66668888' },  // Đặt tên người dùng và mật khẩu
    challenge: true,
    unauthorizedResponse: 'Unauthorized'
}));

// Middleware để xử lý dữ liệu JSON
app.use(express.json());

// Đường dẫn để nhận dữ liệu từ Orange Pi qua POST
app.post('/sms', (req, res) => {
    const sender = req.body.sender;   // Số điện thoại người gửi
    const sms = req.body.sms;         // Nội dung tin nhắn

    const logEntry = `From: ${sender} Message: ${sms}`;
    console.log(logEntry);

    // Ghi dữ liệu vào file RPsms.log
    fs.appendFile(logFile, logEntry + '\n', (err) => {
        if (err) {
            console.error('Error writing to file:', err);
            res.status(500).send('Error processing SMS.');
        } else {
            res.status(200).send('SMS received and logged.');
        }
    });
});

// Đường dẫn để hiển thị nội dung nhận được từ RPsms.log dưới dạng HTML
app.get('/', (req, res) => {
    const htmlContent = `
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta http-equiv="X-UA-Compatible" content="IE=edge">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Received SMS</title>
        <style>
            body { font-family: Arial, sans-serif; margin: 20px; }
            h2 { color: #333; }
            #sms-log { white-space: pre-wrap; font-family: monospace; background: #f9f9f9; padding: 15px; border: 1px solid #ddd; }
        </style>
    </head>
    <body>
        <h2>Received SMS Log</h2>
        <div id="sms-log">Loading...</div>

        <script>
            // Hàm để tải và hiển thị nội dung từ file RPsms.log mỗi 2 giây
            function fetchLog() {
                fetch('/sms-log')
                    .then(response => response.text())
                    .then(data => {
                        document.getElementById('sms-log').textContent = data;
                    })
                    .catch(error => console.error('Error fetching log:', error));
            }

            // Tải log ban đầu và thiết lập để cập nhật mỗi 2 giây
            fetchLog();
            setInterval(fetchLog, 2000);
        </script>
    </body>
    </html>`;
    
    res.send(htmlContent);
});

// Endpoint để đọc và trả về nội dung của file RPsms.log
app.get('/sms-log', (req, res) => {
    fs.readFile(logFile, 'utf8', (err, data) => {
        if (err) {
            console.error('Error reading log file:', err);
            res.status(500).send('Error reading log file.');
        } else {
            res.send(data);
        }
    });
});

// Khởi động server
app.listen(port, () => {
    console.log(`Server is running on http://localhost:${port}`);
});

