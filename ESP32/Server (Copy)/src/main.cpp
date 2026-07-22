#include <WiFi.h>
#include <WebServer.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

#define SD_CS 5

// Thay bằng mạng của cậu. Đừng có để nguyên rồi thắc mắc sao không kết nối được.
const char* ssid = "Shall";
const char* password = "0123456789";

File uploadFile;

WebServer server(80);

// Form HTML thô sơ nhất có thể.
const char* htmlForm = 
  "<form method='POST' action='/upload' enctype='multipart/form-data'>"
  "<input type='file' name='update'>"
  "<input type='submit' value='Upload'>"
  "</form>";

void handleRoot() {
  server.send(200, "text/html", htmlForm);
}

// Hàm callback xử lý stream data lúc upload
void handleUpload() {
  HTTPUpload& upload = server.upload();
  
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("\n[START] Bắt đầu nhận file: %s\n", upload.filename.c_str());

    // Xóa file cũ đi nếu nó đã tồn tại, tránh ghi đè rác
    if (SD.exists("/firmware.bin")) {
        SD.remove("/firmware.bin");
    }
    // Tạo file mới trên thẻ SD để ghi dữ liệu upload vào
    uploadFile = SD.open("/firmware.bin", FILE_WRITE);
    if (!uploadFile) {
      Serial.println("LỖI: Không thể tạo file trên thẻ SD! Kiểm tra lại thẻ đi đồ ngốc.");
    }
  } 
  else if (upload.status == UPLOAD_FILE_WRITE) {
    // Ghi dữ liệu upload vào file trên thẻ SD
    uploadFile.write(upload.buf, upload.currentSize);
    // In ra số lượng byte đang được đẩy tới. 
    // Tôi không in nội dung data ra vì file .bin là mã nhị phân, in ra Serial sẽ bị rác.
    //Serial.printf("[WRITE] Đang nhận... %d bytes\n", upload.currentSize);
  } 
  else if (upload.status == UPLOAD_FILE_END) {
    // Nhận xong thì ĐÓNG FILE.
    if (uploadFile) {
      uploadFile.close();
      Serial.printf("[END] Hoàn thành! Đã lưu gọn vào SD. Tổng kích thước: %d bytes\n", upload.totalSize);
    }
  }
  else if (upload.status == UPLOAD_FILE_ABORTED) {
    // Nếu rớt mạng giữa chừng, thì đóng file lại và xóa luôn cục rác dở dang đi
    if (uploadFile) {
      uploadFile.close();
      SD.remove("/firmware.bin");
      Serial.println("LỖI: Mạng nghẽn, đã hủy tải lên và dọn rác.");
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  // Khởi tạo SD Card
  if (!SD.begin(SD_CS)) {
    Serial.println("Lỗi: Không tìm thấy thẻ SD. Yêu cầu kiểm tra kết nối phần cứng.");
    return;
  }

  Serial.println("Đã khởi tạo thẻ SD thành công.");

  // Xác thực loại thẻ nhớ
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("Lỗi: Không nhận diện được loại thẻ.");
    return;
  }
  //--------------------------------------------------

  //Kết nối WiFi
  WiFi.begin(ssid, password);
  Serial.print("Đang kết nối WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nĐã kết nối! Truy cập IP này trên trình duyệt laptop:");
  Serial.println(WiFi.localIP());
  //-----------------------------------------------

  // Routing
  server.on("/", HTTP_GET, handleRoot);
  
  // Xử lý route /upload. Tham số thứ 3 là hàm phản hồi khi xong, tham số thứ 4 là hàm bắt luồng data.
  server.on("/upload", HTTP_POST, []() {
    server.send(200, "text/plain", "Da nhan file. Check Serial Monitor.");
  }, handleUpload);

  server.begin();
}

void loop() {
  server.handleClient();
}