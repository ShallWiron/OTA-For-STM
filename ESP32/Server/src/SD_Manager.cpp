#include "SD_Manager.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "Config.h"

bool initSDCard() {
    if (!SD.begin(SD_CS)) {
        Serial.println("Lỗi: Không tìm thấy thẻ SD. Kêu cậu kiểm tra phần cứng rồi mà?");
        return false;
    }

    if (SD.cardType() == CARD_NONE) {
        Serial.println("Lỗi: Không nhận diện được loại thẻ.");
        return false;
    }
    
    Serial.println("Đã khởi tạo thẻ SD thành công.");
    return true;
}