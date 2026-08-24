#include "nh_kurulum.h"
#include "nh_port.h"

static uint8_t s_kimlik;

void nh_kayit_yukle(void)
{
    s_kimlik = nh_port_kimlik_oku();
}

uint8_t nh_kayit_kimlik(void)
{
    return s_kimlik;
}

bool nh_kayit_ata(uint8_t adres)
{
    if (!nh_port_kimlik_yaz(adres)) {
        return false;
    }
    s_kimlik = adres;
    return true;
}

void nh_kayit_sil(void)
{
    nh_port_kimlik_yaz(0);
    s_kimlik = 0;
}
