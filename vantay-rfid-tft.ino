#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <MFRC522.h>
#include <SPI.h>
#include <EEPROM.h>
#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>

// Định nghĩa các chân
#define TOUCH_CS 4   // Chip Select cho cảm ứng
#define RFID_CS 5    // Chip Select cho RFID
#define RFID_RST 32  // Reset cho RFID
#define VSPI_SCK 18  // VSPI SCK
#define VSPI_MISO 19 // VSPI MISO
#define VSPI_MOSI 23 // VSPI MOSI

// Cấu hình EEPROM
#define EEPROM_SIZE 512
#define MAX_FINGERPRINTS 12
#define MAX_RFID_CARDS 12
#define RFID_UID_SIZE 4 // Kích thước UID (4 byte cho MIFARE Classic)

// Khởi tạo UART cho AS608
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger(&mySerial);

// Khởi tạo đối tượng
TFT_eSPI tft = TFT_eSPI(); // HSPI cho TFT
XPT2046_Touchscreen ts(TOUCH_CS); // VSPI cho touch
MFRC522 rfid(RFID_CS, RFID_RST); // VSPI cho RFID

// Trạng thái màn hình
enum Screen { MAIN, AUTH, SETTINGS, PASS_INPUT, CHANGE_PASS_OLD, CHANGE_PASS_NEW, SCAN_FINGERPRINT, ADD_FINGER_AUTH, ADD_FINGERPRINT, DELETE_FINGERPRINT, FORGOT_PASS_RFID, CONFIRM_DELETE_FINGERPRINT, SCAN_RFID, ADD_RFID_AUTH, ADD_RFID, DELETE_RFID, CONFIRM_DELETE_RFID };
Screen currentScreen = MAIN;

// Trạng thái SPI
enum Mode { TOUCH_MODE, RFID_MODE };
Mode currentMode = TOUCH_MODE;
unsigned long lastRFIDTime = 0;

// Định nghĩa vùng nút Back
#define BACK_BUTTON_X 10
#define BACK_BUTTON_Y 10
#define BACK_BUTTON_W 40
#define BACK_BUTTON_H 30

// Định nghĩa vùng nút cho giao diện chính
#define AUTH_BUTTON_X 20
#define AUTH_BUTTON_Y 80
#define AUTH_BUTTON_W 135
#define AUTH_BUTTON_H 80
#define SETTING_BUTTON_X 165
#define SETTING_BUTTON_Y 80
#define SETTING_BUTTON_W 135
#define SETTING_BUTTON_H 80

// Định nghĩa vùng nút cho giao diện Xác thực
#define PASS_BUTTON_X 20
#define PASS_BUTTON_Y 50
#define PASS_BUTTON_W 280
#define PASS_BUTTON_H 50
#define RFID_BUTTON_X 20
#define RFID_BUTTON_Y 110
#define RFID_BUTTON_W 280
#define RFID_BUTTON_H 50
#define FINGER_BUTTON_X 20
#define FINGER_BUTTON_Y 170
#define FINGER_BUTTON_W 280
#define FINGER_BUTTON_H 50

// Định nghĩa vùng nút cho giao diện Cài đặt
#define CHANGE_PASS_X 20
#define CHANGE_PASS_Y 50
#define CHANGE_PASS_W 140
#define CHANGE_PASS_H 50
#define FORGOT_PASS_X 160
#define FORGOT_PASS_Y 50
#define FORGOT_PASS_W 140
#define FORGOT_PASS_H 50
#define ADD_FINGER_X 20
#define ADD_FINGER_Y 110
#define ADD_FINGER_W 140
#define ADD_FINGER_H 50
#define DEL_FINGER_X 160
#define DEL_FINGER_Y 110
#define DEL_FINGER_W 140
#define DEL_FINGER_H 50
#define ADD_RFID_X 20
#define ADD_RFID_Y 170
#define ADD_RFID_W 140
#define ADD_RFID_H 50
#define DEL_RFID_X 160
#define DEL_RFID_Y 170
#define DEL_RFID_W 140
#define DEL_RFID_H 50

// Định nghĩa vùng nút bàn phím
#define KEY_W 100
#define KEY_H 40
#define KEY_X1 10
#define KEY_X2 110
#define KEY_X3 210
#define KEY_Y1 60
#define KEY_Y2 100
#define KEY_Y3 140
#define KEY_Y4 180

// Định nghĩa vùng nút cho danh sách ID vân tay và RFID
#define ID_W 70
#define ID_H 40
#define ID_X_START 10
#define ID_Y_START (BACK_BUTTON_Y + BACK_BUTTON_H + 30)
#define ID_SPACING 10

// Định nghĩa vùng nút xác nhận xóa
#define CONFIRM_OK_X 20
#define CONFIRM_OK_Y 170
#define CONFIRM_OK_W 135
#define CONFIRM_OK_H 50
#define CONFIRM_CANCEL_X 165
#define CONFIRM_CANCEL_Y 170
#define CONFIRM_CANCEL_W 135
#define CONFIRM_CANCEL_H 50

// Biến lưu mật khẩu
String password = "";
String storedPassword = "1234";
String newPassword = "";
String deleteInput = "";

// Biến lưu vân tay
byte storedFingerprints[MAX_FINGERPRINTS];
int fingerprintCount = 0;
int selectedFingerprintID = 0;

// Biến lưu RFID
byte storedRFIDs[MAX_RFID_CARDS][RFID_UID_SIZE];
byte storedRFIDIDs[MAX_RFID_CARDS];
int rfidCount = 0;
byte selectedRFID[RFID_UID_SIZE];
int selectedRFIDID = 0;

// Hàm chuyển sang chế độ Touch
void switchToTouch() {
  digitalWrite(RFID_CS, HIGH);
  SPI.end();
  delay(10);
  SPI.begin(VSPI_SCK, VSPI_MISO, VSPI_MOSI, TOUCH_CS);
  ts.begin();
  ts.setRotation(2);
  currentMode = TOUCH_MODE;
  Serial.println("Switched to Touch mode");
}

// Hàm chuyển sang chế độ RFID
void switchToRFID() {
  digitalWrite(TOUCH_CS, HIGH);
  SPI.end();
  delay(10);
  SPI.begin(VSPI_SCK, VSPI_MISO, VSPI_MOSI, RFID_CS);
  delay(10);
  rfid.PCD_Init();
  currentMode = RFID_MODE;
  Serial.println("Switched to RFID mode");
}

// Hàm vẽ giao diện chính
void drawMainScreen() {
  digitalWrite(TOUCH_CS, HIGH);
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 320, 240, TFT_BLUE);
  tft.fillRect(AUTH_BUTTON_X, AUTH_BUTTON_Y, AUTH_BUTTON_W, AUTH_BUTTON_H, TFT_NAVY);
  tft.drawRect(AUTH_BUTTON_X, AUTH_BUTTON_Y, AUTH_BUTTON_W, AUTH_BUTTON_H, TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(AUTH_BUTTON_X + 10, AUTH_BUTTON_Y + 30);
  tft.println("Xac thuc");
  tft.fillRect(SETTING_BUTTON_X, SETTING_BUTTON_Y, SETTING_BUTTON_W, SETTING_BUTTON_H, TFT_NAVY);
  tft.drawRect(SETTING_BUTTON_X, SETTING_BUTTON_Y, SETTING_BUTTON_W, SETTING_BUTTON_H, TFT_WHITE);
  tft.setCursor(SETTING_BUTTON_X + 10, SETTING_BUTTON_Y + 30);
  tft.println("Cai dat");
  Serial.println("Main Screen drawn");
}

