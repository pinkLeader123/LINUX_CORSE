from flask import Flask, request, jsonify
import ssl
import time

app = Flask(__name__)

# Cấu hình đường dẫn đến file chứng chỉ và khóa
CERT_FILE = "server.crt" 
KEY_FILE = "server.key" 

# Endpoint đơn giản nhận số và trả về số đó + 1
@app.route('/api/v1/echo_number', methods=['POST'])
def echo_number_endpoint():
    data = request.get_json()
    
    # 1. Kiểm tra dữ liệu input
    if not data or 'number' not in data:
        return jsonify({"status": "error", "message": "Thiếu trường 'number' trong JSON."}), 400

    try:
        input_number = int(data['number'])
        
        # 2. Xử lý logic: trả về số + 1
        output_number = input_number + 1
        
        print(f"[{time.strftime('%H:%M:%S')}] Nhận số: {input_number}. Trả về: {output_number}")
        
        # 3. Trả về kết quả
        response = {
            "status": "success", 
            "received": input_number,
            "processed": output_number,
            "message": "Đã nhận số và trả về kết quả thành công qua HTTPS."
        }
        return jsonify(response), 200
        
    except ValueError:
        return jsonify({"status": "error", "message": "Giá trị 'number' phải là số nguyên."}), 400

# Chạy Server với SSL/TLS
if __name__ == '__main__':
    try:
        # Tải chứng chỉ và khóa để tạo ngữ cảnh SSL
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(CERT_FILE, KEY_FILE)
        
        print("Bắt đầu Flask Server (HTTPS) trên cổng 5000. Đang chờ kết nối TLS...")
        # Chạy Flask trên HTTPS, port 5000, lắng nghe tất cả IP
        app.run(host='0.0.0.0', port=5000, ssl_context=context, debug=False)
        
    except FileNotFoundError:
        print("LỖI: Không tìm thấy file server.crt hoặc server.key. Vui lòng chạy lệnh openssl để tạo chúng.")
    except Exception as e:
        print(f"LỖI KHỞI ĐỘNG SERVER: {e}")

# huowng dan chay
#BBB_IP="192.168.0.101"
#curl -k -X POST https://${BBB_IP}:5000/api/v1/echo_number \
#     -H "Content-Type: application/json" \
#     -d "{\"number\": 123}"
#
#
#