#pragma once
#include "csdr.h"
#include "logging.h"

#include <zmq.h>
#include <algorithm>
#include <execution>
#include <vector>

struct raw {
   private:
    void* context = nullptr;
    void* socket = nullptr;
    freq_shift shift;
    std::vector<float> in;
    std::vector<float> out;

   public:
    raw(float rate, const char* addr) : context(zmq_ctx_new()), socket(zmq_socket(context, ZMQ_PUB)), shift(rate) {
        zmq_bind(socket, addr);
        log(LOG_NOTICE, "zmq:  bind '%s'\n", addr);
    }
    ~raw() {
        zmq_close(socket);
        zmq_ctx_destroy(context);
    }

    template <typename T>
    void process(T const* buffer, std::size_t len) {
        out.resize(len);
        if constexpr (std::is_same_v<T, float>) {
            shift(buffer, out.data(), len);
        } else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
            log(LOG_NOTICE, "raw_sender: resize: %d\n", len);
            in.resize(len);
            // convert from type T to float and store in vector in
            std::transform(std::execution::par_unseq, buffer, buffer + len, in.begin(), [](auto v) { return static_cast<short>(v * std::numeric_limits<T>::max()); });
            log(LOG_NOTICE, "raw_sender: shift: %d\n", len / 2);
            shift(in.data(), out.data(), len / 2);
        } else {
            log(LOG_NOTICE, "raw(process): unsupported type\n");
        }
        log(LOG_NOTICE, "raw send len: %d\n", len * sizeof(decltype(out)::value_type));
        send(out.data(), len * sizeof(decltype(out)::value_type));
    }

    void send(void const* buffer, std::size_t len) {
        auto sent = zmq_send(socket, buffer, len, 0);
        printf("zmq:  sent %ld, %d\n", len, sent);
    }
};
