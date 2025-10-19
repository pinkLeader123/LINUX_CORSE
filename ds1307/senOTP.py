import smtplib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
import os
import struct
import time
from flask import Flask, request, jsonify
import threading
import ssl  # Thêm import ssl cho HTTPS

# --- CẤU HÌNH CỐ ĐỊNH ---
FIFO_PATH = "/tmp/my_data_fifo"
DATA_SIZE = 4  # Kích thước int (4 bytes)
CERT_FILE = "server.crt"
KEY_FILE = "server.key"

# --- CẤU HÌNH EMAIL (THAY BẰNG GIÁ TRỊ THỰC TẾ) ---
SENDER_EMAIL = "tacaphuc6@gmail.com"  # Email gửi
SENDER_PASSWORD = "jpec qvmc rnfv atur "  # App Password của Gmail (không dùng mật khẩu thường)
RECEIVER_EMAIL = "dangphuc20802@gmail.com"  # Email nhận OTP

# --- BIẾN TOÀN CỤC CHỨA OTP MỚI NHẤT ---
LAST_VALID_OTP = -1  # Khởi tạo không hợp lệ

app = Flask(__name__)

# --- HÀM GỬI EMAIL ---
def send_email_with_int(sender_email, sender_password, receiver_email, int_value):
    """Gửi số nguyên (OTP) qua email."""
    message = MIMEMultipart()
    message['From'] = sender_email
    message['To'] = receiver_email
    message['Subject'] = f'OTP Mới Từ BBB: {int_value}'
    body = f'Mã OTP mới được tạo bởi Kernel Driver là: {int_value}\n\nThời gian: {time.strftime("%Y-%m-%d %H:%M:%S")}'
    message.attach(MIMEText(body, 'plain'))
    try:
        server = smtplib.SMTP('smtp.gmail.com', 587, timeout=10)
        server.starttls()
        server.login(sender_email, sender_password)
        text = message.as_string()
        server.sendmail(sender_email, receiver_email, text)
        server.quit()
        print(f"[{time.strftime('%H:%M:%S')}] Email gửi THÀNH CÔNG! OTP: {int_value}")
        return True
    except smtplib.SMTPAuthenticationError:
        print("\n[LỖI] Lỗi xác thực SMTP: Kiểm tra App Password hoặc quyền truy cập 'Less secure apps'.")
        return False
    except Exception as e:
        print(f"\n[LỖI] Lỗi khi gửi email: {e}")
        return False

# --- ENDPOINT XÁC THỰC OTP TỪ CLIENT ---
@app.route('/api/v1/echo_number', methods=['POST'])
def echo_number_endpoint():
    global LAST_VALID_OTP
    data = request.get_json()
    # 1. Kiểm tra dữ liệu input
    if not data or 'number' not in data:
        return jsonify({"status": "error", "message": "Thiếu trường 'number' trong JSON."}), 400
    try:
        input_number = int(data['number'])
        # 2. SO SÁNH OTP: So sánh số từ Client với OTP cuối cùng từ FIFO
        if LAST_VALID_OTP == -1:
            # Nếu chưa có OTP nào được đọc, từ chối
            return jsonify({"status": "error", "message": "Chưa có OTP hợp lệ nào được tạo."}), 403
        if input_number == LAST_VALID_OTP:
            # XÁC THỰC THÀNH CÔNG
            print(f"[{time.strftime('%H:%M:%S')}] XÁC THỰC THÀNH CÔNG: {input_number} (Server OTP: {LAST_VALID_OTP})")
            # TÙY CHỌN: Xóa OTP sau khi dùng để tránh dùng lại (uncomment nếu cần)
            # LAST_VALID_OTP = -2
            response = {
                "status": "success",
                "received": input_number,
                "processed": True,
                "message": "OTP ĐÚNG. Xác thực thành công."
            }
            return jsonify(response), 200
        else:
            print(f"[{time.strftime('%H:%M:%S')}] XÁC THỰC THẤT BẠI. Client: {input_number}, Server: {LAST_VALID_OTP}")
            return jsonify({"status": "error", "message": "OTP KHÔNG ĐÚNG."}), 401
    except ValueError:
        return jsonify({"status": "error", "message": "Giá trị 'number' phải là số nguyên."}), 400

# --- LOGIC ĐỌC FIFO VÀ GỬI EMAIL (CHẠY TRONG THREAD RIÊNG) ---
def fifo_reader_and_mailer():
    global LAST_VALID_OTP
    # Chờ FIFO tồn tại
    while not os.path.exists(FIFO_PATH):
        print(f"[{time.strftime('%H:%M:%S')}] Đang chờ file FIFO {FIFO_PATH}...")
        time.sleep(1)
    print(f"[{time.strftime('%H:%M:%S')}] File FIFO đã sẵn sàng. Bắt đầu đọc OTP và gửi email.")
    while True:
        try:
            with open(FIFO_PATH, 'rb') as fifo:
                print(f"[{time.strftime('%H:%M:%S')}] Đang chờ OTP mới từ kernel...")
                binary_data = fifo.read(DATA_SIZE)
                if len(binary_data) == DATA_SIZE:
                    received_int = struct.unpack('<i', binary_data)[0]  # Little-endian int
                    if received_int > 0:  # Giả sử OTP > 0 là hợp lệ
                        print(f"[{time.strftime('%H:%M:%S')}] Nhận OTP từ FIFO: {received_int}")
                        # 1. CẬP NHẬT BIẾN TOÀN CỤC CHO FLASK SO SÁNH
                        LAST_VALID_OTP = received_int
                        # 2. GỬI EMAIL
                        send_email_with_int(SENDER_EMAIL, SENDER_PASSWORD, RECEIVER_EMAIL, received_int)
                    else:
                        print(f"[{time.strftime('%H:%M:%S')}] OTP không hợp lệ (≤0): {received_int}")
                else:
                    # FIFO rỗng, chờ tiếp
                    time.sleep(0.1)
        except Exception as e:
            print(f"[{time.strftime('%H:%M:%S')}] [LỖI] Lỗi đọc FIFO: {e}. Thử lại sau 5s.")
            time.sleep(5)

# --- PHẦN CHÍNH (MAIN) ---
if __name__ == "__main__":
    # 1. KHỞI TẠO THREAD ĐỌC FIFO (daemon để tự động dừng khi main kết thúc)
    print("=== User App: Bắt đầu Luồng đọc OTP từ FIFO và gửi Email ===")
    fifo_thread = threading.Thread(target=fifo_reader_and_mailer, daemon=True)
    fifo_thread.start()

    # 2. KIỂM TRA CERT/KEY CHO HTTPS
    if not (os.path.exists(CERT_FILE) and os.path.exists(KEY_FILE)):
        print(f"[LỖI] Không tìm thấy {CERT_FILE} hoặc {KEY_FILE}. Chạy HTTP thay vì HTTPS.")
        app.run(host='0.0.0.0', port=5000, debug=False)
    else:
        try:
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            context.load_cert_chain(CERT_FILE, KEY_FILE)
            print("Bắt đầu Flask Server (HTTPS) trên https://0.0.0.0:5000.")
            app.run(host='0.0.0.0', port=5000, ssl_context=context, debug=False)
        except Exception as e:
            print(f"[LỖI] Khởi động HTTPS thất bại: {e}. Chạy HTTP fallback.")
            app.run(host='0.0.0.0', port=5000, debug=False)