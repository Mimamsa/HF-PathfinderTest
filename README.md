# HF-PathfinderTest

HF-PathfinderTest 是 HANGFA Pathfinder 遙控器的接收與通訊協定驗證工具，適用於 **WTM-1-WU USB 接收器**。

程式以唯讀方式開啟序列埠，解析 Pathfinder 的控制資料，並即時顯示搖桿、旋鈕、波段開關與按鈕狀態。

## 功能

- 自動辨識並驗證 31-byte Pathfinder 封包
- 顯示所有控制項的即時數值
- 擷取基準值並標示發生變化的控制項
- 可調整類比訊號變化門檻
- 顯示通過 CRC 驗證的原始 HEX 封包
- 將原始串流記錄為二進位檔案
- 支援 Qt 5.15 與 Qt 6.2 以上版本
- 不會向 WTM-1-WU 傳送設定或握手資料

## 連接設定

1. 將 WTM-1-WU 接上電腦。
2. 啟動 HF-PathfinderTest。
3. 選擇接收器的序列埠，Linux 通常是 `/dev/ttyUSB0`。
4. 使用以下參數：

| 設定 | 數值 |
| --- | --- |
| USB UART baud rate | `115200` |
| Data bits | `8` |
| Parity | None |
| Stop bits | `1` |
| Flow control | None |
| RTS | Deasserted |
| 開啟模式 | Read-only |

5. 按下 **Connect**。

Pathfinder 螢幕顯示的 `BAUD: 19200` 是無線鏈路設定。實測 WTM-1-WU 的 USB UART 使用 `115200` baud。

## 已確認的封包格式

每個封包固定為 **31 bytes**。

| Byte | 內容 | 格式 |
| --- | --- | --- |
| 0–4 | 固定標頭 `AA 0F 01 2F 17` | 5 bytes |
| 5–27 | 控制資料 | 詳見下表 |
| 28–29 | CRC | CRC-16/XMODEM，little-endian |
| 30 | 固定結尾 `0D` | 1 byte |

CRC 的計算範圍是 bytes 0–27，初始值為 `0x0000`，多項式為 `0x1021`。

### 控制資料配置

| 控制項 | Byte / bit | 資料格式 |
| --- | --- | --- |
| JOY1 X | bytes 5–6 | `int16` little-endian |
| JOY1 Y | bytes 7–8 | `int16` little-endian |
| JOY1 Z | bytes 9–10 | `int16` little-endian |
| JOY2 X | bytes 11–12 | `int16` little-endian |
| JOY2 Y | bytes 13–14 | `int16` little-endian |
| JOY3 X | bytes 15–16 | `int16` little-endian |
| JOY3 Y | bytes 17–18 | `int16` little-endian |
| POT A | bytes 19–20 | `int16` little-endian |
| POT B | bytes 21–22 | `int16` little-endian |
| Ban A | byte 23 bits 3–0 | 範圍 1–6 |
| Ban B | byte 23 bits 7–4 | 範圍 1–6 |
| ENC A | byte 24 | `int8` |
| ENC B | byte 25 | `int8` |
| JOY1 D | byte 26 bit 6 | Boolean |
| TOG1–TOG5 | byte 26 bits 4–0 | Boolean |
| JOY2 D | byte 27 bit 7 | Boolean |
| JOY3 D | byte 27 bit 6 | Boolean |
| BUT1–BUT6 | byte 27 bits 5–0 | Boolean |

以上配置均已透過實際控制項逐項驗證。

## 控制項驗證方法

1. 將所有搖桿、旋鈕與開關保持不動。
2. 等待程式收到有效封包。
3. 按下 **Capture baseline**。
4. 每次只操作一個控制項。
5. 對應的表格列會變成綠色，並顯示目前值、基準值與差值。
6. 放開按鈕或將搖桿回中，確認綠色標示消失。
7. 測試下一個控制項前，重新擷取基準值。

### Analog threshold

**Analog threshold** 用來忽略搖桿與電位器靜止時的微小雜訊，預設值是 `10`。

- 未操作的類比欄位持續閃綠：提高門檻。
- 操作控制項卻沒有變綠：降低門檻。
- 數位按鈕、波段開關與 encoder 使用精確比較，不受此門檻影響。

## 記錄原始資料

按下 **Record raw data...** 可將接收到的原始 byte stream 儲存成 `.bin` 檔案，供離線分析。

記錄內容是接收器送出的原始資料，包含有效封包以及可能存在的非同步或損壞資料。中斷連線時，程式會自動停止記錄並關閉檔案。

## 建置需求

- C++17 編譯器
- Qt 5.15 以上或 Qt 6.2 以上
- Qt Widgets
- Qt SerialPort
- qmake
- make 或對應平台的建置工具

### Ubuntu 22.04 / Qt 5

安裝相依套件：

```bash
sudo apt install build-essential qtbase5-dev libqt5serialport5-dev
```

建置：

```bash
mkdir build
cd build
qmake ../HF-PathfinderTest.pro
make -j4
./HF-PathfinderTest
```

### Qt 6

使用所安裝 Qt 6 kit 的 qmake：

```bash
mkdir build-qt6
cd build-qt6
/path/to/Qt/6.x/gcc_64/bin/qmake ../HF-PathfinderTest.pro
make -j4
./HF-PathfinderTest
```

## 測試

建立並執行測試：

```bash
mkdir build-tests
cd build-tests
qmake ../tests/receiver.pro
make -j4
QT_QPA_PLATFORM=offscreen ./receiver-test
```

測試內容包括：

- 無效序列埠的錯誤處理
- 透過 pseudo-terminal 接收封包
- CRC 與封包邊界解析
- BUT1 與 JOY D bit mapping
- 基準值擷取與變化偵測
- 斷線與重新連線
- 確認程式不會向接收器傳送資料

若要執行 WTM-1-WU 實機只讀測試：

```bash
QT_QPA_PLATFORM=offscreen \
PATHFINDER_TEST_PORT=/dev/ttyUSB0 \
./receiver-test optionalHardwareReception
```

## 專案結構

```text
HF-PathfinderTest.pro
main.cpp
PathfinderReceiver.h
PathfinderReceiver.cpp
README.md
tests/
├── receiver.pro
└── receiver_test.cpp
```

