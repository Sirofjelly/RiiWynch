# RiiWynch

RiiWynch controls a motorized surfboard through an engine-mounted main unit and a handheld remote.

## Language

**Main Unit**:
The engine-mounted controller responsible for the winch or motor hardware.
_Avoid_: Main display, receiver

**Remote**:
The handheld controller used by the rider to start, stop, and adjust the ride.
_Avoid_: Remote display, sender

**Cruising**:
The ride state in which the Remote considers the motor run accepted by the Main Unit and actively supervised by rider input.
_Avoid_: Running, started

**Arming**:
The pre-cruise state in which the rider is intentionally requesting a motor start, but the ride is not yet cruising.
_Avoid_: Starting, warmup

**Start Acceptance**:
The Main Unit's confirmation that a rider's start request is valid and the motor start sequence has begun.
_Avoid_: Start ACK, received start

**Remote Stop Delay**:
The grace period after the rider releases the Remote controls before the ride is stopped.
_Avoid_: Delay, timeout

**Stop Request**:
The rider's request to end the ride as soon and as reliably as possible.
_Avoid_: Stop packet, release event

**Ride Supervision**:
The Remote's ongoing signal that cruising is still intentionally active, including during the Remote Stop Delay grace period.
_Avoid_: Keepalive, heartbeat
