#include "llama-auxrng.h"
#include <atomic>
#include "llama-context.h"   // for llama_context methods (get/set_auxrng_provider)
#include "llama-impl.h"           // for LLAMA_LOG_* and llama_auxrng_cb typedef

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>  // for std::snprintf

#include <fcntl.h>
#include <unistd.h>

struct llama_auxrng_fd {
    int fd = -1;
};

static bool llama_auxrng_read_fd(void * user_data, uint8_t * out, size_t n) {
    auto * st = (llama_auxrng_fd *) user_data;
    if (!st || st->fd < 0) return false;

    static std::atomic<uint64_t> calls{0};
    const uint64_t call_n = ++calls;

    // Read exactly n bytes from the auxrng device
    size_t got = 0;
    while (got < n) {
        const ssize_t r = ::read(st->fd, out + got, n - got);
        if (r > 0) { got += (size_t) r; continue; }
        if (r == 0) return false;           // EOF
        if (errno == EINTR) continue;       // retry
        return false;
    }

    // Log EVERY call with clear labels.
    // If n==8, also print the 8 bytes and the uint64 interpretation (host-endian memcpy).
    if (n == 8) {
        uint64_t x = 0;
        std::memcpy(&x, out, 8);

        LLAMA_LOG_INFO(
            "Auxiliary RNG: read callcount=%llu filedescriptor=%d want=%zu bytes=%02x%02x%02x%02x%02x%02x%02x%02x x=0x%016llx\n",
            (unsigned long long) call_n, st->fd, n,
            out[0], out[1], out[2], out[3], out[4], out[5], out[6], out[7],
            (unsigned long long) x
        );
    } else {
        // For other sizes, log only the first up to 16 bytes to avoid excessive output.
        const size_t k = n < 16 ? n : 16;

        // Build a compact hex preview
        char hex[16*2 + 1];
        for (size_t i = 0; i < k; i++) {
            std::snprintf(hex + i*2, 3, "%02x", out[i]);
        }
        hex[k*2] = '\0';

        LLAMA_LOG_INFO(
            "Auxiliary RNG: read callcount=%llu filedescriptor=%d want=%zu preview_bytes=%zu hex=%s%s\n",
            (unsigned long long) call_n, st->fd, n, k, hex, (k < n ? "..." : "")
        );
    }

    return true;
}


void llama_auxrng_try_enable_from_env(llama_context * ctx) {
    if (!ctx) return;

    const char * dev = std::getenv("OLLAMA_AUXRNG_DEV");
    if (!dev || !*dev) return;

    // If already configured, don't override
    if (ctx->get_auxrng_cb() != nullptr) {
        LLAMA_LOG_WARN("OLLAMA_AUXRNG_DEV set but provider already configured; leaving existing provider in place\n");
        return;
    }

    const int fd = ::open(dev, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        LLAMA_LOG_WARN("OLLAMA_AUXRNG_DEV set but open(%s) failed: %s\n", dev, std::strerror(errno));
        return;
    }

    auto * st = new llama_auxrng_fd();
    st->fd = fd;

    // Use your Option A method, not direct member access
    ctx->set_auxrng_provider(llama_auxrng_read_fd, st);

    LLAMA_LOG_INFO("Auxiliary RNG enabled for sampling via OLLAMA_AUXRNG_DEV=%s\n", dev);
}


void llama_auxrng_cleanup(llama_context * ctx) {
    if (!ctx) return;

    auto cb = ctx->get_auxrng_cb();
    auto ud = ctx->get_auxrng_ud();

    // Only clean up state we own
    if (cb == llama_auxrng_read_fd && ud != nullptr) {
        auto * st = (llama_auxrng_fd *) ud;
        if (st->fd >= 0) ::close(st->fd);
        delete st;

        ctx->set_auxrng_provider(nullptr, nullptr);
    }
}
