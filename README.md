# BadgerScript

A toy language written in C.

Test with:

```text
$ make
$ src/fh tests/test.fh
```

## Features

The code implements a pretty simple and fast register-based VM. Script
execution has 3 phases:

- lexing/parsing converts text to an abstract syntax tree (AST)
- compilation converts the AST to bytecode
- the VM runs the bytecode

There's a simple mark-and-sweep garbage collector, but it currently
doesn't run automatically.  It can be forced by calling the function
`gc()` from inside a script.


## Status

Feature                  | Status
------------------------ | ------------------------------------
Parsing to AST           | Works
Bytecode compilation     | Works
VM (bytecode execution)  | Works
Garbage collection       | Works
Closures                 | TODO


Current plans:

- `map` and `array` values
- closures


## Example Code

This script draws the Mandelbrot set in the terminal:

```javascript

# Calculate the color of the point c=cx + i cy
function calc_point(cx, cy, max_iter)
{
    var i = 0;
    
    # start with z = x + i y = 0
    var x = 0;
    var y = 0;

    while (i < max_iter) {
        # z = z^2 + c
        var t = x*x - y*y + cx;
        y = 2*x*y + cy;
        x = t;

        # stop when |z| > 2
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
                printf(".");
            else
                printf("%d", c%10);
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
