# 🏭 Industrial Safety Mesh  
### ESP32 Conveyor Monitoring & Emergency System

**Node A (ESP32) + Node B (ESP32-S3) + Sensor IR + LCD I2C + Motor DC + Railway Backend + Web Dashboard**

---

# 📌 Deskripsi Project
Project ini merupakan **sistem keamanan dan monitoring konveyor industri berbasis IoT** yang mengintegrasikan dua mikrokontroler ESP32, sensor infrared, motor DC, serta dashboard web real-time melalui backend Railway.

Sistem mampu:

- Mendeteksi objek menggunakan **sensor IR** (menghitung produk)
- Mengontrol **kecepatan motor secara real-time dari web** (PWM 0–255)
- **Tombol darurat fisik** dengan prioritas tertinggi via **ESP-NOW**
- Menampilkan data di **LCD I2C** (counter & PWM)
- Mengirim data ke **backend Railway** (counter, PWM, status emergency)
- Menerima perintah dari **web dashboard melalui Railway** (HTTP polling)
- Update data setiap **1–1.5 detik**

---

# 🎯 Tujuan Project

- Mengembangkan sistem keamanan industri dengan **prioritas emergency yang tinggi**
- Mengintegrasikan **ESP-NOW untuk komunikasi darurat lokal**
- Membangun **backend cloud (Railway)** untuk kontrol dan monitoring dari mana saja
- Mengimplementasikan **komunikasi dua arah** antara hardware dan web dashboard
- Menampilkan data **real-time di LCD dan web**

---

# ⚙️ Cara Kerja Sistem

## 🔄 Alur Data Utama
┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐
│ NODE A │ │ NODE B │ │ BACKEND │
│ (Tombol) │────▶│ (Motor/Sensor)│◀───▶│ RAILWAY │
│ ESP-NOW │ │ ESP32-S3 │ │ (Node.js) │
└─────────────────┘ └─────────────────┘ └─────────────────┘
│ │ │
ESP-NOW HTTP (POST/GET) WebSocket
(prioritas) (tiap 1–1.5 detik) (real-time)
│
┌─────▼─────┐
│ WEB │
│ DASHBOARD │
└───────────┘

---

# 📋 Penjelasan Tiap Komponen

## Node A (ESP32)
- Membaca **tombol darurat (GPIO4)**
- Setiap tekanan mengirim sinyal **emergency = 1 atau 0** via **ESP-NOW** ke Node B
- Menggunakan **interrupt dan debounce** untuk respons cepat

---

## Node B (ESP32-S3)

Fungsi utama:

- Menerima sinyal **emergency dari Node A** → langsung menghentikan / menjalankan motor
- **Sensor IR (GPIO2)** mendeteksi objek → counter bertambah
- Mengirim data **counter, PWM, emergency status** ke Railway via **HTTP POST**
- Melakukan **HTTP GET** tiap 1 detik untuk membaca perintah kontrol
- Menampilkan data pada **LCD I2C**
- Mengontrol motor melalui **driver L298N**

---

## Backend Railway (Node.js + Express + WebSocket)

Fungsi:

- Menyediakan **REST API**
- Menyimpan data dari Node B
- Mengirim data **real-time ke web dashboard**

### Endpoint REST

| Method | Endpoint | Fungsi |
|------|------|------|
| POST | `/api/data` | Menerima data dari Node B |
| GET | `/api/data` | Mengambil data terbaru |
| GET | `/api/control` | Dibaca Node B untuk kontrol |
| POST | `/api/control` | Perintah dari web dashboard |

### WebSocket

Digunakan untuk mengirim data **real-time ke web dashboard**

---

## Web Dashboard (HTML + JavaScript)

Fitur:

- Terhubung ke Railway via **WebSocket**
- Menampilkan:

  - Status emergency  
  - Counter IR  
  - Nilai PWM  
  - Waktu update terakhir  

- **Slider PWM** untuk kontrol motor
- **Tombol Cancel Emergency**

---

# 🚀 Fitur Utama

