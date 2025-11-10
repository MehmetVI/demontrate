# demontrate# Calculator Project – C Parser & Evaluator  
**Student:** Mehmet Kaan ULU  
**Student ID:** 231ADB102  
**Course:** RTU – Programming Languages (C Lab)  
**Semester:** Fall 2025  

---

## 🧠 Project Description
This program reads mathematical expressions from a `.txt` file, parses them, evaluates the result, and writes the output to a separate file inside an automatically created output directory.

The program supports the following arithmetic operations:
- Addition: `+`  
- Subtraction: `-`  
- Multiplication: `*`  
- Division: `/`  
- Power: `**`  
- Parentheses: `( ... )`

It also handles:
- Floating-point numbers (e.g. `3.5`, `.25`, `2.`)
- Multiple lines and comments starting with `#`
- Automatic error detection (prints `ERROR:<position>` when syntax or division errors occur)

---

## ⚙️ How to Compile

Open your terminal inside the `src` folder and run:

```bash
gcc -std=c11 -O2 -Wall -Wextra -o calc calc.c -lm