// Hàm vẽ giao diện xác thực
void drawAuthScreen() {
  digitalWrite(TOUCH_CS, HIGH);
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 320, 240, TFT_BLUE);
  tft.fillRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_W, BACK_BUTTON_H, TFT_RED);
  tft.drawRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_W, BACK_BUTTON_H, TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(BACK_BUTTON_X + 10, BACK_BUTTON_Y + 5);
  tft.println("<");
  tft.fillRect(PASS_BUTTON_X, PASS_BUTTON_Y, PASS_BUTTON_W, PASS_BUTTON_H, TFT_NAVY);
  tft.drawRect(PASS_BUTTON_X, PASS_BUTTON_Y, PASS_BUTTON_W, PASS_BUTTON_H, TFT_WHITE);
  tft.setCursor(PASS_BUTTON_X + 10, PASS_BUTTON_Y + 15);
  tft.println("Nhap mat khau");
  tft.fillRect(RFID_BUTTON_X, RFID_BUTTON_Y, RFID_BUTTON_W, RFID_BUTTON_H, TFT_NAVY);
  tft.drawRect(RFID_BUTTON_X, RFID_BUTTON_Y, RFID_BUTTON_W, RFID_BUTTON_H, TFT_WHITE);
  tft.setCursor(RFID_BUTTON_X + 10, RFID_BUTTON_Y + 15);
  tft.println("Quet the9272 RFID");
  tft.fillRect(FINGER_BUTTON_X, FINGER_BUTTON_Y, FINGER_BUTTON_W, FINGER_BUTTON_H, TFT_NAVY);
  tft.drawRect(FINGER_BUTTON_X, FINGER_BUTTON_Y, FINGER_BUTTON_W, FINGER_BUTTON_H, TFT_WHITE);
  tft.setCursor(FINGER_BUTTON_X + 10, FINGER_BUTTON_Y + 15);
  tft.println("Quet van tay");
}

// Hàm vẽ giao diện cài đặt
void drawSettingsScreen() {
  digitalWrite(TOUCH_CS, HIGH);
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 320, 240, TFT_BLUE);
  tft.fillRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_W, BACK_BUTTON_H, TFT_RED);
  tft.drawRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_W, BACK_BUTTON_H, TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(BACK_BUTTON_X + 10, BACK_BUTTON_Y + 5);
  tft.println("<");
  tft.fillRect(CHANGE_PASS_X, CHANGE_PASS_Y, CHANGE_PASS_W, CHANGE_PASS_H, TFT_NAVY);
  tft.drawRect(CHANGE_PASS_X, CHANGE_PASS_Y, CHANGE_PASS_W, CHANGE_PASS_H, TFT_WHITE);
  tft.setCursor(CHANGE_PASS_X + 20, CHANGE_PASS_Y + 15);
  tft.println("Doi MK");
  tft.fillRect(FORGOT_PASS_X, FORGOT_PASS_Y, FORGOT_PASS_W, FORGOT_PASS_H, TFT_NAVY);
  tft.drawRect(FORGOT_PASS_X, FORGOT_PASS_Y, FORGOT_PASS_W, FORGOT_PASS_H, TFT_WHITE);
  tft.setCursor(FORGOT_PASS_X + 20, FORGOT_PASS_Y + 15);
  tft.println("Quen MK");
  tft.fillRect(ADD_FINGER_X, ADD_FINGER_Y, ADD_FINGER_W, ADD_FINGER_H, TFT_NAVY);
  tft.drawRect(ADD_FINGER_X, ADD_FINGER_Y, ADD_FINGER_W, ADD_FINGER_H, TFT_WHITE);
  tft.setCursor(ADD_FINGER_X + 10, ADD_FINGER_Y + 15);
  tft.println("Them van tay");
  tft.fillRect(DEL_FINGER_X, DEL_FINGER_Y, DEL_FINGER_W, DEL_FINGER_H, TFT_NAVY);
  tft.drawRect(DEL_FINGER_X, DEL_FINGER_Y, DEL_FINGER_W, DEL_FINGER_H, TFT_WHITE);
  tft.setCursor(DEL_FINGER_X + 10, DEL_FINGER_Y + 15);
  tft.println("Xoa van tay");
  tft.fillRect(ADD_RFID_X, ADD_RFID_Y, ADD_RFID_W, ADD_RFID_H, TFT_NAVY);
  tft.drawRect(ADD_RFID_X, ADD_RFID_Y, ADD_RFID_W, ADD_RFID_H, TFT_WHITE);
  tft.setCursor(ADD_RFID_X + 10, ADD_RFID_Y + 15);
  tft.println("Them RFID");
  tft.fillRect(DEL_RFID_X, DEL_RFID_Y, DEL_RFID_W, DEL_RFID_H, TFT_NAVY);
  tft.drawRect(DEL_RFID_X, DEL_RFID_Y, DEL_RFID_W, DEL_RFID_H, TFT_WHITE);
  tft.setCursor(DEL_RFID_X + 10, DEL_RFID_Y + 15);
  tft.println("Xoa RFID");
}

