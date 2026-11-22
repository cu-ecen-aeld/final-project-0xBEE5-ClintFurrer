
#ifndef QR_READER_APP_H
#define QR_READER_APP_H

typedef struct __attribute__ ((__packed__)) {
    int content_length;
    char content_bytes[254];
} tiny_code_reader_results_t;



#endif /* QR_READER_APP_H */