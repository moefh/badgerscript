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

- compilation to bytecode
- pretty fast register-based VM following [Lua](https://www.lua.org/)'s design
- simple mark-and-sweep garbage collector
- full closures
- dynamic typing with `null`, `boolean`, `number`, `string`, `array`,
  `map`, `closure` (function), and `c_func` (C function) values

## Status

Feature                  | Status
------------------------ | ------------------------------------
Parsing to AST           | Working
Bytecode generation      | Working
VM (bytecode execution)  | Working
Garbage collection       | Working
Closures                 | Working
Map and array objects    | Working


## Example Code

### Closures

```php
function make_counter(num) {
    return {
        "next" : function() {
            num = num + 1;
        },

        "read" : function() {
            return num;
        },
    };
}

function main() {
    var c1 = make_counter(0);
    var c2 = make_counter(10);
    c1.next();
    c2.next();
    printf("%d, %d\n", c1.read(), c2.read());    # prints 1, 11

    c1.next();
    if (c1.read() == 2 && c2.read() == 11) {
        printf("ok!\n");
    } else {
        error("this should will not happen");
    }
}
```

### Mandelbrot Set

```php
# check point c = (cx, cy) in the complex plane
function calc_point(cx, cy, max_iter)
{
    # start at the critical point z = (x, y) = 0
    var x = 0;
    var y = 0;

    var i = 0;
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
    var step_x = (x2-x1) / (size_x-1);
    var step_y = (y2-y1) / (size_y-1);

    var y = y1;
    while (y <= y2) {
        var x = x1;
        while (x <= x2) {
            var n = calc_point(x, y, max_iter);
            if (n == max_iter)
                printf(".");         # in Mandelbrot set
            else
                printf("%d", n%10);  # outside
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