// Hàm vẽ bàn phím
void drawKeyboardScreen(String title, String currentInput) {
  digitalWrite(TOUCH_CS, HIGH);
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 320, 240, TFT_BLUE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 20);
  tft.println(title);
  tft.fillRect(20, 20, 280, 30, TFT_BLACK);
  tft.drawRect(20, 20, 280, 30, TFT_WHITE);
  tft.setCursor(30, 25);
  tft.println(currentInput);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.fillRect(KEY_X1, KEY_Y1, KEY_W, KEY_H, TFT_NAVY);
  tft.drawRect(KEY_X1, KEY_Y1, KEY_W, KEY_H, TFT_WHITE);
  tft.setCursor(KEY_X1 + 45, KEY_Y1 + 10);
  tft.println("1");
  tft.fillRect(KEY_X2, KEY_Y1, KEY_W, KEY_H, TFT_NAVY);
  tft.drawRect(KEY_X2, KEY_Y1, KEY_W, KEY_H, TFT_WHITE);
  tft.setCursor(KEY_X2 + 45, KEY_Y1 + 10);
  tft.println("2");
  tft.fillRect(KEY_X3, KEY_Y1, KEY_W, KEY_H, TFT_NAVY);
  tft.drawRect(KEY_X3, KEY_Y1, KEY_W, KEY_H, TFT_WHITE);
  tft.setCursor(KEY_X3 + 45, KEY_Y1 + 10);
  tft.println("3");
  tft.fillRect(KEY_X1, KEY_Y2, KEY_W, KEY_H, TFT_NAVY);
  tft.drawRect(KEY_X1, KEY_Y2, KEY_W, KEY_H, TFT_WHITE);
  tft.setCursor(KEY_X1 + 45, KEY_Y2 + 10);
  tft.println("4");
  tft.fillRect(KEY_X2, KEY_Y2, KEY_W, KEY_H, TFT_NAVY);
  tft.drawRect(KEY_X2, KEY_Y2, KEY_W, KEY_H, TFT_WHITE);
  tft.setCursor(KEY_X2 + 45, KEY_Y2 + 10);
  tft.println("5");
  tft.fillRect(KEY_X3, KEY_Y2, KEY_W, KEY_H, TFT_NAVY);
  tft.drawRect(KEY_X3, KEY_Y2, KEY_W, KEY_H, TFT_WHITE);
  tft.setCursor(KEY_X3 + 45, KEY_Y2 + 10);
  tft.println("6");
  tft.fillRect(KEY_X1, KEY_Y3, KEY_W, KEY_H, TFT_NAVY);
  tft.drawRect(KEY_X1, KEY_Y3, KEY_W, KEY_H, TFT_WHITE);
  tft.setCursor(KEY_X1 + 45, KEY_Y3 + 10);
  tft.println("7");
  tft.fillRect(KEY_X2, KEY_Y3, KEY_W, KEY_H, TFT_NAVY);
  tft.drawRect(KEY_X2, KEY_Y3, KEY_W, KEY_H, TFT_WHITE);
  tft.setCursor(KEY_X2 + 45, KEY_Y3 + 10);
  tft.println("8");
  tft.fillRect(KEY_X3, KEY_Y3, KEY_W, KEY_H, TFT_NAVY);
  tft.drawRect(KEY_X3, KEY_Y3, KEY_W, KEY_H, TFT_WHITE);
  tft.setCursor(KEY_X3 + 45, KEY_Y3 + 10);
  tft.println("9");
  tft.fillRect(KEY_X1, KEY_Y4, KEY_W, KEY_H, TFT_NAVY);
  tft.drawRect(KEY_X1, KEY_Y4, KEY_W, KEY_H, TFT_WHITE);
  tft.setCursor(KEY_X1 + 35, KEY_Y4 + 10);
  tft.println("Clr");
  tft.fillRect(KEY_X2, KEY_Y4, KEY_W, KEY_H, TFT_NAVY);
  tft.drawRect(KEY_X2, KEY_Y4, KEY_W, KEY_H, TFT_WHITE);
  tft.setCursor(KEY_X2 + 45, KEY_Y4 + 10);
  tft.println("0");
  tft.fillRect(KEY_X3, KEY_Y4, KEY_W, KEY_H, TFT_NAVY);
  tft.drawRect(KEY_X3, KEY_Y4, KEY_W, KEY_H, TFT_WHITE);
  tft.setCursor(KEY_X3 + 35, KEY_Y4 + 10);
  tft.println("OK");
}

// Hàm vẽ giao diện quét vân tay/RFID
void drawScanScreen(String msg) {
  digitalWrite(TOUCH_CS, HIGH);
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 320, 240, TFT_BLUE);
  tft.fillRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_W, BACK_BUTTON_H, TFT_RED);
  tft.drawRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_W, BACK_BUTTON_H, TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(BACK_BUTTON_X + 10, BACK_BUTTON_Y + 5);
  tft.println("<");
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 100);
  tft.println(msg);
}

// Hàm hiển thị thông báo vân tay/RFID
void showScanMessage(String msg) {
  digitalWrite(TOUCH_CS, HIGH);
  tft.fillRect(10, 100, 280, 50, TFT_BLUE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 100);
  tft.println(msg);
  Serial.println("Message: " + msg);
}

// Hàm vẽ danh sách xóa vân tay/RFID
void drawDeleteListScreen(String title, bool isFingerprint) {
  digitalWrite(TOUCH_CS, HIGH);
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 320, 240, TFT_BLUE);
  tft.fillRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_W, BACK_BUTTON_H, TFT_RED);
  tft.drawRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_W, BACK_BUTTON_H, TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(BACK_BUTTON_X + 10, BACK_BUTTON_Y + 5);
  tft.println("<");
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, BACK_BUTTON_Y + BACK_BUTTON_H + 10);
  tft.println(title);
  if (isFingerprint ? fingerprintCount == 0 : rfidCount == 0) {
    tft.setTextSize(2);
    tft.setCursor(10, ID_Y_START + 20);
    tft.println(isFingerprint ? "Khong co van tay" : "Khong co the RFID");
  } else {
    tft.setTextSize(1);
    int count = isFingerprint ? fingerprintCount : rfidCount;
    for (int i = 0; i < count && i < 12; i++) {
      int row = i / 4;
      int col = i % 4;
      int x_pos = ID_X_START + col * (ID_W + ID_SPACING);
      int y_pos = ID_Y_START + row * (ID_H + ID_SPACING);
      tft.fillRect(x_pos, y_pos, ID_W, ID_H, TFT_NAVY);
      tft.drawRect(x_pos, y_pos, ID_W, ID_H, TFT_WHITE);
      tft.setCursor(x_pos + 10, y_pos + 15);
      if (isFingerprint) {
        tft.print("ID "); tft.print(storedFingerprints[i]);
      } else {
        tft.print("ID "); tft.print(storedRFIDIDs[i]);
      }
    }
  }
}

// Hàm vẽ xác nhận xóa
void drawConfirmDeleteScreen(String item, bool isFingerprint) {
  digitalWrite(TOUCH_CS, HIGH);
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 320, 240, TFT_BLUE);
  tft.fillRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_W, BACK_BUTTON_H, TFT_RED);
  tft.drawRect(BACK_BUTTON_X, BACK_BUTTON_Y, BACK_BUTTON_W, BACK_BUTTON_H, TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(BACK_BUTTON_X + 10, BACK_BUTTON_Y + 5);
  tft.println("<");
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 100);
  tft.println("Xac nhan xoa " + item);
  tft.fillRect(CONFIRM_OK_X, CONFIRM_OK_Y, CONFIRM_OK_W, CONFIRM_OK_H, TFT_NAVY);
  tft.drawRect(CONFIRM_OK_X, CONFIRM_OK_Y, CONFIRM_OK_W, CONFIRM_OK_H, TFT_WHITE);
  tft.setCursor(CONFIRM_OK_X + 50, CONFIRM_OK_Y + 15);
  tft.println("OK");
  tft.fillRect(CONFIRM_CANCEL_X, CONFIRM_CANCEL_Y, CONFIRM_CANCEL_W, CONFIRM_CANCEL_H, TFT_NAVY);
  tft.drawRect(CONFIRM_CANCEL_X, CONFIRM_CANCEL_Y, CONFIRM_CANCEL_W, CONFIRM_CANCEL_H, TFT_WHITE);
  tft.setCursor(CONFIRM_CANCEL_X + 30, CONFIRM_CANCEL_Y + 15);
  tft.println("Cancel");
}

