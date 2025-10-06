#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>      // open
#include <unistd.h>     // close
#include <sys/ioctl.h>  // ioctl
#include <string.h>     // memset
#include <errno.h>      // errno
#include <linux/ioctl.h> // Chỉ cần trong kernel, nhưng nên đặt ở đây cho đầy đủ

#define LED_IOC_MAGIC 'k' 
#define LED_ON  _IOW(LED_IOC_MAGIC, 1, int) 
#define LED_OFF _IOW(LED_IOC_MAGIC, 2, int)
#define LED_GET_STATUS _IOR(LED_IOC_MAGIC, 3, int) 

#define DEVICE_FILE "/dev/misc_example"

void print_menu() {
    printf("\n=====================================\n");
    printf("        LED Driver Control Menu      \n");
    printf("=====================================\n");
    printf("1. Bật LED (LED_ON)\n");
    printf("2. Tắt LED (LED_OFF)\n");
    printf("3. Đọc trạng thái LED (LED_GET_STATUS)\n");
    printf("4. Thoát\n");
    printf("-------------------------------------\n");
    printf("Nhập lựa chọn của bạn (1-4): ");
}

int main() 
{
    int fd;
    int choice;
    int status;
    int dummy_data = 1; // Dữ liệu giả (dummy data) để gửi kèm lệnh _IOW

    // 1. Mở device file
    fd = open(DEVICE_FILE, O_RDWR);
    if (fd < 0) {
        // Kiểm tra lỗi nếu driver chưa được nạp hoặc tên device không đúng
        fprintf(stderr, "[-] Failed to open device file %s. Error: %s\n", 
                DEVICE_FILE, strerror(errno));
        fprintf(stderr, "    => Hãy chắc chắn driver đã được nạp (sudo insmod) và device name là 'misc_example'.\n");
        return 1;
    }

    printf("[+] Successfully opened device file %s.\n", DEVICE_FILE);

    while (1) {
        print_menu();
        if (scanf("%d", &choice) != 1) {
            // Xử lý đầu vào không phải số
            printf("Lựa chọn không hợp lệ. Vui lòng nhập số.\n");
            while (getchar() != '\n'); // Xóa bộ đệm đầu vào
            continue;
        }

        switch (choice) {
            case 1: // BẬT LED
                printf("[*] Sending LED_ON command...\n");
                // Gửi lệnh LED_ON. Tham số thứ 3 là con trỏ dữ liệu (int)
                if (ioctl(fd, LED_ON, &dummy_data) < 0) {
                    perror("[-] IOCTL LED_ON failed");
                } else {
                    printf("[+] LED BẬT thành công.\n");
                }
                break;

            case 2: // TẮT LED
                printf("[*] Sending LED_OFF command...\n");
                // Gửi lệnh LED_OFF. Tham số thứ 3 có thể là NULL hoặc con trỏ dữ liệu
                if (ioctl(fd, LED_OFF, NULL) < 0) {
                    perror("[-] IOCTL LED_OFF failed");
                } else {
                    printf("[+] LED TẮT thành công.\n");
                }
                break;

            case 3: // ĐỌC TRẠNG THÁI
                printf("[*] Sending LED_GET_STATUS command...\n");
                // Gửi lệnh LED_GET_STATUS. Tham số thứ 3 là con trỏ để nhận giá trị
                if (ioctl(fd, LED_GET_STATUS, &status) < 0) {
                    perror("[-] IOCTL LED_GET_STATUS failed");
                } else {
                    // Trạng thái 1: Bật, 0: Tắt (theo logic driver)
                    printf("[+] Trạng thái LED từ kernel: %s (Value: %d)\n", 
                           status == 1 ? "BẬT" : "TẮT", status);
                }
                break;

            case 4: // THOÁT
                printf("[*] Exiting application and closing device file.\n");
                close(fd);
                return 0;

            default:
                printf("Lựa chọn không hợp lệ. Vui lòng chọn từ 1 đến 4.\n");
                break;
        }
    }

    close(fd);
    return 0;
}
