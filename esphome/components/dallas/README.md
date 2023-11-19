# Dallas Sensor
## Documentation
* [Technical documentation](https://www.analog.com/media/en/technical-documentation/data-sheets/ds18b20.pdf)
* [1wire-communication-through-software](https://www.analog.com/en/technical-articles/1wire-communication-through-software.html)


### Electrical caracteristics
|Logical|Level|
|-|-|
|Input Logic-Low|-0.3 --> +0.8|
|Input Logic-High|+2.2 --> The lower of 5.5 or VDD + 0.3|
|Input Logic-High (Parasite mode)|+3.0 --> The lower of 5.5 or VDD + 0.3|

Above means that +0.8 to +2.2 (or +3.0 in parasite mode) is none-defined. Driving the MCU high while the Sensor is driving Low (or the other way around) will end up in a undefined state

By this reason we will never drive the MCU high, only putting it it tri-state mode (input mode)

### Basic operations
The 1-wire bus has 4 basic operations:
* Write bit 1
* Write bit 0
* Read bit
* Reset

These operations should be private (or protected) within the 1-wire class for a better structure of the code

### Transaction sequence
* Step 1. Initialization
  * Reset pulse by master
  * Precense puls by slave(s)
* Step 2. ROM Command (followed by any required data exchange)
* Step 3. DS18B20 Function Command (followed by any required data exchange)

#### ROM commands
|Command|Code|Description|
|-|-|-|
|Search Rom|[F0h]|When a system is initially powered up, the master must identify the ROM codes of all slave devices on the bus, which allows the master to determine the number of slaves and their device types|
|Read Rom|[33h]|This command can only be used when there is one slave on the bus|
|Match Rom|[55H]|The match ROM command followed by a 64-bit ROM code sequence allows the bus master to address a specific slave device on a multidrop or single-drop bus
Skip Rom|[CCh]|The master can use this command to address all devices on the bus simultaneously without sending out any ROM code information|
|Alarm Search [ECh]|The operation of this command is identical to the operation of the Search ROM command except that only slaves with a set alarm flag will respond|

#### DS18B20 Function Commands
|Command|Code|Description|
|-|-|-|
|Convert T|[44h]|This command initiates a single temperature conversion. Following the conversion, the resulting thermal data is stored in the 2-byte temperature register in the scratchpad memory and the DS18B20 returns to its low-power idle state|
|Write Scratchpad|[4Eh]|This command allows the master to write 3 bytes of data to the DS18B20’s scratchpad|
|Read Scratchpad|[BEh]|This command allows the master to read the contents of the scratchpad|
|Copy Scratchpad|[48h]|This command copies the contents of the scratchpad TH, TL and configuration registers (bytes 2, 3 and 4) to EEPROM|
|Recall E2|[B8h]|This command recalls the alarm trigger values (TH and TL) and configuration data from EEPROM and places the data in bytes 2, 3, and 4, respectively, in the scratchpad memory|
|Read Power Supply|[B4h]|The master device issues this command followed by a read time slot to determine if any DS18B20s on the bus are using parasite power|



### Timing
saf



---
---
---

# Heading1
## Heading2
### Heading3


*Italic*
**Bold**
***Cursive***

> blockquote dfafd

1. One
2. Two

* Bullet1
* Bullet2

`Code`

```
#Code block
{
  "firstName": "John",
  "lastName": "Smith",
  "age": 25
}
```

| Column1 | Column2 |
| ------- | ------- |
| 1 | 2 |
| 1 | 2 |

One Two
Three

Four

term
: definition

- [x] Write the press release
- [ ] Update the website
- [ ] Contact the media


I need to highlight these ==very important words==

[title](https://www.example.com)

![alt text](image.jpg)