// Hàm hiển thị thông báo
void showMessage(String msg) {
  digitalWrite(TOUCH_CS, HIGH);
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 320, 240, TFT_BLUE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(30, 100);
  tft.println(msg);
  Serial.println("Message: " + msg);
  delay(1500);
  currentScreen = MAIN;
  switchToTouch();
  drawMainScreen();
}

// Hàm kiểm tra vân tay
bool checkFingerprint(int &fingerID) {
  Serial.println("Scanning fingerprint...");
  for (int i = 0; i < 5; i++) {
    int result = finger.getImage();
    if (result == FINGERPRINT_OK) {
      Serial.println("Fingerprint detected! Processing!");
      result = finger.image2Tz();
      if (result != FINGERPRINT_OK) {
        Serial.printf("image2Tz failed: %d\n", result);
        return false;
      }
      result = finger.fingerFastSearch();
      if (result == FINGERPRINT_OK) {
        fingerID = finger.fingerID - 1; // Chuyển về ID tuần tự (1 -> 0, 2 -> 1, ...)
        Serial.print("Found ID: ");
        Serial.println(fingerID);
        return true;
      } else if (result == FINGERPRINT_NOTFOUND) {
        Serial.println("Fingerprint not found");
      } else {
        Serial.printf("fingerFastSearch failed: %d\n", result);
      }
      return false;
    } else if (result != FINGERPRINT_NOFINGER) {
      Serial.printf("Error getting image: %d\n", result);
      return false;
    }
    delay(50);
  }
  Serial.println("No fingerprint detected");
  return false;
}

// Hàm kiểm tra vân tay đã lưu
bool checkStoredFingerprint(int fingerID) {
  for (int i = 0; i < fingerprintCount; i++) {
    if (storedFingerprints[i] == fingerID) {
      return true;
    }
  }
  return false;
}

// Hàm kiểm tra RFID Master
bool checkMasterRFID(byte *uid) {
  if (rfidCount == 0) return false;
  for (int i = 0; i < RFID_UID_SIZE; i++) {
    if (storedRFIDs[0][i] != uid[i]) {
      return false;
    }
  }
  return true;
}

// Hàm thêm vân tay
void addFingerprint() {
  if (fingerprintCount >= MAX_FINGERPRINTS) {
    showMessage("Danh sach day");
    return;
  }
  Serial.println("Adding new fingerprint...");
  int id = fingerprintCount; // Sử dụng số thứ tự làm ID

  // Bước 1: Chụp ảnh vân tay lần 1
  drawScanScreen("Dat van tay len...");
  unsigned long startTime = millis();
  bool fingerDetected = false;
  while (millis() - startTime < 10000) {
    int result = finger.getImage();
    if (result == FINGERPRINT_OK) {
      fingerDetected = true;
      break;
    } else if (result != FINGERPRINT_NOFINGER) {
      Serial.printf("Error getting image (1): %d\n", result);
      showMessage("Loi lay hinh anh");
      return;
    }
    delay(50);
  }
  if (!fingerDetected) {
    showMessage("Khong phat hien van tay");
    return;
  }

  // Xử lý ảnh lần 1
  showScanMessage("Dang xu ly...");
  if (finger.image2Tz(1) != FINGERPRINT_OK) {
    Serial.println("image2Tz(1) failed");
    showMessage("Loi xu ly hinh anh");
    return;
  }

  // Bước 2: Yêu cầu bỏ vân tay
  showScanMessage("Bo van tay ra...");
  delay(2000);

  // Bước 3: Chụp ảnh vân tay lần 2
  showScanMessage("Dat van tay lai...");
  startTime = millis();
  fingerDetected = false;
  while (millis() - startTime < 10000) {
    int result = finger.getImage();
    if (result == FINGERPRINT_OK) {
      fingerDetected = true;
      break;
    } else if (result != FINGERPRINT_NOFINGER) {
      Serial.printf("Error getting image (2): %d\n", result);
      showMessage("Loi lay hinh anh thu 2");
      return;
    }
    delay(50);
  }
  if (!fingerDetected) {
    showMessage("Khong phat hien van tay");
    return;
  }

  // Xử lý ảnh lần 2
  showScanMessage("Dang xu ly...");
  if (finger.image2Tz(2) != FINGERPRINT_OK) {
    Serial.println("image2Tz(2) failed");
    showMessage("Loi xu ly hinh anh thu 2");
    return;
  }

  // Bước 4: Tạo mô hình vân tay
  if (finger.createModel() != FINGERPRINT_OK) {
    Serial.println("createModel failed");
    showMessage("Loi tao mo hinh");
    return;
  }

  // Bước 5: Lưu vân tay với ID = id + 1 (AS608 dùng ID từ 1)
  if (finger.storeModel(id + 1) != FINGERPRINT_OK) {
    Serial.println("storeModel failed");
    showMessage("Loi luu van tay");
    return;
  }

  // Lưu vào danh sách và EEPROM
  storedFingerprints[fingerprintCount] = id;
  saveFingerprint(fingerprintCount);
  fingerprintCount++;
  showMessage("Them OK. ID = " + String(id));
}

// Hàm xóa vân tay
void deleteFingerprint(int id) {
  if (id < 0 || id >= fingerprintCount || !checkStoredFingerprint(id)) {
    showMessage("ID khong hop le");
    return;
  }
  if (finger.deleteModel(id + 1) != FINGERPRINT_OK) {
    showMessage("Loi xoa van tay");
    return;
  }
  for (int i = 0; i < fingerprintCount; i++) {
    if (storedFingerprints[i] == id) {
      for (int j = i; j < fingerprintCount - 1; j++) {
        storedFingerprints[j] = storedFingerprints[j + 1];
      }
      fingerprintCount--;
      saveAllFingerprints();
      showMessage("Xoa ID " + String(id) + " OK");
      return;
    }
  }
}

// Hàm kiểm tra RFID
bool checkRFID(byte *uid) {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return false;
  }
  for (int i = 0; i < RFID_UID_SIZE; i++) {
    uid[i] = rfid.uid.uidByte[i];
  }
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  return true;
}

// Hàm kiểm tra RFID đã lưu
bool checkStoredRFID(byte *uid) {
  for (int i = 0; i < rfidCount; i++) {
    bool match = true;
    for (int j = 0; j < RFID_UID_SIZE; j++) {
      if (storedRFIDs[i][j] != uid[j]) {
        match = false;
        break;
      }
    }
    if (match) return true;
  }
  return false;
}

// Hàm thêm RFID
void addRFID() {
  if (rfidCount >= MAX_RFID_CARDS) {
    showMessage("Danh sach RFID day");
    return;
  }
  drawScanScreen("Quet the RFID...");
  switchToRFID();
  lastRFIDTime = millis();
  byte uid[RFID_UID_SIZE];
  bool cardDetected = false;
  while (millis() - lastRFIDTime < 10000) {
    if (checkRFID(uid)) {
      cardDetected = true;
      break;
    }
    delay(50);
  }
  switchToTouch();
  if (!cardDetected) {
    showMessage("Khong phat hien the");
    return;
  }
  if (checkStoredRFID(uid)) {
    showMessage("The da ton tai");
    return;
  }
  for (int i = 0; i < RFID_UID_SIZE; i++) {
    storedRFIDs[rfidCount][i] = uid[i];
  }
  storedRFIDIDs[rfidCount] = rfidCount;
  saveRFID(rfidCount);
  rfidCount++;
  showMessage("Them RFID OK: ID = " + String(storedRFIDIDs[rfidCount - 1]));
}

