# BadgerScript

A toy language written in C.

Test with:

```text
$ make
$ src/fh tests/mandelbrot.fh
```

Or, with Visual Studio on Windows:

```text
> nmake -f Makefile.win
> fh tests\mandelbrot.fh
```

## Features

The code implements a parser and bytecode compiler for a dynamically
typed toy language with closures, and a pretty fast register-based VM
that follows [Lua](https://www.lua.org/) 5's design.

Script execution has 3 phases:

- lexing/parsing converts text to an abstract syntax tree (AST)
- compilation converts the AST to bytecode
- the VM runs the bytecode

There's a very simple mark-and-sweep garbage collector.


## Status

Feature                  | Status
------------------------ | ------------------------------------
Parsing to AST           | Working
Bytecode compilation     | Working
VM (bytecode execution)  | Working
Garbage collection       | Working
Closures                 | Working


Current plans:

- `map` objects


## Example Code

This script draws the Mandelbrot set in the terminal:

```php

# check point c = (cx, cy) in the complex plane
function calc_point(cx, cy, max_iter)
{
    var i = 0;
    
    # start at the critical point z = (x, y) = 0
    var x = 0;
    var y = 0;

    while (i < max_iter) {
        # calculate next iteration: z = z^2 + c
        var t = x*x - y*y + cx;
        y = 2*x*y + cy;
        x = t;

        # stop if |z| > 2
        if (x*x + y*y > 4)
            break;
        i = i + 1;
    }
    return i;
}

function mandelbrot(x1, y1, x2, y2, size_x, size_y, max_iter)
{
    var step_x = (x2-x1)/size_x;
    var step_y = (y2-y1)/size_y;

    var y = y1;
    while (y <= y2) {
        var x = x1;
        while (x <= x2) {
            var c = calc_point(x, y, max_iter);
            if (c == max_iter)
                printf(".");         # in Mandelbrot set
            else
                printf("%d", c%10);  # outside
            x = x + step_x;
        }
        y = y + step_y;
        printf("\n");
    }
}

function main()
{
  mandelbrot(-2, -2, 2, 2, 150, 50, 1500);
}
```

## License

MIT License ([LICENSE](https://github.com/ricardo-massaro/badgerscript/blob/master/LICENSE))
