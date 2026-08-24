# Sözleşme — hem hub hem node bu bileşeni kullanır.
# Protokol sabitleri (nodehub_protocol.h) ve CRC gerçeklemesi burada durur;
# iki tarafta ayrı kopya bulunmaz.

idf_component_register(
    SRCS         "nh_crc.c"
    INCLUDE_DIRS "."
)
