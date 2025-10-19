#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>   
#include <unistd.h>  
#include <sys/ioctl.h> 
#include <string.h>
#include <time.h>    
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdint.h> 
#include "otp.h"  
#include "sha1.h" 

// Định nghĩa IOCTL, FIFO và Device (Giữ nguyên)
#define DELAY_SECONDS 30
#define FIFO_PATH "/tmp/my_data_fifo"
#define DEVICE_FILE "/dev/rtc_time" 
#define LED_IOC_MAGIC 'k' 
#define GET_TIME_CMD _IOR(LED_IOC_MAGIC, 1, int) 


uint8_t secret_key[] = "12345678901234567890";



// Hàm xử lý đọc dữ liệu
void read_time_from_kernel(int device_fd) {
    int unix_time = 0; // Kernel side sẽ trả về tổng giây
    int otpcode = 0; 
    int fifo_fd; // Biến fd cho FIFO, KHÔNG phải device
    int ret; 

    // 1. GỌI IOCTL: Lệnh ĐỌC (READ - GET_TIME_CMD)
    // SỬA: Sử dụng device_fd cho ioctl
    if (ioctl(device_fd, GET_TIME_CMD, &unix_time) < 0) {
        perror("IOCTL GET_TIME_CMD failed");
        printf("Loi: Khong the doc du lieu tu kernel.\n");
        return; // Thoát nếu IOCTL lỗi
    } 
    
    // 2. Tinh toan OTP (Phải chia cho 30s)
    // SỬA: Sử dụng thời gian nhận được từ kernel
    otpcode = totp(secret_key, sizeof(secret_key) - 1, (int)(unix_time / 30), 6);

    // 3. Mo FIFO va Gui du lieu
    // Mở FIFO chỉ để ghi (nó sẽ block nếu không có reader)
    fifo_fd = open(FIFO_PATH, O_WRONLY | O_NONBLOCK); // Mở NONBLOCKING để không bị treo
    if (fifo_fd == -1) {
        perror("Error opening FIFO for writing (Is receiver running?)");
        return;
    }
    
    // Ghi mã OTP (số nguyên) vào FIFO
    ret = write(fifo_fd, &otpcode, sizeof(otpcode));
    close(fifo_fd); // Đóng FIFO ngay sau khi ghi

    if (ret == sizeof(otpcode)) {
        printf("C Sender: Gui OTP THANH CONG.\n");
        printf("[SUCCESS] Tong Giay: %d, OTP code: %d\n", unix_time, otpcode);
    } else {
        perror("Error during write to FIFO");
    }
}


int main() {
    int device_fd;
    
    // 1. Tạo Named Pipe (FIFO)
    if (mkfifo(FIFO_PATH, 0666) == -1 && errno != EEXIST) {
        perror("Error creating FIFO");
        return 1;
    }

    // 2. Mở thiết bị Kernel (Misc Device)
    device_fd = open(DEVICE_FILE, O_RDWR);
    if (device_fd < 0) {
        perror("Failed to open device " DEVICE_FILE);
        fprintf(stderr, "Vui long dam bao module kernel da duoc nap.\n");
        return 1;
    }
    
    printf("Userspace TOTP Logger started. Logging every %d seconds.\n", DELAY_SECONDS);

    // 3. Vòng lặp chính
    while (1)
    {
        printf("---------------------------------------------------\n");
        read_time_from_kernel(device_fd); // Truyen fd cua device
        
        // Ngủ (sleep) 30 giây để đồng bộ với bước nhảy của TOTP
        sleep(DELAY_SECONDS);
    }
    

    close(device_fd);
    return 0;
}