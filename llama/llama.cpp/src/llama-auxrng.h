#pragma once

struct llama_context;

// Enable Auxiliary RNG provider from environment (no-op if env vars not set)
void llama_auxrng_try_enable_from_env(llama_context * ctx);

// Cleanup provider state owned by llama-auxrng (safe to call always)
void llama_auxrng_cleanup(llama_context * ctx);