✅ Emergency stop fisik dengan prioritas tertinggi  
✅ Kontrol kecepatan motor dari web (PWM 0–255)  
✅ Monitoring counter produk via sensor IR  
✅ Tampilan LCD 16x2 (counter & PWM)  
✅ Backend cloud Railway dengan REST API dan WebSocket  
✅ Web dashboard real-time  
✅ Sinkronisasi **WiFi & ESP-NOW channel**  
✅ Debounce hardware & software  
✅ Auto reconnect WiFi dan WebSocket  

---

# 🛠 Hardware yang Digunakan

| Komponen | Jumlah | Keterangan |
|--------|--------|--------|
| ESP32 (Node A) | 1 | Tombol darurat |
| ESP32-S3 (Node B) | 1 | Kontrol motor & WiFi |
| Sensor IR | 1 | Menghitung objek |
| Motor DC | 1 | Simulasi conveyor |
| Driver Motor L298N | 1 | Pengendali motor |
| Push Button | 1 | Emergency stop |
| LCD I2C 16x2 | 1 | Display |
| Baterai 9V | 1 | Catu daya motor |
| Breadboard & Jumper | - | Wiring |

---

# 🔌 Konfigurasi Pin

## ESP32-S3 (Node B)

| Komponen | GPIO | Keterangan |
|--------|--------|--------|
| Sensor IR | 2 | Interrupt |
| LCD SDA | 9 | I2C |
| LCD SCL | 8 | I2C |
| Motor IN1 | 36 | Arah motor |
| Motor IN2 | 37 | Arah motor |
| Motor ENA | 35 | PWM |

⚠ Catatan:  
Pin **35, 36, 37** pada beberapa board ESP32-S3 terhubung dengan **PSRAM**.  
Jika motor tidak stabil, gunakan **18, 19, 21**.

---

## ESP32 (Node A)

| Komponen | GPIO | Keterangan |
|--------|--------|--------|
| Push Button | 4 | Input Pull-up |

---

# 🖥 Tampilan LCD
Count: 1234
PWM: 150

## 📍 Kondisi Normal

Baris 1 → jumlah objek  
Baris 2 → nilai PWM

---

## 🚨 Kondisi Emergency
EMERGENCY STOP!!
Motor berhenti


Motor berhenti dan **tidak merespon perintah PWM**

---

# 🌐 Integrasi Backend Railway

Backend menggunakan:

- **Node.js**
- **Express**
- **WebSocket**

### Dependencies
express
ws

---

# 🔁 Alur Kontrol PWM
1. Web dashboard mengirim:

```json
{
  "type": "set_pwm",
  "pwm": 200
}
2.Backend memperbarui controlState.pwm
3.Node B membaca melalui GET /api/control
4.Node B mengubah kecepatan motor
5.LCD dan dashboard ikut diperbarui

🔁 Alur Cancel Emergency

Web dashboard mengirim
{
  "type": "cancel_emergency",
  "cancel": 1
}
2.Backend mengubah state cancel
3.Node B membaca melalui GET /api/control

Saat ini emergency hanya benar-benar dikontrol oleh Node A (tombol fisik).

🖥 Web Dashboard
Fitur :
WebSocket real-time
Status RUN / STOP
Counter IR
PWM value
Update time
PWM slider
Cancel emergency button

Contoh Kode :
const ws = new WebSocket(WS_URL);

ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  updateDashboard(data);
};

fetch(`${RAILWAY_URL}/api/control`, {
  method: "POST",
  headers: { "Content-Type": "application/json" },
  body: JSON.stringify({ type: "set_pwm", pwm: parseInt(value) })
});

📊 Informasi Tambahan
| Parameter             | Interval  |
| --------------------- | --------- |
| Data send ke Railway  | 1.5 detik |
| Check control command | 1 detik   |
| LCD refresh           | 300 ms    |
| IR debounce           | 50 ms     |
| Button debounce       | 120 ms    |

📝 Catatan Penting
Channel WiFi & ESP-NOW harus sama
Pin motor ESP32-S3 mungkin berbeda tergantung board
Emergency dari web belum sepenuhnya diimplementasikan
Cancel emergency dari web masih placeholder
