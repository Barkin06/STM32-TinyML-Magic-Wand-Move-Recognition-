import serial
import time
import csv
import os

SERIAL_PORT = 'COM6'   
BAUD_RATE = 115200
SAMPLE_LIMIT = 200     # 200 veri (yaklaşık 4-5 saniye)

if not os.path.exists('veriler'):
    os.makedirs('veriler')

def veri_topla():
    print(f"\n--- {SERIAL_PORT} PORTUNA BAĞLANILIYOR ---")
    ser = None
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        time.sleep(2) 
    except serial.SerialException:
        print(f"HATA: {SERIAL_PORT} açılamadı! Kabloyu kontrol et.")
        return

    # --- HAREKET SEÇİMİ ---
    print("-" * 40)
    print("1. cizgisel (Linear)")
    print("2. dairesel (Circular)")
    print("3. sabit    (Idle)")
    print("-" * 40)
    
    secim = input("Seçiminiz (1-3): ").strip().lower()
    
    if secim == '1' or secim == 'cizgisel': label = 'cizgisel'
    elif secim == '2' or secim == 'dairesel': label = 'dairesel'
    elif secim == '3' or secim == 'sabit': label = 'sabit'
    else: return

    filename = f"veriler/{label}_{int(time.time())}.csv"
    
    print(f"\n>>> HAZIRLAN! '{label}' hareketi kaydedilecek...")
    for i in range(3, 0, -1):
        print(f"{i}...")
        time.sleep(1)
    
    print("Buffer temizleniyor...")
    try:
        while ser.in_waiting > 0:
            ser.read(ser.in_waiting) # Çöp veriyi oku ve yok et
            time.sleep(0.01)
    except:
        pass 


    print(">>> BAŞLA! <<<")

    data_buffer = []
    
    while len(data_buffer) < SAMPLE_LIMIT:
        try:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                
                if line.startswith("START") and line.endswith("END"):
                    parts = line.split(',')
                    if len(parts) == 5:
                        try:
                            x = int(parts[1])
                            y = int(parts[2])
                            z = int(parts[3])
                            
                            data_buffer.append([x, y, z])
                            
                            kalan = SAMPLE_LIMIT - len(data_buffer)
                            print(f"Kalan: {kalan:03d} | X={x:>5} Y={y:>5} Z={z:>5}", end='\r')
                        except ValueError:
                            pass
        except KeyboardInterrupt:
            break
        except serial.SerialException:
            print("\n HATA: Bağlantı koptu")
            break

    if len(data_buffer) > 0:
        print(f"\n\nTamamlandı! {len(data_buffer)} veri yakalandı.")
        with open(filename, 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(['x', 'y', 'z'])
            writer.writerows(data_buffer)
        print(f"Dosya oluşturuldu:1 {filename}")
    else:
        print("\nVeri toplanamadı.")
        
    if ser.is_open:
        ser.close()

if __name__ == "__main__":
    while True:
        veri_topla()
        if input("\nYeni kayıt? (e/h): ").lower() != 'e': break