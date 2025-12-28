<div align="center">
  <a href="https://ollama.com">
    <img alt="ollama" width="240" src="https://github.com/ollama/ollama/assets/3325447/0d0b44e2-8f4a-4e99-9b52-a5c1c741c8f7">
  </a>
</div>

# Ollama AuxRNG

This community fork of Ollama introduces **AuxRNG**: an *optional* auxiliary randomness source that can be specified at runtime for use during token sampling.

With this, Ollama can draw from any byte-producing device node for sampling decisions instead of using the default internal PRNG path. The model weights do **not** change — only the sampler's randomness stream does.

> AuxRNG is meant for experimentation and research work and is especially useful for those interested in supplying "true/quantum randomness" to LLMs. If you don’t explicitly enable it, this Ollama behaves the same as upstream (PRNG).

---

## What is AuxRNG?

**AuxRNG** is a small integration point that provides an environment hook `OLLAMA_AUXRNG_DEV` by which the runtime will pull bytes from a specified external device node (e.g. a hardware TRNG like `/dev/ttyACM0`) and feed those bytes to the sampler.

- Does **not** change model files, prompts, or the REST API schema
- Works with any model you run through Ollama
- Can be enabled/disabled at runtime (via environment)
- Throughput/latency depends on your device and how you read from it

If the environment hook is undefined, or cannot be opened or read, Ollama falls back to default internal PRNG behavior (and logs accordingly).

AuxRNG is device-agnostic; technically it can read from any device node that produces bytes, though its intended use here involves TRNG or QRNG devices.

---

## Process flow

In this iteration, AuxRNG simply feeds fresh bytes "on-demand" to the chooser sampler on a per-token basis. The intention is to provide "live randomness" for selecting each token rather than snapshot determinism from a prefilled buffer.

Runtime logging is somewhat verbose at present; `Auxiliary RNG:` prints with metadata for each read of your AuxRNG device (~per-token). 


## Quick Start

### 1) Identify the node path of your auxiliary device or randomness source

Examples:

- `/dev/ttyACM0` (typical of TrueRNGpro V2 hardware RNG)
- `/dev/urandom` (Linux kernel-provided cryptographic pseudorandom byte stream)

### 2) Ensure that filesystem permissions allow the device to be read

On many Linux distros, the user that will serve Llama needs to be added to the `dialout` group so that Llama can read from the serial device:

```bash
sudo usermod -aG dialout "$USER"
# then log out/in
```

### 3) (Optional but recommended) Install official Ollama to establish GPU support libraries and a convenient side-by-side testing environment.

GPU (CUDA) libaries are included with the official Ollama installation. The custom ollama-auxrng binary will use those. If you don't have it and you want GPU support and/or a side-by-side testing environment, install it.

```bash
curl -fsSL https://ollama.com/install.sh | sh
```

### 4) Clone AuxRNG, build

```bash
git clone https://github.com/orphiceye/ollama-auxrng.git ~/ollama-auxrng
cd ~/ollama-auxrng
go clean -cache
# We will place the ollama-auxrng binary alongside the standard ollama binary - same path use by the official Ollama installation (typically /usr/local/bin/).
# This helps the auxrng find CUDA libs at runtime for GPU support in case you want it.
go build -buildvcs=false -o /usr/local/bin/ollama-auxrng
```

### 5) Set the environment and launch

I recommend using an alternative port like 11435 to avoid clashing with another Ollama instance on the default 11434. In this manner they can run side-by-side.

```bash
#Environment variables
OLLAMA_HOST=http://0.0.0.0:11435 \
OLLAMA_MODELS=/path/to/your/language/models \
OLLAMA_AUXRNG_DEV=/dev/yourDeviceNode \
/usr/local/bin/ollama-auxrng serve
```