// Hàm xóa RFID
void deleteRFID(int id) {
  if (id < 0 || id >= rfidCount) {
    showMessage("ID khong hop le");
    return;
  }
  int index = -1;
  for (int i = 0; i < rfidCount; i++) {
    if (storedRFIDIDs[i] == id) {
      index = i;
      break;
    }
  }
  if (index == -1) {
    showMessage("The khong ton tai");
    return;
  }
  for (int k = index; k < rfidCount - 1; k++) {
    for (int j = 0; j < RFID_UID_SIZE; j++) {
      storedRFIDs[k][j] = storedRFIDs[k + 1][j];
    }
    storedRFIDIDs[k] = storedRFIDIDs[k + 1];
  }
  rfidCount--;
  saveAllRFIDs();
  showMessage("Xoa RFID OK: ID = " + String(id));
}

// Hàm xóa toàn bộ vân tay
void clearAllFingerprints() {
  Serial.println("Clearing all fingerprints...");
  if (finger.emptyDatabase() != FINGERPRINT_OK) {
    Serial.println("Failed to clear fingerprints");
    return;
  }
  fingerprintCount = 0;
  saveAllFingerprints();
  Serial.println("All fingerprints cleared");
}

// Hàm xóa toàn bộ RFID
void clearAllRFIDs() {
  rfidCount = 0;
  saveAllRFIDs();
  Serial.println("All RFIDs cleared");
}

// Hàm lưu vân tay vào EEPROM
void saveFingerprint(int index) {
  EEPROM.write(index, storedFingerprints[index]);
  EEPROM.write(EEPROM_SIZE - 1, fingerprintCount);
  EEPROM.commit();
}

// Hàm lưu RFID vào EEPROM
void saveRFID(int index) {
  for (int i = 0; i < RFID_UID_SIZE; i++) {
    EEPROM.write(MAX_FINGERPRINTS + index * RFID_UID_SIZE + i, storedRFIDs[index][i]);
  }
  EEPROM.write(MAX_FINGERPRINTS + MAX_RFID_CARDS * RFID_UID_SIZE + index, storedRFIDIDs[index]);
  EEPROM.write(EEPROM_SIZE - 2, rfidCount);
  EEPROM.commit();
}

// Hàm tải vân tay từ EEPROM
void loadFingerprints() {
  fingerprintCount = EEPROM.read(EEPROM_SIZE - 1);
  if (fingerprintCount > MAX_FINGERPRINTS) fingerprintCount = 0;
  for (int i = 0; i < fingerprintCount; i++) {
    storedFingerprints[i] = EEPROM.read(i);
  }
}

// Hàm tải RFID từ EEPROM
void loadRFIDs() {
  rfidCount = EEPROM.read(EEPROM_SIZE - 2);
  if (rfidCount > MAX_RFID_CARDS) rfidCount = 0;
  for (int i = 0; i < rfidCount; i++) {
    for (int j = 0; j < RFID_UID_SIZE; j++) {
      storedRFIDs[i][j] = EEPROM.read(MAX_FINGERPRINTS + i * RFID_UID_SIZE + j);
    }
    storedRFIDIDs[i] = EEPROM.read(MAX_FINGERPRINTS + MAX_RFID_CARDS * RFID_UID_SIZE + i);
  }
}

// Hàm lưu tất cả vân tay
void saveAllFingerprints() {
  for (int i = 0; i < fingerprintCount; i++) {
    saveFingerprint(i);
  }
  EEPROM.write(EEPROM_SIZE - 1, fingerprintCount);
  EEPROM.commit();
}

// Hàm lưu tất cả RFID
void saveAllRFIDs() {
  for (int i = 0; i < rfidCount; i++) {
    saveRFID(i);
  }
  EEPROM.write(EEPROM_SIZE - 2, rfidCount);
  EEPROM.commit();
}

// Hàm cài đặt
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("XPT2046 Touchscreen, AS608 Fingerprint, and MFRC522 RFID Interface");

  // Cấu hình các chân
  pinMode(TOUCH_CS, OUTPUT); digitalWrite(TOUCH_CS, HIGH);
  pinMode(RFID_CS, OUTPUT); digitalWrite(RFID_CS, HIGH);
  pinMode(VSPI_SCK, OUTPUT); digitalWrite(VSPI_SCK, HIGH);
  pinMode(VSPI_MOSI, OUTPUT); digitalWrite(VSPI_MOSI, HIGH);
  pinMode(VSPI_MISO, INPUT);

  // Khởi động TFT (HSPI)
  tft.init();
  tft.setRotation(1);
  Serial.println("TFT initialized");

  // Khởi động VSPI cho touch
  SPI.begin(VSPI_SCK, VSPI_MISO, VSPI_MOSI, TOUCH_CS);
  if (!ts.begin()) {
    Serial.println("Failed to initialize touchscreen!");
    while (1);
  }
  ts.setRotation(2);
  Serial.println("Touchscreen initialized");

  // Khởi động RFID
  switchToRFID();
  Serial.println("RFID initialized");
  switchToTouch();

  // Khởi động AS608
  Serial.println("Initializing Fingerprint sensor...");
  mySerial.begin(57600, SERIAL_8N1, 16, 17);
  finger.begin(57600);
  if (finger.verifyPassword()) {
    Serial.println("Fingerprint sensor ready");
  } else {
    Serial.println("Trying baud rate 9600...");
    mySerial.begin(9600, SERIAL_8N1, 16, 17);
    finger.begin(9600);
    if (finger.verifyPassword()) {
      Serial.println("Fingerprint sensor ready (9600 baud)");
    } else {
      Serial.println("Can't connect to fingerprint sensor");
      while (1);
    }
  }

  // Khởi động EEPROM và xóa dữ liệu
  EEPROM.begin(EEPROM_SIZE);
  clearAllFingerprints();
  clearAllRFIDs();

  // Vẽ giao diện chính
  drawMainScreen();
  Serial.println("Setup complete");
}

