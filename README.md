# TinyCache

TinyCache is a Redis-like in-memory cache server implemented in C++20.

## Run with Docker

1. Build the image:

```bash
docker build -t tinycache:local .
```

2. Run the container:

```bash
docker run --rm -p 8080:8080 tinycache:local
```

The server reads `config.toml` from `/app/config.toml` inside the container.
To provide a custom config file:

```bash
docker run --rm -p 8080:8080 \
  -v "$(pwd)/config.toml:/app/config.toml:ro" \
  tinycache:local
```

## Run with Docker Compose

```bash
docker compose up --build
```
