# PL/0 language reference

This is the complete reference of the language that the program runs:
Niklaus Wirth's PL/0 with floating-point numbers, procedure parameters,
maths functions and turtle graphics. The same compiler and interpreter
run on the Commodore 64 and, for the tests, on a PC (`build/pl0host`), so
everything here holds for both unless a section says otherwise.

New to the program? The [user guide](USER-GUIDE.md) shows how to type,
run and save programs, and has a short tutorial. This page is for looking
things up.

Every PL/0 example on this page is checked by `make test`
(`tools/check_doc_code.py`): it compiles, and where an output or an error
is shown, the host driver gives exactly that.

## Contents

- [A first look](#a-first-look)
- [Lexical rules](#lexical-rules)
- [Grammar](#grammar)
- [Declarations](#declarations)
- [Statements](#statements)
- [Expressions](#expressions)
- [Conditions](#conditions)
- [Procedures](#procedures)
- [Maths functions](#maths-functions)
- [Turtle graphics](#turtle-graphics)
- [Numbers](#numbers)
- [Limits](#limits)
- [Errors](#errors)
- [Differences from Wirth's PL/0](#differences-from-wirths-pl0)

## A first look

```pl0 run
{ the sum of the first n squares }
const n = 5;
var i, s;
begin
  i := 1; s := 0;
  while i <= n do
  begin
    s := s + i * i;
    i := i + 1
  end;
  !'sum = '; !s; !
end.
```

```output
sum = 55
```

A program is a block followed by a full stop. A block declares constants,
variables and procedures, in that order, and ends with one statement
(often a `begin ... end` group). There is one data type, the number.

## Lexical rules

- **Case.** Keywords and names are not case-sensitive: `BEGIN`, `Begin`
  and `begin` are the same word, and so are `Total` and `total`. Text in
  quotes keeps its case.
- **Names** (identifiers) start with a letter, continue with letters and
  digits, and have at most 16 characters. The underscore is not allowed.
- **Reserved words** cannot be used as names: `abs atan begin call const
  cos do end exp forward home if int log moveto odd pendown penup
  procedure sin sqrt then turn var while`.
- **Numbers** are written in decimal: `12`, `3.14`, `0.5`. A number
  starts with a digit (`.5` is an error, write `0.5`), has at most 16
  characters, and has no exponent (`1e5` is not a number; write
  `100000`). A minus sign is an operator, not part of the number. A point
  that is not followed by a digit ends the number, so `!5.` prints 5 and
  ends the program.
- **Strings** are written in single quotes, `'pi = '`, on one line, with
  at most 16 characters. They exist only for the output statement `!`.
  A string cannot contain a single quote.
- **Comments** are written in braces, `{ like this }`, and can stand
  wherever a space can, also over several lines. They do not nest: the
  first `}` ends the comment. On the C64 the braces are typed with
  SHIFT+: and SHIFT+; and the screen shows them as `[` and `]`.
- **Symbols:** `+ - * / ( ) , ; . := = # <> < <= > >= ! ?`. Both `#`
  and `<>` mean "not equal".
- Spaces, tabs and line ends separate words and are otherwise ignored.
  The text is plain ASCII.

## Grammar

In this EBNF, `[ ]` is optional, `{ }` repeats zero or more times, `|`
separates choices, and words in quotes are written as they stand. It
follows the compiler, `src/core/compiler.c`, rule for rule.

```ebnf
program    = block "." .

block      = [ "const" constdef { "," constdef } ";" ]
             [ "var" ident { "," ident } ";" ]
             { "procedure" ident [ "(" ident { "," ident } ")" ] ";" block ";" }
             statement .

constdef   = ident "=" [ "+" | "-" ] number .

statement  = [ ident ":=" expression
             | "call" ident [ "(" expression { "," expression } ")" ]
             | "begin" statement { ";" statement } "end"
             | "if" condition "then" statement
             | "while" condition "do" statement
             | "!" [ expression | string ]
             | "?" ident
             | "forward" "(" expression ")"
             | "turn" "(" expression ")"
             | "moveto" "(" expression "," expression ")"
             | "penup" | "pendown" | "home" ] .

condition  = "odd" expression
           | expression ( "=" | "#" | "<>" | "<" | "<=" | ">" | ">=" ) expression .

expression = term { ( "+" | "-" ) term } .
term       = factor { ( "*" | "/" ) factor } .
factor     = ident | number | "(" expression ")"
           | ( "+" | "-" ) factor
           | function "(" expression ")" .
function   = "sin" | "cos" | "atan" | "sqrt" | "abs" | "int" | "exp" | "log" .
```

Notes on the grammar:

- A statement can be empty, so `begin end`, `begin !1; end` (a `;`
  before `end`) and `;;` are all fine.
- A bare `!` (a new line) must be followed by `;`, `end` or the final
  `.`; anything else is read as the start of an expression.
- An `ident` in a factor must be a constant or a variable, and the
  `ident` of an assignment or of `?` must be a variable.
- There is no `else`, no `for` or `repeat` loop, and no `and`, `or` or
  `not`, as in Wirth's PL/0.

## Declarations

### Constants

```pl0 run
const pi = 3.14159265, steps = 12, down = -1;
begin !pi * steps * down; ! end.
```

```output
-37.6991118
```

A constant is a name for a number. Its value is a number literal with an
optional sign; it cannot be an expression or another constant
(`const a = 1, b = a;` is an error). A constant can stand wherever a
number can, but cannot be assigned to.

### Variables

```pl0 run
var x, y;
begin
  !x; !' '; !y; !;
  x := 1; y := x + 1;
  !x; !' '; !y; !
end.
```

```output
0 0
1 2
```

Every variable holds one number and starts at 0 each time the block
runs (the program, or a procedure on each call).

### Procedures

A procedure declaration has a name, an optional list of parameters in
parentheses, a `;`, a block (its own constants, variables, nested
procedures and statement) and a final `;`. See [Procedures](#procedures).

The order inside a block is fixed: `const`, then `var`, then the
procedures, then the statement.

## Statements

| Statement | Meaning |
|---|---|
| `x := e` | assign the value of the expression `e` to the variable `x` |
| `call p` / `call p(e1, e2)` | run procedure `p`, with the arguments if it has parameters |
| `begin s1; s2; ... end` | run the statements in order |
| `if c then s` | run `s` if the condition `c` holds |
| `while c do s` | run `s` again and again while `c` holds; `c` is tested before each round |
| `!e` | print the value of `e` (no space or new line after it) |
| `!'text'` | print the text |
| `!` | start a new line |
| `?x` | read a number from the keyboard into the variable `x` |
| `forward(e)`, `turn(e)`, `moveto(x, y)`, `penup`, `pendown`, `home` | [turtle graphics](#turtle-graphics) |

### Output

`!` prints exactly what it is given: numbers in the [number
format](#printing-numbers), text as it is, and nothing between them. Put
the spaces and new lines in yourself:

```pl0 run
var i;
begin
  i := 1;
  while i <= 3 do
  begin
    !i; !' squared is '; !i * i; !;
    i := i + 1
  end
end.
```

```output
1 squared is 1
2 squared is 4
3 squared is 9
```

On the C64 the output goes to the 40-column text screen: a line longer
than 40 characters continues on the next row, and the screen scrolls
when it is full.

### Input

`?x` reads a number into the variable `x`. On the C64 it prints `? ` and
waits for a line of up to 20 characters ended by RETURN; DEL deletes the
last character. Spaces around the number are ignored. The number may
have a sign and, unlike a literal in the program, an exponent (`1e3`,
`-2.5E-2`). Anything else, an empty line, or RUN/STOP during the input
ends the program with the runtime error `bad input`.

```pl0
{ reads two numbers and prints their sum }
var a, b;
begin
  ?a; ?b;
  !'sum = '; !a + b; !
end.
```

If the program has drawn something, `?` switches back to the text screen
for the question; the next line the program draws shows the drawing
again.

The host driver used by the tests has no keyboard: there `?` always
ends with `bad input`.

## Expressions

| Priority | Operators | Meaning |
|---|---|---|
| 1 (first) | `( )`, functions, `+` `-` before a factor | grouping, `sin(x)`, sign |
| 2 | `*` `/` | multiply, divide |
| 3 (last) | `+` `-` | add, subtract |

Operators of the same priority work from left to right: `10 - 4 - 3`
is 3 and `12 / 4 / 3` is 1. A sign can stand before any factor, so
`2 * -3` and `- -3` are allowed.

```pl0 run
begin
  !1 + 2 * 3; !' '; !(1 + 2) * 3; !' ';
  !10 - 4 - 3; !' '; !12 / 4 / 3; !' ';
  !2 * -3; !' '; !7 / 2; !
end.
```

```output
7 9 3 1 -6 3.5
```

`/` is the ordinary division of numbers (`7 / 2` is 3.5). There is no
remainder or power operator. A remainder is `a - int(a / b) * b`, as in
the GCD example, and a power is a loop or `exp(y * log(x))` for
positive `x`.

## Conditions

A condition compares two expressions or tests a number with `odd`. It is
used only after `if` and `while`.

| Condition | True when |
|---|---|
| `a = b` | equal |
| `a # b`, `a <> b` | not equal |
| `a < b`, `a <= b` | less, less or equal |
| `a > b`, `a >= b` | greater, greater or equal |
| `odd a` | the integer part of `a` (rounded down) is odd |

```pl0 run
begin
  if odd 3 then !'3 ';
  if odd 4 then !'4 ';
  if odd 2.5 then !'2.5 ';
  if odd -2.5 then !'-2.5 ';
  if 1 <> 2 then !'1<>2';
  !
end.
```

```output
3 -2.5 1<>2
```

(`odd -2.5` is true because -2.5 rounded down is -3.)

Comparisons are exact. Results of arithmetic with fractions can differ
in the last binary digit, so `0.1 + 0.2 = 0.3` can be false: compare
with a margin, as in `abs(a - b) < 0.000001`.

There is no `and`, `or` or `not`. Nest `if` statements for "and", use
the opposite comparison for "not" (`a >= b` for not `a < b`), and test a
flag variable for "or".

## Procedures

```pl0 run
var r;

procedure power(x, n);
begin
  r := 1;
  while n > 0 do
  begin
    r := r * x;
    n := n - 1
  end
end;

begin
  call power(2, 10); !r; !;
  call power(1.5, 2); !r; !
end.
```

```output
1024
2.25
```

- **Parameters** are passed by value: the procedure gets the values of
  the arguments, evaluated from left to right, and its parameters are
  then local variables like the others. Assigning to a parameter (`n`
  above) changes only the procedure's copy.
- **Results** come back through a variable of an enclosing block (`r`
  above). There are no functions written in PL/0.
- **Calls** need exactly as many arguments as there are parameters. A
  procedure without parameters is declared and called without
  parentheses: `procedure p;` and `call p`. Empty parentheses, `()`, are
  an error in both places.
- **Recursion** works: a procedure can call itself, and every call has
  its own parameters and variables (see the factorial, GCD, Koch,
  Sierpinski and tree examples). How deep it can go is in
  [Limits](#limits).
- **Order:** a procedure can call itself, the procedures declared before
  it in the same or an enclosing block, and its own nested procedures.
  There are no forward declarations, so two procedures cannot call each
  other.
- **Nesting:** procedures can be declared inside procedures, up to 4
  levels deep.

### Scope

A name can be used in the block that declares it, after its declaration,
and in every block nested inside it. A name declared in an inner block,
parameters included, hides the same name of an outer block. A nested
procedure sees the parameters and variables of the procedures around
it, as they are in the current call:

```pl0 run
var x, total;

procedure outer(x);
  procedure inner;
  begin
    total := total + x
  end;
begin
  call inner; call inner
end;

begin
  x := 100; total := 0;
  call outer(21);
  !x; !' '; !total; !
end.
```

```output
100 42
```

In the Koch example the parameters `len` and `depth` of the procedure
`koch` hide the constants of the same names: inside the procedure they
are the parameters, outside the constants.

## Maths functions

| Function | Result | Error |
|---|---|---|
| `sin(x)`, `cos(x)` | sine and cosine of `x` in radians | none |
| `atan(x)` | arctangent in radians, between -pi/2 and pi/2 | none |
| `sqrt(x)` | square root | `domain error` if `x` < 0 |
| `abs(x)` | absolute value | none |
| `int(x)` | `x` rounded down to a whole number: `int(2.7)` = 2, `int(-2.7)` = -3 | none |
| `exp(x)` | e to the power `x` | `number overflow` if the result is too large |
| `log(x)` | natural logarithm | `domain error` if `x` <= 0 |

There is no constant for pi: write `const pi = 3.14159265;` or compute
`4 * atan(1)`. The turtle turns in degrees while the functions use
radians: multiply degrees by `pi / 180`.

```pl0 run
begin
  !4 * atan(1); !' '; !sqrt(2); !' '; !exp(1); !' ';
  !log(10); !' '; !int(-2.7); !' '; !abs(-3); !
end.
```

```output
3.14159265 1.41421356 2.71828183 2.30258509 -3 3
```

On the C64 the functions are those of the BASIC ROM; they are slow
(about 28 ms for a sine, 53 ms for a square root) and, for very large
arguments of `sin` and `cos`, inaccurate.

## Turtle graphics

The turtle is a pen on the C64's 320 x 200 high-resolution screen. The
first turtle statement of a run clears the screen to white and shows it
instead of the text; lines are black.

| Statement | Effect |
|---|---|
| `forward(d)` | move `d` units in the direction the turtle faces, drawing if the pen is down; negative `d` moves backwards |
| `turn(a)` | turn `a` degrees clockwise (negative: anticlockwise) |
| `moveto(x, y)` | move to the point (x, y), drawing if the pen is down; the direction does not change |
| `penup` | lift the pen: moves draw nothing |
| `pendown` | lower the pen: moves draw |
| `home` | back to (0, 0) facing up, without drawing; the pen stays as it is |

```text
                 y = 99
                   ^
                   |
  x = -160  <------+------>  x = 159
                   |  (0, 0): the middle
                   v
                 y = -100
```

- **Coordinates:** one unit is one pixel. The origin is the middle of the
  screen, x grows to the right and y upwards. The visible area is x from
  -160 to 159 and y from -100 to 99.
- **Direction:** in degrees; 0 is up, 90 is right (east), 180 down. A
  positive `turn` is clockwise.
- **Start:** every run starts at (0, 0), facing up, with the pen down.
- **Clipping:** the turtle can go anywhere. Lines are cut at the edge of
  the screen; nothing wraps around and nothing outside the screen is
  touched.
- **Precision:** position and direction are numbers, not pixels. Only the
  ends of a line are rounded to the nearest pixel, so a figure that
  returns to its start closes exactly.

```pl0 run
{ an equilateral triangle }
var i;
begin
  penup; moveto(-50, -40); pendown;
  turn(90);
  i := 0;
  while i < 3 do
  begin
    forward(100);
    turn(-120);
    i := i + 1
  end
end.
```

Plotting a function is easiest with `moveto`: lift the pen for the first
point and lower it after, as the SINE and LISSAJOUS examples do.

On the C64 a `turn` computes a sine and a cosine with the ROM (about
56 ms together); `forward` reuses them while the direction stays the
same, so drawings with many turns take longest.
After the program ends, SPACE switches between the drawing and the text
output (see the [user guide](USER-GUIDE.md#running-a-program)).

## Numbers

### One type: floating point

Every value is a floating-point number.

| | On the C64 | On the host (tests) |
|---|---|---|
| Format | Commodore BASIC's 5-byte float (32-bit mantissa) | IEEE double |
| Significant digits | about 9 | about 16 |
| Largest | 1.70141183E+38 | about 1.8E+308 |
| Smallest above 0 | about 2.9E-39 | smallest normal about 2.2E-308; smaller host values lose precision before becoming 0 |
| Too large | runtime error `number overflow` | the same |
| Too small | becomes 0, no error | the same, after losing precision below 2.2E-308 (checked with `build/pl0host`: halving 1 repeatedly prints `4.9406565E-324` last) |

The arithmetic and the functions on the C64 are the BASIC ROM's, so
results can differ from the host in the last digits. Both print the same
way, rounded to 9 digits, so in most programs the output is identical;
it is not when a result depends on the last digits. The NEWTON example
shows this: its final errors print as `1.59472435E-12` and
`-2.22044605E-16` on the host and as `0` on the C64.

The C64 conversions between text and numbers (number literals, `?`
input and printing) do not use the ROM's routines, which are wrong in
the ninth digit; they are exact, so every literal with up to 9
significant digits prints back as written.

### Printing numbers

`!` prints a number with at most 9 significant digits:

| Value | Printed | Rule |
|---|---|---|
| 0, -0 | `0` | |
| 42, -7 | `42`, `-7` | whole numbers below 1E+09: no point |
| 0.1 + 0.2 | `0.3` | otherwise 9 significant digits, trailing zeros removed |
| 1 / 3 | `0.333333333` | |
| 0.0001 | `0.0001` | |
| 0.00001 | `1E-05` | below 0.0001: exponent form |
| 1234567890 | `1.23456789E+09` | from 1E+09 up: exponent form |
| 100000 * 100000 | `1E+10` | the exponent has a sign and at least two digits |
| 999999999.5 | `1E+09` | rounding up to 1E+09 switches to the exponent form |

```pl0 run
begin
  !0.1 + 0.2; !; !1 / 3; !; !0.0001; !; !0.00001; !;
  !1234567890; !; !100000 * 100000; !; !-0.5; !
end.
```

```output
0.3
0.333333333
0.0001
1E-05
1.23456789E+09
1E+10
-0.5
```

The rules are `num_format` in `src/host/num_host.c`; the C64's
assembly version (`src/c64/num_c64.s`) prints the same characters for
the same value, checked on the C64 with 174 random values
(`make c64-num`). A number never needs more than 15 characters.

## Limits

The limits come from the C64's memory (`src/core/limits.h`,
[memory map](architecture/memory-map.md)) and are the same on the host.

| What | Limit | When it is exceeded |
|---|---|---|
| program text | 8,191 characters (the 8 KB buffer at `$A000` holds a terminating zero) | the editor shows `BUFFER FULL`; a longer file does not load (`FILE TOO BIG`) |
| name length | 16 characters | `name too long` |
| string length | 16 characters | `string too long (max 16)` |
| number literal | 16 characters | `bad character or number` |
| names in scope at one point (constants, variables, parameters and procedures of the current block and the blocks around it) | 64 | `too many names` |
| compiled program | 768 P-code instructions (the largest example, PI, needs 153) | `program too big` |
| different number literals | 128 | `program too big` |
| text of all strings | 1,024 bytes (each string takes its length + 1) | `program too big` |
| procedure nesting | 4 levels | `nested too deeply` |
| nested parentheses, signs and function or turtle arguments in one expression | 16 | `nested too deeply` |
| nested `begin`, `if` and `while` | 16 | `nested too deeply` |
| runtime stack | 512 numbers | `stack overflow (too deep)` |

**Recursion depth.** Each active call takes 3 stack cells plus one per
parameter and per variable (and room for its intermediate results), out
of 512. Measured with `build/pl0host`, which uses the same limit as the
C64:

| Recursive procedure | Deepest call that works |
|---|---|
| no parameters, no variables | 168 |
| one parameter (like `fact(k)` in FACTORIAL) | 125-126 |
| two parameters, or one parameter and one variable (like `koch(len, depth)`) | 101 |

The examples need far less: TREE goes 7 calls deep, KOCH 4, FACTORIAL
13. A runaway recursion ends with `stack overflow (too deep)` and
nothing else is harmed; the next run starts clean.

## Errors

When the compiler or the running program finds an error, it stops and
reports it as

```text
ERROR line N: message
```

On the C64 the editor then puts the cursor on line N and shows the
message on the bottom row (runtime errors are first shown on the output
screen, after the output, until a key is pressed). The compiler stops at
the first error.

**Which line.** For an error found by the compiler, N is the line of the
word where it noticed the problem. That is often the word after the
mistake: a missing `;` at the end of line 2 is reported on line 3, where
the next statement starts. For a runtime error, N is the line of the
operator, function or turtle statement that failed (`/` for a division
by zero, `sqrt` for a domain error); if an expression spans several
lines, that can be a different line from the one where the statement
starts. A stack overflow reports the line of the `call` that did not
fit, and `stopped` the line of the `while` condition of the loop that
was running.

```pl0 error:ERROR line 3: ";" expected
begin
  !1
  !2
end.
```

```pl0 error:ERROR line 3: division by zero
var a;
begin
  a := 1 / (2 -
    2)
end.
```

(Line 3: the `/` stands on line 3, the zero divisor is complete on
line 4.)

### All messages

| Message | Kind |
|---|---|
| `unterminated comment` | text |
| `unterminated string` | text |
| `bad character or number` | text |
| `name too long` | text |
| `string too long (max 16)` | text |
| `";" expected` | grammar |
| `"end" expected` | grammar |
| `"." expected` | grammar |
| `"then" expected` | grammar |
| `"do" expected` | grammar |
| `identifier expected` | grammar |
| `"=" expected` | grammar |
| `":=" expected` | grammar |
| `"(" expected` | grammar |
| `")" expected` | grammar |
| `expression expected` | grammar |
| `undeclared name` | names |
| `not a variable` | names |
| `not a procedure` | names |
| `name already declared` | names |
| `wrong number of arguments` | names |
| `too many names` | size |
| `program too big` | size |
| `nested too deeply` | size |
| `division by zero` | runtime |
| `domain error` | runtime |
| `number overflow` | runtime |
| `stack overflow (too deep)` | runtime |
| `stopped` | runtime |
| `bad input` | runtime |

(`ok` is the text of error code 0, "no error"; it is never shown.)

The messages live in one table, `src/core/errors.c`; `make test` fails
if any of them is missing from this page (`tools/check_error_docs.py`).

### Errors in the text

**`unterminated comment`**: a `{` has no matching `}`. The line is where
the comment starts.

```pl0 error:ERROR line 1: unterminated comment
{ a comment that never ends
var x;
x := 1.
```

**`unterminated string`**: a `'` has no closing `'` on the same line.

```pl0 error:unterminated string
!'hello.
```

**`bad character or number`**: a character that has no meaning in PL/0
(such as `@`, `_`, `"` or a lone `:`), a number that starts with a point
(`.5`), or a number longer than 16 characters.

```pl0 error:bad character or number
var first_value;
first_value := 1.
```

**`name too long`**: a name has more than 16 characters.

```pl0 error:name too long
var averagetemperature;
averagetemperature := 20.
```

**`string too long (max 16)`**: a string has more than 16 characters.
Split it over two `!` statements.

```pl0 error:string too long (max 16)
!'the result is now: '.
```

### Grammar errors

**`";" expected`**: two statements or declarations are not separated by
`;`, or the `;` after a declaration or a procedure is missing.

```pl0 error:";" expected
var x
begin x := 1 end.
```

**`"end" expected`**: a `begin` group is not closed (or a statement
inside it is followed by something that cannot continue it).

```pl0 error:"end" expected
begin
  !1;
  !2
.
```

**`"." expected`**: the program does not end with a full stop, or there
is something after the main statement that cannot follow it (for
example a second statement without `begin ... end` around both, or a
`var` after a procedure).

```pl0 error:"." expected
var x;
x := 1
```

**`"then" expected`**: `if` and its condition are not followed by
`then`.

```pl0 error:"then" expected
var x;
if x = 0 x := 1.
```

**`"do" expected`**: `while` and its condition are not followed by `do`.

```pl0 error:"do" expected
var i;
while i < 3 i := i + 1.
```

**`identifier expected`**: a name is needed here: after `var`, `const`,
`procedure`, `call` or `?`, or as a parameter. Empty parentheses and
reserved words used as names give this error too.

```pl0 error:identifier expected
procedure hello();
  !'hello';
call hello.
```

**`"=" expected`**: a constant is declared without `=` (`:=` is for
variables).

```pl0 error:"=" expected
const n := 5;
!n.
```

**`":=" expected`**: an assignment without `:=` (`=` compares).

```pl0 error:":=" expected
var x;
x = 1.
```

**`"(" expected`**: a function or a turtle statement without its
parentheses.

```pl0 error:"(" expected
!sqrt 2.
```

**`")" expected`**: a closing parenthesis is missing. A missing comma in
`moveto` also gives this message.

```pl0 error:")" expected
moveto(10 20).
```

**`expression expected`**: a number, a name or a parenthesis is needed
here. It also appears when a condition has no comparison operator
(`if x then`), when a constant is given anything but a number, and for
`call p()`.

```pl0 error:expression expected
var x;
if x then !1.
```

### Errors with names

**`undeclared name`**: a name is used that is not declared here, is
misspelt, or is declared later (procedures must be declared before they
are called).

```pl0 error:undeclared name
var x;
x := y + 1.
```

**`not a variable`**: a value is assigned to a constant or a
procedure, `?` reads into one, or a procedure name is used in an
expression.

```pl0 error:not a variable
const limit = 10;
limit := 20.
```

**`not a procedure`**: `call` is used with a constant or variable.

```pl0 error:not a procedure
var x;
call x.
```

**`name already declared`**: the same name is declared twice in one
block (parameters count as names of the procedure's block). An inner
block may reuse an outer name.

```pl0 error:name already declared
var x, y, x;
x := 1.
```

**`wrong number of arguments`**: a `call` passes more or fewer values
than the procedure has parameters.

```pl0 error:wrong number of arguments
procedure add(a, b);
  !a + b;
call add(1).
```

### Errors of size

**`too many names`**: more than 64 names are in scope at one point (see
[Limits](#limits)). Reuse variables, or move parts into procedures,
whose names disappear again after their block.

**`program too big`**: the compiled program needs more than 768 P-code
instructions, more than 128 different number literals or more than
1,024 bytes of strings. Shorten the program, or use procedures for
repeated parts.

These two need long programs to trigger; `tests/test_run.c` builds them
(`too_many_idents`, `code_limit`, `consts_limit`, `strings_limit`).

**`nested too deeply`**: procedures nested more than 4 levels deep, or
more than 16 levels of parentheses, signs and function arguments in one
expression, or of `begin`, `if` and `while` inside each other.

```pl0 error:nested too deeply
procedure a;
  procedure b;
    procedure c;
      procedure d;
        procedure e;
          !1;
        ;
      ;
    ;
  ;
;
call a.
```

### Runtime errors

These stop a running program. Output printed before stays on the screen.

**`division by zero`**: `/` with a divisor of 0.

```pl0 error:division by zero
var a, b;
begin a := 1; b := 0; !a / b end.
```

**`domain error`**: `sqrt` of a negative number or `log` of a number
that is not positive.

```pl0 error:domain error
!log(0).
```

**`number overflow`**: a result is too large for a number (above
1.70141183E+38 on the C64), from arithmetic, `exp`, or a turtle move.
`exp(100)` already overflows on the C64; the example below overflows
everywhere.

```pl0 error:number overflow
!exp(1000).
```

**`stack overflow (too deep)`**: the calls are nested too deeply, almost
always a recursion that never stops. See the recursion depth in
[Limits](#limits).

```pl0 error:stack overflow (too deep)
procedure forever;
  call forever;
call forever.
```

**`stopped`**: RUN/STOP was pressed while a loop was running. The line
is the loop's.

```pl0
{ press RUN/STOP to end it: ERROR line 4: stopped }
var i;
begin
  while 1 = 1 do i := i + 1
end.
```

The C64 looks at RUN/STOP every time a `while` loop goes round, so a
program stops within one round. A long recursion without a loop cannot
be stopped until it ends.

**`bad input`**: `?` got something that is not a number, an empty line,
or RUN/STOP.

```pl0
{ type abc and RETURN: ERROR line 2: bad input }
var x;
?x.
```

## Differences from Wirth's PL/0

PL/0 is the small language that Niklaus Wirth used to teach compiler
construction in his book *Algorithms + Data Structures = Programs*
(Prentice-Hall, 1976). The book's last chapter develops a complete PL/0
compiler that translates programs into code for a simple stack machine
(its instructions are LIT, OPR, LOD, STO, CAL, INT, JMP and JPC) and an
interpreter for that code. This program follows the same plan: a
one-pass recursive-descent compiler, the same eight instructions plus a
few new ones, and an interpreter loop (see the
[architecture overview](architecture/overview.md)).

What is the same: `const`, `var`, `procedure`, `call`, `begin ... end`,
`if ... then`, `while ... do`, `odd`, the relational operators with `#`
for "not equal", nested procedures with lexical scope, recursion, no
`else`, no arrays, no types to declare. The `?` and `!` statements for
input and output are those of the versions of PL/0 that are commonly
used in teaching.

What is different:

| Here | In Wirth's PL/0 |
|---|---|
| one type: floating-point numbers (`3.14`, `7 / 2` is 3.5) | whole numbers only |
| procedures with value parameters: `procedure koch(len, depth)`, `call koch(10, 3)` | no parameters; procedures share global variables |
| `!` prints without a new line, takes a string (`!'pi = '`), and a bare `!` starts a new line | (teaching versions) `!` prints a number |
| maths functions `sin cos atan sqrt abs int exp log` | none |
| turtle graphics: `forward turn moveto penup pendown home` | none |
| comments `{ ... }` | none |
| `<>` as well as `#` for "not equal" | `#` |
| a sign can stand before any factor (`2 * -3`) | a sign only at the start of an expression |
| case-insensitive names and keywords | |
| the limits above, chosen for a 64 KB machine, and plain error messages with a line number | |
