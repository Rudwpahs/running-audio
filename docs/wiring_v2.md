# Wiring — Prototype v2

## Transmitter

No external audio ADC is used.

- PC USB port -> USB-C data cable -> LILYGO T3-S3 SX1280 TX
- Attach the supplied SX1280 antenna before powering any radio transmission.

## Receiver radio board to MAX98357A

| LILYGO receiver | MAX98357A |
|---|---|
| VBUS 5V | VIN |
| GND | GND |
| GPIO43 | BCLK |
| GPIO44 | LRC / WS |
| GPIO21 | DIN |

GPIO43 and GPIO44 are free board pins. GPIO21 is associated with the external socket, so verify the physical breakout on the purchased board before soldering.

## MAX98357A to transducer

Connect the single bone-conduction transducer to the amplifier speaker output terminals. Do not connect either speaker output terminal directly to ground.

## First-power safety

1. Connect both radio antennas before transmitting.
2. Power the receiver from USB only; do not add a battery in the first test.
3. Keep the transducer off the head during the first electrical test.
4. The firmware limits samples to an absolute value of 4096.
5. Test at low PC volume first and increase only after checking heat and comfort.
6. Stop immediately if the amplifier, transducer, or board becomes unusually hot.
