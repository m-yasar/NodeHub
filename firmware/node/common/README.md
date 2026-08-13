# Node — ortak kod

Tüm node tiplerinin paylaştığı katman:

- Modbus RTU slave
- Register haritası ([`contract/`](../../../contract/) tanımından üretilir)
- Bootloader ve firmware güncelleme
- Ortak alanlar: cihaz kimliği, firmware sürümü, durum bayrakları

Node tipine özgü hiçbir şey buraya girmez. Ölçüm elemanına ait hesaplama
`node-<olcum>/` klasöründe durur.
