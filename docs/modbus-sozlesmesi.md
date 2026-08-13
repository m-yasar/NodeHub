# Modbus Sözleşmesi

> ⏳ **Bu belge henüz yazılmadı — yol haritasında Adım 2.**
> Kod yazılmadan önce netleşmesi gereken tek şey budur.

Sözleşme, tüm node'ların uyacağı ortak register haritasıdır. Hub'ın herhangi bir node'u,
ne ölçtüğünü bilmeden okuyabilmesini sağlar.

## Neyi tanımlar

Bir node'u sisteme tanıtmak için gereken alanlar:

| Alan | Ne işe yarar |
|---|---|
| Slave adres | Hat üzerinde node'u ayırt eder |
| Function code | Hangi Modbus komutuyla okunacağı |
| Register offset | Değerin hangi adresten başladığı |
| Register sayısı | Kaç register kapladığı |
| Veri tipi | Sayının nasıl yorumlanacağı |
| Ölçek | Ham değerden gerçek değere çevrim |
| Birim | °C, %RH, bar, rpm … |
| Periyot | Ne sıklıkta okunacağı |

Buna ek olarak her node'un ortak alanları olur: cihaz kimliği, firmware sürümü, durum bayrakları.
Bunlar node tipinden bağımsız, sabit adreslerde durur.

Sözleşmenin makine-okunur hâli [`contract/registers.yaml`](../contract/registers.yaml)
dosyasında tutulur.
