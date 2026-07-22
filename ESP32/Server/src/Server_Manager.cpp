#include "Server_Manager.h"
#include "Config.h"
#include <WiFi.h>
#include <WebServer.h>
#include "FS.h"
#include "SD.h"

// Hai đối tượng này giờ chỉ tồn tại và được xử lý nội bộ trong file này
WebServer server(80);
File uploadFile;

const char* htmlForm = 
  "<form method='POST' action='/upload' enctype='multipart/form-data'>"
  "<input type='file' name='update'>"
  "<input type='submit' value='Upload'>"
  "</form>";

void handleRoot() {
    server.send(200, "text/html", htmlForm);
}

void handleUpload() {
    HTTPUpload& upload = server.upload();
    
    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("\n[START] Bắt đầu nhận file: %s\n", upload.filename.c_str());

        if (SD.exists("/firmware.bin")) {
            SD.remove("/firmware.bin");
        }
        
        uploadFile = SD.open("/firmware.bin", FILE_WRITE);
        if (!uploadFile) {
            Serial.println("LỖI: Không thể tạo file trên thẻ SD! Kiểm tra lại thẻ đi đồ ngốc.");
        }
    } 
    else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) {
            uploadFile.write(upload.buf, upload.currentSize);
        }
    } 
    else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) {
            uploadFile.close();
            Serial.printf("[END] Hoàn thành! Đã lưu gọn vào SD. Tổng kích thước: %d bytes\n", upload.totalSize);
        }
    }
    else if (upload.status == UPLOAD_FILE_ABORTED) {
        if (uploadFile) {
            uploadFile.close();
            SD.remove("/firmware.bin");
            Serial.println("LỖI: Mạng nghẽn, đã hủy tải lên và dọn rác.");
        }
    }
}

void initNetworkAndServer() {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Đang kết nối WiFi");
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("\nĐã kết nối! Truy cập IP này trên trình duyệt laptop:");
    Serial.println(WiFi.localIP());

    server.on("/", HTTP_GET, handleRoot);
    
    server.on("/upload", HTTP_POST, []() {
        server.send(200, "text/plain", "Da nhan file. Check Serial Monitor.");
    }, handleUpload);

    server.begin();
}

void handleServerClient() {
    server.handleClient();
}