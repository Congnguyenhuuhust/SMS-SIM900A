const express = require('express');
const basicAuth = require('express-basic-auth');
const fs = require('fs');
const app = express();
const port = 5555;


// Thiết lập Basic Authentication
app.use(basicAuth({
    users: { 'congnh': '66668888' },  // Thay đổi tên người dùng và mật khẩu
    challenge: true,
    unauthorizedResponse: 'Unauthorized'
}));

// Endpoint mặc định
app.get('/', (req, res) => {
    res.send(`
        <html>
            <head>
                <title>OTP Log</title>
                <script>
                    function fetchOTPs() {
                        fetch('/get-otps')
                            .then(response => response.json())
                            .then(data => {
                                const otpList = document.getElementById('otp-list');
                                otpList.innerHTML = ''; // Xóa danh sách cũ
                                data.forEach(otp => {
                                    const li = document.createElement('li');
                                    li.textContent = 'sms: ' + otp;
                                    otpList.appendChild(li);
                                });
                            })
                            .catch(error => console.error('Error fetching OTPs:', error));
                    }

                    setInterval(fetchOTPs, 2000); // Cập nhật mỗi 2 giây
                </script>
            </head>
            <body>
                <h1>Received OTPs</h1>
                <ul id="otp-list"></ul>
            </body>
        </html>
    `);
});

// Đường dẫn để nhận OTP từ Orange Pi
app.get('/otpwebhook', (req, res) => {
    const otp = req.query.otp;  // Lấy giá trị OTP từ query parameter
    const apiKey = req.query.api_key;  // Lấy api_key từ query parameter

    // Kiểm tra nếu api_key không khớp thì từ chối yêu cầu
    if (apiKey !== 'secret_key_123') {
        res.status(403).send('Forbidden: Invalid API key.');
        return;
    }

    if (otp) {
        const logEntry = `Received OTP: ${otp}`;
        console.log(logEntry);

        // Ghi OTP vào file recsms.log
        fs.appendFile('recsms.log', logEntry + '\n', (err) => {
            if (err) {
                console.error('Error writing to file:', err);
                res.status(500).send('Error processing OTP.');
            } else {
                res.status(200).send('OTP received and logged.');
            }
        });
    } else {
        res.status(400).send('No OTP provided.');
    }
});
// Endpoint lay danh sach otp
app.get('/get-otps', (req, res) => {
    fs.readFile('recsms.log', 'utf8', (err, data) => {
        if (err) {
            console.error('Error reading file:', err);
            res.status(500).send('Error loading OTP data.');
            return;
        }

        const entries = data.split('\n').filter(line => line.trim() !== ''); // Lọc bỏ các dòng trống
        res.json(entries); // Trả về dữ liệu dưới dạng JSON
    });
});


// Khởi động server
app.listen(port, () => {
    console.log(`Server is running on http://localhost:${port}`);
});


