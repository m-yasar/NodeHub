# Node Firmware

Ölçümü yapan kartların yazılımı. Her node bir Modbus RTU slave'dir: sürekli ölçüm yapar,
hub sorduğunda cevap verir.

## Klasör düzeni

```
node/
├── common/          tüm node'ların paylaştığı kod
└── node-<olcum>/    her node tipi için ayrı klasör
```

Yeni bir node tipi eklerken `common/` içinde ne kadar az şey değişirse, sözleşme o kadar
sağlamdır. Hiçbir şey değişmiyorsa sözleşme doğru kurulmuş demektir.

## Hedef donanım

32KB flash sınıfı bir MCU. Sahada RS485 üzerinden güncellenebilmesi için küçük bir
bootloader + app CRC doğrulaması yeterli; çift bank flash gerekmez.
