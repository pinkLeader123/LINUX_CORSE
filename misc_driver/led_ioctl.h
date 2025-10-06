#ifndef __LED_IOCTL_H
#define __LED_IOCTL_H

#include <linux/ioctl.h> // Chỉ cần trong kernel, nhưng nên đặt ở đây cho đầy đủ

// --- Thông số cho IOCTL ---
// 'k': Ký tự độc nhất cho nhóm lệnh (8-bit)
#define LED_IOC_MAGIC 'k' 

// --- Định nghĩa các lệnh IOCTL ---
// 1. Lệnh BẬT LED: Ghi (Write) một giá trị (int) vào kernel
#define LED_ON  _IOW(LED_IOC_MAGIC, 1, int) 

// 2. Lệnh TẮT LED: Ghi (Write) một giá trị (int) vào kernel
#define LED_OFF _IOW(LED_IOC_MAGIC, 2, int)

// 3. Lệnh ĐỌC trạng thái LED: Đọc (Read) một giá trị (int) từ kernel
#define LED_GET_STATUS _IOR(LED_IOC_MAGIC, 3, int) 

#endif /* __LED_IOCTL_H */