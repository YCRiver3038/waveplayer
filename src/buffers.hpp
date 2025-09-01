#ifndef BUFFERS_H_INCLUDED
#define BUFFERS_H_INCLUDED
#include "stdint.h"
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>
#include <queue>

template <typename DTYPE> class ring_buffer {
    private:
        //std::vector<DTYPE> vectbuf;
        std::queue<DTYPE> quebuf;
        DTYPE* buffer_arr = nullptr;
        DTYPE* buf_ret_dest = nullptr;
        DTYPE* ret_nl_dest = nullptr;
        uint32_t blength = 0;
        uint32_t h_idx = 0;
        uint32_t read_start_idx = 0;
        std::mutex mx_buf_guard;
        uint32_t nl_sz_cur = 0;
        uint32_t stored_length = 0;
        bool nl_allocated = false;
        void put_data_nolock(DTYPE data){
            buffer_arr[h_idx] = data;
            h_idx = (h_idx+1) % blength;
            stored_length++;
            if (stored_length > blength) {
                stored_length = blength;
            }
        }

    public:
        ring_buffer(uint32_t c_blength){
            blength = c_blength;
            //vectbuf.resize(blength);
            buffer_arr = new DTYPE[blength];
            buf_ret_dest = new DTYPE[blength];
            std::memset(buffer_arr, 0, blength);
        }
        ~ring_buffer(){
            if (buffer_arr) {
                //printf("DEBUG (to free):\n  buffer_arr: %p\n", buffer_arr);
                delete[] buffer_arr;
            }
            if (buf_ret_dest){
                //printf("DEBUG (to free):\n  buf_ret_dest: %p\n", buf_ret_dest);
                delete[] buf_ret_dest;
            }
            if (ret_nl_dest) {
                //printf("DEBUG (to free):\n  ret_nl_dest: %p\n", ret_nl_dest);
                delete[] ret_nl_dest;
            }
        }
        uint32_t inline get_buf_length(){
            return blength;
        }
        uint32_t inline get_start_index(){
            return read_start_idx;
        }
        uint32_t get_stored_length(){
            return stored_length;
        }
        void init_buffer(){
            if (!buffer_arr) {
                return;
            }
            std::memset(buffer_arr, 0, blength);
            h_idx = 0;
            read_start_idx = 0;
            stored_length = 0;
        }
        void put_data(DTYPE data){
            std::lock_guard<std::mutex> buf_sput_lock(mx_buf_guard);
            put_data_nolock(data);
        }
        void put_data_queue(DTYPE data) {
            quebuf.push(data);
            h_idx = (h_idx+1) % blength;
            stored_length++;
            if (stored_length > blength) {
                blength = stored_length;
            }
        }
        void put_data_arr(DTYPE* data_arr, uint32_t length) {
            std::lock_guard<std::mutex> buf_aput_lock(mx_buf_guard);
            for (uint32_t tempctr=0; tempctr<length; tempctr++) {
                put_data_nolock(data_arr[tempctr]);
            }
        }
        void put_data_arr_queue(DTYPE* data_arr, uint32_t length) {
            std::lock_guard<std::mutex> buf_aput_lock(mx_buf_guard);
            for (uint32_t tempctr=0; tempctr<length; tempctr++) {
                put_data_queue(data_arr[tempctr]);
            }
        }
        void put_data_memcpy(DTYPE* data_arr, uint32_t length) {
            std::lock_guard<std::mutex> buf_aput_lock(mx_buf_guard);
            if (!buffer_arr) {
                return;
            }
            uint32_t ac_length = length;
            uint32_t length_capped = length;
            uint32_t remains = 0;
            if (length > blength) {
                length_capped = blength;
            }
            if ((h_idx+length_capped) >= blength) {
                ac_length = blength - h_idx;
                remains = length_capped - ac_length;
            }
            memcpy(&(buffer_arr[h_idx]), data_arr, ac_length*sizeof(DTYPE));
            h_idx = (h_idx+ac_length) % blength;
            if (remains != 0) {
                memcpy(&(buffer_arr[h_idx]), &(data_arr[ac_length]), remains*sizeof(DTYPE));
                h_idx = remains;
            }
            if (stored_length+length_capped > blength) {
                stored_length = blength;
            } else {
                stored_length += length_capped;
            }
        }
        DTYPE get_data_single(){
            std::lock_guard<std::mutex> buf_sget_lock(mx_buf_guard);
            if (stored_length != 0) {
                stored_length--;
            }
            return buffer_arr[h_idx];
        }
        DTYPE get_data_single_queue() {
            DTYPE data;
            if (!quebuf.empty()) {
                data = quebuf.front();
                quebuf.pop();
                if (stored_length != 0) {
                    stored_length--;
                }
            }
            return data;
        }
        DTYPE* get_data_nelm_queue(uint32_t length) {
            std::lock_guard<std::mutex> buf_nget_lock(mx_buf_guard);
            if (nl_sz_cur != length) {
                if (nl_allocated) {
                    delete[] ret_nl_dest;
                }
                ret_nl_dest = new DTYPE[length];
                nl_allocated = true;
                nl_sz_cur = length;
            }
            uint32_t temp_cpcount = 0;
            while (temp_cpcount < length) {
                ret_nl_dest[temp_cpcount] = get_data_single_queue();
                temp_cpcount++;
            }
            return ret_nl_dest;
        }
        DTYPE* get_data_nelm(uint32_t length){
            std::lock_guard<std::mutex> buf_nget_lock(mx_buf_guard);
            if (nl_sz_cur != length) {
                if (nl_allocated) {
                    delete[] ret_nl_dest;
                }
                ret_nl_dest = new DTYPE[length];
                nl_allocated = true;
                nl_sz_cur = length;
            }
            int temp_cpcount = 0;
            int temp_idx = read_start_idx;
            while (temp_cpcount < length) {
                ret_nl_dest[temp_cpcount] = buffer_arr[temp_idx];
                temp_idx = (temp_idx+1) % blength;
                temp_cpcount++;
                stored_length--;
                if (stored_length == 0) {
                    break;
                }
            }
            read_start_idx = temp_idx;
            return ret_nl_dest;
        }
        DTYPE* get_data_memcpy(uint32_t length) {
            std::lock_guard<std::mutex> buf_nget_lock(mx_buf_guard);
            if (!buffer_arr) {
                return nullptr;
            }
            if (nl_sz_cur != length) {
                if (nl_allocated) {
                    delete[] ret_nl_dest;
                }
                ret_nl_dest = new DTYPE[length];
                nl_allocated = true;
                nl_sz_cur = length;
            }
            uint32_t ac_length = length;
            uint32_t length_capped = length;
            uint32_t remains = 0;
            if (length > blength) {
                length_capped = blength;
            }
            if ((read_start_idx+length_capped) >= blength) {
                ac_length = blength - read_start_idx;
                remains = length_capped - ac_length;
            }
            memcpy(ret_nl_dest, &(buffer_arr[read_start_idx]), ac_length*sizeof(DTYPE));
            read_start_idx = (read_start_idx+ac_length) % blength;
            if (remains != 0) {
                memcpy(&(ret_nl_dest[ac_length]), &(buffer_arr[read_start_idx]), remains*sizeof(DTYPE));
                read_start_idx = remains;
            }
            if (stored_length >= length_capped) {
                stored_length -= length_capped;
            } else {
                stored_length = 0;
            }
            return ret_nl_dest;
        }
        DTYPE* get_data_array(uint32_t& hidx_dest){
            std::lock_guard<std::mutex> buf_aget_lock(mx_buf_guard);
            int tempctr = h_idx;
            int cp_count = 0;
            while (cp_count < blength) {
                buf_ret_dest[cp_count] = buffer_arr[tempctr];
                tempctr = (tempctr+1) % blength;
                cp_count++;
            }
            hidx_dest = h_idx;
            return buf_ret_dest;
        }
};
#endif