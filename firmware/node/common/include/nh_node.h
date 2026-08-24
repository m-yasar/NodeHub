/*
 * NodeHub — node yazarının gördüğü tek başlık.
 *
 * Yeni bir node tipi yaparken bu dosyayı include eder, ölçümünü yazar ve
 * nh_node_tanim yapısını doldurursun. Modbus, kimlik atama, CRC ve zamanlama
 * ortak katmanda halledilir; onlara dokunman gerekmez.
 */

#ifndef NH_NODE_H
#define NH_NODE_H

#include <stdint.h>
#include <stdbool.h>

/* --------------------------------------------------------------- */
/* Ölçüm veri tipleri — sözleşmedeki karşılıkları                    */
/* --------------------------------------------------------------- */

typedef enum {
    NH_TIP_UINT16,
    NH_TIP_INT16,
    NH_TIP_UINT32,
    NH_TIP_INT32,
} nh_veri_tipi_t;

/* --------------------------------------------------------------- */
/* Bir ölçüm alanı                                                   */
/*                                                                   */
/* kaynak: ölçüm değişkeninin adresi. Ortak katman Modbus sorgusu    */
/* geldiğinde buradan anlık değeri okur — kopyalama yoktur.          */
/* --------------------------------------------------------------- */

typedef struct {
    uint16_t        offset;     /* register adresi, 0x0010'dan başlar */
    nh_veri_tipi_t  tip;
    const void     *kaynak;
} nh_olcum_t;

/* --------------------------------------------------------------- */
/* Node tanımı — her node projesi bunu tanımlar                      */
/* --------------------------------------------------------------- */

typedef struct {
    uint16_t          tip;          /* node tipi kodu, sözleşmeden      */
    uint16_t          fw_surum;     /* yüksek bayt ana, düşük bayt alt  */
    void            (*baslat)(void);/* bir kez, açılışta                */
    void            (*olc)(void);   /* periyodik olarak                 */
    uint32_t          periyot_ms;   /* olc() ne sıklıkta çağrılacak     */
    const nh_olcum_t *olcumler;
    uint8_t           olcum_adet;
} nh_node_tanim_t;

/* Node projesi bu değişkeni tanımlar. */
extern const nh_node_tanim_t nh_node_tanim;

/* --------------------------------------------------------------- */
/* Durum bayrakları — sözleşme register 0x0002                       */
/* --------------------------------------------------------------- */

#define NH_DURUM_SENSOR_HATASI   (1u << 0)
#define NH_DURUM_HAZIR_DEGIL     (1u << 1)
#define NH_DURUM_KALIBRASYON     (1u << 2)
#define NH_DURUM_BESLEME_DUSUK   (1u << 3)

void     nh_durum_ayarla(uint16_t bayrak);
void     nh_durum_temizle(uint16_t bayrak);
uint16_t nh_durum_oku(void);

/* --------------------------------------------------------------- */
/* Geçersiz ölçüm değerleri                                          */
/*                                                                   */
/* Ölçüm alınamadığında sıfır değil bunlar yazılır — sıfır çoğu      */
/* birimde geçerli bir değerdir.                                     */
/* --------------------------------------------------------------- */

#define NH_GECERSIZ_UINT16   0xFFFFu
#define NH_GECERSIZ_INT16    ((int16_t)0x8000)
#define NH_GECERSIZ_UINT32   0xFFFFFFFFu
#define NH_GECERSIZ_INT32    ((int32_t)0x80000000)

/* --------------------------------------------------------------- */
/* Taşıyıcı algılama                                                 */
/*                                                                   */
/* Node göndermeden önce hattı dinler; biri konuşuyorsa vazgeçer.    */
/* Çakışmaları büyük ölçüde önleyen mekanizma budur.                 */
/*                                                                   */
/* Ama MAX485 gibi true fail-safe alıcısı olmayan transceiver'larda   */
/* hat boştayken çıkış gürültüyle titrer ve sahte kenarlar üretir.    */
/* O durumda node hiç gönderemez.                                     */
/*                                                                   */
/* Kalıcı çözüm hatta kutuplama direnci takmaktır: A yukarı, B aşağı, */
/* ~560 Ω. Modüllerin üstündeki 20 kΩ, 120 Ω sonlandırmaya karşı      */
/* yetersiz kalıyor.                                                  */
/*                                                                   */
/* Belirti: node hiç gönderemez, "gonderemedim — kenar=1" yazar.      */
/* O durumda ya kutuplama direnci tak ya da geçici olarak 0 yap.      */
/* --------------------------------------------------------------- */

#ifndef NH_TASIYICI_ALGILA
#define NH_TASIYICI_ALGILA  1
#endif

/* --------------------------------------------------------------- */
/* Başlatma — node projesinin app_main'i bunu çağırır                */
/* --------------------------------------------------------------- */

void nh_baslat(const nh_node_tanim_t *tanim);

/* Şu anki kimlik. 0 = henüz kimlik alınmadı. */
uint8_t nh_kimlik(void);

#endif
