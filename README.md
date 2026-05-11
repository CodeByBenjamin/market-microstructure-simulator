# Market Microstructure Simulator

A high-performance limit order book simulator in C++20 featuring a price-time priority matching engine with order pool reuse, five heterogeneous agent strategies, and real-time SFML visualization of market microstructure dynamics.

Built to study price formation, liquidity dynamics, and agent behavior in a continuous double auction environment.

---

## Preview

### Simulation
![Simulation](assets/sim.gif)

### Simulation Summary
![Overview](assets/stats.png)

---

## Performance & Design Choices

- **Order pool reuse** eliminates heap allocation in the hot path
- **Deterministic RNG seeds** enable fully reproducible simulations and backtesting
- **Price-time priority** matching ensures correct order execution semantics
- **Modular strategy interface** allows zero-overhead strategy swapping and experimentation
- **Inventory conservation checks** validate simulation correctness at runtime
- **Trader update ordering** ensures consistent agent interaction across ticks

---

## Features

**Market Engine**
- Price-time priority limit order book
- Matching engine with partial fills and cancellations
- Inventory and capital accounting per agent
- Full order lifecycle management

**Trading Agents**
- Market makers — two-sided quoting with inventory-aware pricing
- Momentum traders — directional trading based on recent price movement
- Contrarian traders — mean-reversion logic against short-term deviations
- Fundamental value traders — trading relative to an internal fair-value estimate
- Noise traders — stochastic background order flow

**Diagnostics**
- Per-trader PnL tracking
- Inventory distribution analysis
- Price range and volatility statistics
- Market stability checks

**Visualization**
- Real-time candlestick chart
- Live order book ladder display
- Cumulative depth chart
- Custom SFML UI

---

## Architecture

```
LimitOrderBook       — Price-time priority matching engine and trade execution
Trader               — Capital, inventory, PnL, and active order tracking  
Strategies           — Encapsulated agent decision logic and trading behavior
Simulation Clock     — Discrete-time simulation driver
Visualization        — Order book, depth, and price chart rendering
```

---

## Tech Stack

- C++20
- SFML
- CMake

---

## Requirements

- C++20 compatible compiler
- CMake 3.20+
- Git (for SFML FetchContent)

---

## How to Run

**Build:**
```bash
mkdir build
cd build
cmake ..
cmake --build .
```

**Run:**
```bash
cd bin/Debug
MarketSim.exe
```

---

## Motivation

This project explores electronic exchange mechanics, market microstructure, and agent-based price formation from a systems engineering perspective.

The goal was to build a modular, high-performance simulation environment capable of modeling heterogeneous trading behavior and emergent market dynamics — with the same architectural concerns found in production matching engines: allocation efficiency, determinism, and modular extensibility.

---

## License

MIT License