# SmartSan Firmware Output Contract

## Update Behaviour

Immediate update on:
- state change
- valid dispense
- refill reset
- system enable/disable

Periodic idle update:
- every 5 seconds

---

## Output Fields

| Field | Type | Unit | Example | Update Trigger |
|---|---|---|---|---|
| usageCount | integer | events | 14 | dispense + periodic |
| remainingPumps | integer | pumps | 11 | dispense + reset |
| remainingPercent | integer | % | 44 | dispense + reset |
| refillAlert | boolean | true/false | false | refill threshold |
| deviceState | string | enum | IDLE | state transition |
| deviceOnline | boolean | true/false | true | periodic |
| lastDispenseAt | string | ms/time | 124920 | dispense |

---

## State Enum Values

- IDLE
- HAND_DETECTED
- DISPENSING
- WAIT_STABILISE
- CHECK_REFILL
- UPDATE_STATUS
- REFILL_REQUIRED
- DISABLED
- ERROR

---

# Mock Trace 1 — Normal Dispense

t=0s  
state=IDLE  
remainingPumps=25  
refillAlert=false

t=1s  
state=HAND_DETECTED

t=2s  
state=DISPENSING

t=3s  
state=WAIT_STABILISE

t=4s  
state=CHECK_REFILL  
remainingPumps=24

t=5s  
state=UPDATE_STATUS

t=6s  
state=IDLE

---

# Mock Trace 2 — Refill Required

t=0s  
state=IDLE  
remainingPumps=1

t=1s  
state=HAND_DETECTED

t=2s  
state=DISPENSING

t=3s  
state=WAIT_STABILISE

t=4s  
state=CHECK_REFILL  
remainingPumps=0  
refillAlert=true

t=5s  
state=REFILL_REQUIRED

---

# Mock Trace 3 — Recovery / Reset

t=0s  
state=REFILL_REQUIRED  
remainingPumps=0  
refillAlert=true

t=1s  
resetAlert=true

t=2s  
state=UPDATE_STATUS  
remainingPumps=25  
refillAlert=false

t=3s  
state=IDLE