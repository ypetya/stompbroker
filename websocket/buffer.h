/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   buffer.h
 * Author: peter
 *
 * Created on December 30, 2018, 9:11 PM
 */

#ifndef BUFFER_H
#define BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct buffer_item_t {
        int fd;
        char mask[4];
        size_t received_len;
        size_t frame_len; // total len (calculated on for the first header in the buffer)
        char* received;
    } buffer_item;

#define WS_CONTINUATION_BUFFER 1000
#define WS_MAX_BUFFER_SIZE 10000000


    void ws_init_buffer();
    void ws_deinit_buffer();

    int ws_buffer_size_left(size_t size_left);

    buffer_item* ws_buffer_find(int fd);
    void ws_buffer_resize(buffer_item * buffer, size_t old_len, size_t new_len);
    buffer_item* ws_buffer_add(int fd, char*buffer, size_t len);

    struct ws_buffer_stat_t {
        size_t allocated_size;
        size_t hit;
        size_t miss;
    };

    struct ws_buffer_stat_t * ws_buffer_get_stats();

#ifdef __cplusplus
}
#endif

#endif /* BUFFER_H */