// Hàm vòng lặp chính
void loop() {
  if (currentMode == TOUCH_MODE) {
    if (ts.touched()) {
      TS_Point p = ts.getPoint();
      if (p.z >= 400 && p.z <= 3000) {
        int16_t x = map(p.x, 300, 3800, 0, 320);
        int16_t y = map(p.y, 300, 3800, 0, 240);
        x = constrain(x, 0, 319);
        y = constrain(y, 0, 239);
        Serial.print("Touch X: "); Serial.print(x);
        Serial.print(", Y: "); Serial.println(y);

        // Xử lý nút Back
        if (currentScreen == AUTH || currentScreen == SETTINGS || currentScreen == SCAN_FINGERPRINT || currentScreen == ADD_FINGERPRINT || currentScreen == DELETE_FINGERPRINT || currentScreen == FORGOT_PASS_RFID || currentScreen == CONFIRM_DELETE_FINGERPRINT || currentScreen == SCAN_RFID || currentScreen == ADD_RFID || currentScreen == DELETE_RFID || currentScreen == CONFIRM_DELETE_RFID) {
          if (x >= BACK_BUTTON_X && x <= BACK_BUTTON_X + BACK_BUTTON_W &&
              y >= BACK_BUTTON_Y && y <= BACK_BUTTON_Y + BACK_BUTTON_H) {
            Serial.println("Quay lai giao dien chinh");
            currentScreen = MAIN;
            deleteInput = "";
            selectedFingerprintID = 0;
            selectedRFIDID = 0;
            memset(selectedRFID, 0, RFID_UID_SIZE);
            switchToTouch();
            drawMainScreen();
            return;
          }
        }

        // Xử lý giao diện chính
        if (currentScreen == MAIN) {
          if (x >= AUTH_BUTTON_X && x <= AUTH_BUTTON_X + AUTH_BUTTON_W &&
              y >= AUTH_BUTTON_Y && y <= AUTH_BUTTON_Y + AUTH_BUTTON_H) {
            Serial.println("Xac thuc duoc chon");
            currentScreen = AUTH;
            drawAuthScreen();
          } else if (x >= SETTING_BUTTON_X && x <= SETTING_BUTTON_X + SETTING_BUTTON_W &&
                     y >= SETTING_BUTTON_Y && y <= SETTING_BUTTON_Y + AUTH_BUTTON_H) {
            Serial.println("Cai dat duoc chon");
            currentScreen = SETTINGS;
            drawSettingsScreen();
          }
        }
        // Xử lý giao diện xác thực
        else if (currentScreen == AUTH) {
          if (x >= PASS_BUTTON_X && x <= PASS_BUTTON_X + PASS_BUTTON_W &&
              y >= PASS_BUTTON_Y && y <= PASS_BUTTON_Y + PASS_BUTTON_H) {
            Serial.println("Nhap mat khau duoc chon");
            currentScreen = PASS_INPUT;
            password = "";
            drawKeyboardScreen("Nhap mat khau", password);
          } else if (x >= RFID_BUTTON_X && x <= RFID_BUTTON_X + RFID_BUTTON_W &&
                     y >= RFID_BUTTON_Y && y <= RFID_BUTTON_Y + RFID_BUTTON_H) {
            Serial.println("Quet the RFID duoc chon");
            currentScreen = SCAN_RFID;
            drawScanScreen("Quet the RFID...");
            switchToRFID();
            lastRFIDTime = millis();
          } else if (x >= FINGER_BUTTON_X && x <= FINGER_BUTTON_X + FINGER_BUTTON_W &&
                     y >= FINGER_BUTTON_Y && y <= FINGER_BUTTON_Y + FINGER_BUTTON_H) {
            Serial.println("Quet van tay duoc chon");
            currentScreen = SCAN_FINGERPRINT;
            drawScanScreen("Quet van tay...");
          }
        }
        // Xử lý giao diện cài đặt
        else if (currentScreen == SETTINGS) {
          if (x >= CHANGE_PASS_X && x <= CHANGE_PASS_X + CHANGE_PASS_W &&
              y >= CHANGE_PASS_Y && y <= CHANGE_PASS_Y + CHANGE_PASS_H) {
            Serial.println("Doi mat khau duoc chon");
            currentScreen = CHANGE_PASS_OLD;
            password = "";
            drawKeyboardScreen("Nhap mat khau cu", password);
          } else if (x >= ADD_FINGER_X && x <= ADD_FINGER_X + ADD_FINGER_W &&
                     y >= ADD_FINGER_Y && y <= ADD_FINGER_Y + ADD_FINGER_H) {
            Serial.println("Them van tay duoc chon");
            currentScreen = ADD_FINGER_AUTH;
            password = "";
            drawKeyboardScreen("Nhap mat khau", password);
          } else if (x >= DEL_FINGER_X && x <= DEL_FINGER_X + DEL_FINGER_W &&
                     y >= DEL_FINGER_Y && y <= DEL_FINGER_Y + DEL_FINGER_H) {
            Serial.println("Xoa van tay duoc chon");
            currentScreen = DELETE_FINGERPRINT;
            drawDeleteListScreen("Danh sach van tay:", true);
          } else if (x >= FORGOT_PASS_X && x <= FORGOT_PASS_X + FORGOT_PASS_W &&
                     y >= FORGOT_PASS_Y && y <= FORGOT_PASS_Y + FORGOT_PASS_H) {
            Serial.println("Quen mat khau duoc chon");
            currentScreen = FORGOT_PASS_RFID;
            drawScanScreen("Quet RFID Master...");
            switchToRFID();
            lastRFIDTime = millis();
          } else if (x >= ADD_RFID_X && x <= ADD_RFID_X + ADD_RFID_W &&
                     y >= ADD_RFID_Y && y <= ADD_RFID_Y + ADD_RFID_H) {
            Serial.println("Them the RFID duoc chon");
            currentScreen = ADD_RFID_AUTH;
            password = "";
            drawKeyboardScreen("Nhap mat khau", password);
          } else if (x >= DEL_RFID_X && x <= DEL_RFID_X + DEL_RFID_W &&
                     y >= DEL_RFID_Y && y <= DEL_RFID_Y + DEL_RFID_H) {
            Serial.println("Xoa the RFID duoc chon");
            currentScreen = DELETE_RFID;
            drawDeleteListScreen("Danh sach RFID:", false);
          }
        }
        // Xử lý nhập mật khẩu
        else if (currentScreen == PASS_INPUT) {
          if (y >= KEY_Y1 && y <= KEY_Y1 + KEY_H) {
            if (x >= KEY_X1 && x <= KEY_X1 + KEY_W) password += "1";
            else if (x >= KEY_X2 && x <= KEY_X2 + KEY_W) password += "2";
            else if (x >= KEY_X3 && x <= KEY_X3 + KEY_W) password += "3";
          } else if (y >= KEY_Y2 && y <= KEY_Y2 + KEY_H) {
            if (x >= KEY_X1 && x <= KEY_X1 + KEY_W) password += "4";
            else if (x >= KEY_X2 && x <= KEY_X2 + KEY_W) password += "5";
            else if (x >= KEY_X3 && x <= KEY_X3 + KEY_W) password += "6";
          } else if (y >= KEY_Y3 && y <= KEY_Y3 + KEY_H) {
            if (x >= KEY_X1 && x <= KEY_X1 + KEY_W) password += "7";
            else if (x >= KEY_X2 && x <= KEY_X2 + KEY_W) password += "8";
            else if (x >= KEY_X3 && x <= KEY_X3 + KEY_W) password += "9";
          } else if (y >= KEY_Y4 && y <= KEY_Y4 + KEY_H) {
            if (x >= KEY_X1 && x <= KEY_X1 + KEY_W) password = "";
            else if (x >= KEY_X2 && x <= KEY_X2 + KEY_W) password += "0";
            else if (x >= KEY_X3 && x <= KEY_X3 + KEY_W) {
              if (password == storedPassword) {
                showMessage("Dang nhap thanh cong");
              } else {
                showMessage("Mat khau sai");
              }
              return;
            }
          }
          drawKeyboardScreen("Nhap mat khau", password);
        }
        // Xử lý đổi mật khẩu (mật khẩu cũ)
        else if (currentScreen == CHANGE_PASS_OLD) {
          if (y >= KEY_Y1 && y <= KEY_Y1 + KEY_H) {
            if (x >= KEY_X1 && x <= KEY_X1 + KEY_W) password += "1";
            else if (x >= KEY_X2 && x <= KEY_X2 + KEY_W) password += "2";
            else if (x >= KEY_X3 && x <= KEY_X3 + KEY_W) password += "3";
          } else if (y >= KEY_Y2 && y <= KEY_Y2 + KEY_H) {
            if (x >= KEY_X1 && x <= KEY_X1 + KEY_W) password += "4";
            else if (x >= KEY_X2 && x <= KEY_X2 + KEY_W) password += "5";
            else if (x >= KEY_X3 && x <= KEY_X3 + KEY_W) password += "6";
          } else if (y >= KEY_Y3 && y <= KEY_Y3 + KEY_H) {
            if (x >= KEY_X1 && x <= KEY_X1 + KEY_W) password += "7";
            else if (x >= KEY_X2 && x <= KEY_X2 + KEY_W) password += "8";
            else if (x >= KEY_X3 && x <= KEY_X3 + KEY_W) password += "9";
          } else if (y >= KEY_Y4 && y <= KEY_Y4 + KEY_H) {
            if (x >= KEY_X1 && x <= KEY_X1 + KEY_W) password = "";
            else if (x >= KEY_X2 && x <= KEY_X2 + KEY_W) password += "0";
            else if (x >= KEY_X3 && x <= KEY_X3 + KEY_W) {
              if (password == storedPassword) {
                currentScreen = CHANGE_PASS_NEW;
                newPassword = "";
                drawKeyboardScreen("Nhap mat khau moi", newPassword);
              } else {
                showMessage("Mat khau cu sai");
              }
              return;
            }
          }
          drawKeyboardScreen("Nhap mat khau cu", password);
        }
        // Xử lý đổi mật khẩu (mật khẩu mới)
        else if (currentScreen == CHANGE_PASS_NEW) {
          if (y >= KEY_Y1 && y <= KEY_Y1 + KEY_H) {
            if (x >= KEY_X1 && x <= KEY_X1 + KEY_W) newPassword += "1";
            else if (x >= KEY_X2 && x <= KEY_X2 + KEY_W) newPassword += "2";
            else if (x >= KEY_X3 && x <= KEY_X3 + KEY_W) newPassword += "3";
          } else if (y >= KEY_Y2 && y <= KEY_Y2 + KEY_H) {
            if (x >= KEY_X1 && x <= KEY_X1 + KEY_W) newPassword += "4";
            else if (x >= KEY_X2 && x <= KEY_X2 + KEY_W) newPassword += "5";
            else if (x >= KEY_X3 && x <= KEY_X3 + KEY_W) newPassword += "6";
          } else if (y >= KEY_Y3 && y <= KEY_Y3 + KEY_H) {
            if (x >= KEY_X1 && x <= KEY_X1 + KEY_W) newPassword += "7";
            else if (x >= KEY_X2 && x <= KEY_X2 + KEY_W) newPassword += "8";
            else if (x >= KEY_X3 && x <= KEY_X3 + KEY_W) newPassword += "9";
          } else if (y >= KEY_Y4 && y <= KEY_Y4 + KEY_H) {
            if (x >= KEY_X1 && x <= KEY_X1 + KEY_W) newPassword = "";
            else if (x >= KEY_X2 && x <= KEY_X2 + KEY_W) newPassword += "0";
            else if (x >= KEY_X3 && x <= KEY_X3 + KEY_W) {
              storedPassword = newPassword;
              showMessage("Doi mat khau thanh cong");
              return;
            }
          }
          drawKeyboardScreen("Nhap mat khau moi", newPassword);
        }
        // Xử lý xác thực thêm vân tay
        else if (currentScreen == ADD_FINGER_AUTH) {
          if (y >= KEY_Y1 && y <= KEY_Y1 + KEY_H) {
            if (x >= KEY_X1 && x <= KEY_X1 + KEY_W) password += "1";
            else if (x >= KEY_X2 && x <= KEY_X2 + KEY_W) password += "2";
            else if (x >= KEY_X3 && x <= KEY_X3 + KEY_W) password += "3";
          } else if (y >= KEY_Y2 && y <= KEY_Y2 + KEY_H) {
            if (x >= KEY_X1 && x <= KEY_X1 + KEY_W) password += "4";
            else if (x >= KEY_X2 && x <= KEY_X2 + KEY_W) password += "5";
            else if (x >= KEY_X3 && x <= KEY_X3 + KEY_W) password += "6";
          } else if (y >= KEY_Y3 && y <= KEY_Y3 + KEY_H) {
            if (x >= KEY_X1 && x <= KEY_X1 + KEY_W) password += "7";
            else if (x >= KEY_X2 && x <= KEY_X2 + KEY_W) password += "8";
            else if (x >= KEY_X3 && x <= KEY_X3 + KEY_W) password += "9";
          } else if (y >= KEY_Y4 && y <= KEY_Y4 + KEY_H) {
            if (x >= KEY_X1 && x <= KEY_X1 + KEY_W) password = "";
            else if (x >= KEY_X2 && x <= KEY_X2 + KEY_W) password += "0";
            else if (x >= KEY_X3 && x <= KEY_X3 + KEY_W) {
              if (password == storedPassword) {
                currentScreen = ADD_FINGERPRINT;
                drawScanScreen("Quet van tay de them...");
              } else {
                showMessage("Mat khau sai");
              }
              return;
            }
          }
          drawKeyboardScreen("Nhap mat khau", password);
        }
        // Xử lý xác thực thêm RFID
        else if (currentScreen == ADD_RFID_AUTH) {
          if (y >= KEY_Y1 && y <= KEY_Y1 + KEY_H) {
            if (x >= KEY_X1 && x <= KEY_X1 + KEY_W) password += "1";
            else if (x >= KEY_X2 && x <= KEY_X2 + KEY_W) password += "2";
            else if (x >= KEY_X3 && x <= KEY_X3 + KEY_W) password += "3";
          } else if (y >= KEY_Y2 && y <= KEY_Y2 + KEY_H) {
            if (x >= KEY_X1 && x <= KEY_X1 + KEY_W) password += "4";
            else if (x >= KEY_X2 && x <= KEY_X2 + KEY_W) password += "5";
            else if (x >= KEY_X3 && x <= KEY_X3 + KEY_W) password += "6";
          } else if (y >= KEY_Y3 && y <= KEY_Y3 + KEY_H) {
            if (x >= KEY_X1 && x <= KEY_X1 + KEY_W) password += "7";
            else if (x >= KEY_X2 && x <= KEY_X2 + KEY_W) password += "8";
            else if (x >= KEY_X3 && x <= KEY_X3 + KEY_W) password += "9";
          } else if (y >= KEY_Y4 && y <= KEY_Y4 + KEY_H) {
            if (x >= KEY_X1 && x <= KEY_X1 + KEY_W) password = "";
            else if (x >= KEY_X2 && x <= KEY_X2 + KEY_W) password += "0";
            else if (x >= KEY_X3 && x <= KEY_X3 + KEY_W) {
              if (password == storedPassword) {
                currentScreen = ADD_RFID;
                drawScanScreen("Quet the RFID de them...");
                switchToRFID();
                lastRFIDTime = millis();
              } else {
                showMessage("Mat khau sai");
              }
              return;
            }
          }
          drawKeyboardScreen("Nhap mat khau", password);
        }
        // Xử lý xóa vân tay
        else if (currentScreen == DELETE_FINGERPRINT) {
          for (int i = 0; i < fingerprintCount && i < 12; i++) {
            int row = i / 4;
            int col = i % 4;
            int x_pos = ID_X_START + col * (ID_W + ID_SPACING);
            int y_pos = ID_Y_START + row * (ID_H + ID_SPACING);
            if (x >= x_pos && x <= x_pos + ID_W &&
                y >= y_pos && y <= y_pos + ID_H) {
              selectedFingerprintID = storedFingerprints[i];
              Serial.println("Selected fingerprint ID: " + String(selectedFingerprintID));
              currentScreen = CONFIRM_DELETE_FINGERPRINT;
              drawConfirmDeleteScreen("ID " + String(selectedFingerprintID), true);
              return;
            }
          }
        }
        // Xử lý xóa RFID
        else if (currentScreen == DELETE_RFID) {
          for (int i = 0; i < rfidCount && i < 12; i++) {
            int row = i / 4;
            int col = i % 4;
            int x_pos = ID_X_START + col * (ID_W + ID_SPACING);
            int y_pos = ID_Y_START + row * (ID_H + ID_SPACING);
            if (x >= x_pos && x <= x_pos + ID_W &&
                y >= y_pos && y <= y_pos + ID_H) {
              selectedRFIDID = storedRFIDIDs[i];
              for (int j = 0; j < RFID_UID_SIZE; j++) {
                selectedRFID[j] = storedRFIDs[i][j];
              }
              Serial.println("Selected RFID ID: " + String(selectedRFIDID));
              currentScreen = CONFIRM_DELETE_RFID;
              drawConfirmDeleteScreen("ID " + String(selectedRFIDID), false);
              return;
            }
          }
        }
        // Xử lý xác nhận xóa vân tay
        else if (currentScreen == CONFIRM_DELETE_FINGERPRINT) {
          if (x >= CONFIRM_OK_X && x <= CONFIRM_OK_X + CONFIRM_OK_W &&
              y >= CONFIRM_OK_Y && y <= CONFIRM_OK_Y + CONFIRM_OK_H) {
            Serial.println("Confirm delete ID: " + String(selectedFingerprintID));
            deleteFingerprint(selectedFingerprintID);
            currentScreen = DELETE_FINGERPRINT;
            drawDeleteListScreen("Danh sach van tay:", true);
            return;
          } else if (x >= CONFIRM_CANCEL_X && x <= CONFIRM_CANCEL_X + CONFIRM_CANCEL_W &&
                     y >= CONFIRM_CANCEL_Y && y <= CONFIRM_CANCEL_Y + CONFIRM_OK_H) {
            Serial.println("Cancel delete");
            currentScreen = DELETE_FINGERPRINT;
            drawDeleteListScreen("Danh sach van tay:", true);
            return;
          }
        }
        // Xử lý xác nhận xóa RFID
        else if (currentScreen == CONFIRM_DELETE_RFID) {
          if (x >= CONFIRM_OK_X && x <= CONFIRM_OK_X + CONFIRM_OK_W &&
              y >= CONFIRM_OK_Y && y <= CONFIRM_OK_Y + CONFIRM_OK_H) {
            Serial.println("Confirm delete RFID ID: " + String(selectedRFIDID));
            deleteRFID(selectedRFIDID);
            currentScreen = DELETE_RFID;
            drawDeleteListScreen("Danh sach RFID:", false);
            return;
          } else if (x >= CONFIRM_CANCEL_X && x <= CONFIRM_CANCEL_X + CONFIRM_CANCEL_W &&
                     y >= CONFIRM_CANCEL_Y && y <= CONFIRM_CANCEL_Y + CONFIRM_OK_H) {
            Serial.println("Cancel delete");
            currentScreen = DELETE_RFID;
            drawDeleteListScreen("Danh sach RFID:", false);
            return;
          }
        }
      }
    }
  } else if (currentMode == RFID_MODE) {
    // Xử lý quét RFID
    if (currentScreen == SCAN_RFID || currentScreen == ADD_RFID || currentScreen == FORGOT_PASS_RFID) {
      byte uid[RFID_UID_SIZE];
      if (checkRFID(uid)) {
        if (currentScreen == SCAN_RFID) {
          if (checkStoredRFID(uid)) {
            for (int i = 0; i < rfidCount; i++) {
              bool match = true;
              for (int j = 0; j < RFID_UID_SIZE; j++) {
                if (storedRFIDs[i][j] != uid[j]) {
                  match = false;
                  break;
                }
              }
              if (match) {
                showMessage("RFID OK: ID = " + String(storedRFIDIDs[i]));
                break;
              }
            }
          } else {
            showMessage("RFID sai");
          }
        } else if (currentScreen == ADD_RFID) {
          addRFID();
        } else if (currentScreen == FORGOT_PASS_RFID) {
          if (checkMasterRFID(uid)) {
            storedPassword = "1234";
            showMessage("Dat lai MK OK");
          } else {
            showMessage("RFID Master sai");
          }
        }
      }
      if (millis() - lastRFIDTime > 8000) {
        showScanMessage("RFID timeout");
        delay(1000);
        currentScreen = MAIN;
        switchToTouch();
        drawMainScreen();
      }
    }
  }

  // Xử lý quét vân tay
  if (currentScreen == SCAN_FINGERPRINT || currentScreen == ADD_FINGERPRINT) {
    int fingerID = 0;
    if (currentScreen == ADD_FINGERPRINT) {
      addFingerprint();
    } else if (checkFingerprint(fingerID)) {
      if (currentScreen == SCAN_FINGERPRINT) {
        if (checkStoredFingerprint(fingerID)) {
          showMessage("Van tay OK. ID = " + String(fingerID));
        } else {
          showMessage("Van tay sai");
        }
      }
    } else {
      if (currentScreen == SCAN_FINGERPRINT) {
        showMessage("Van tay sai");
      }
    }
  }

  delay(50);
}