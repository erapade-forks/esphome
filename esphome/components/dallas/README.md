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