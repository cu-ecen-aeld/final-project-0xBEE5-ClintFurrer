
#ifndef QR_READER_APP_H
#define QR_READER_APP_H

typedef struct __attribute__ ((__packed__)) {
    int content_length;
    char content_bytes[254];
    int qr_sens_fd;
} tiny_code_reader_results_t;

int qr_code_init(tiny_code_reader_results_t *qr_sensor);
int qr_code_rdy(tiny_code_reader_results_t *qr_sensor);
int qr_code_read(tiny_code_reader_results_t *qr_sensor);

#endif /* QR_READER_APP_H */