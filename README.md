## BadgerScript

A toy language written in C.

Test with:

```text
$ make
$ src/fh tests/test.fh
```

### Features

The code implements a pretty simple and fast register-based VM. Script execution has 3 phases:

- lexing/parsing converts text to an abstract syntax tree (AST)
- compilation converts the AST to bytecode
- execution runs the bytecode in the VM

There's currently no garbage collection, so the only full first class data type is `number`.
Strings are supported, but they can't be dynamically created or modified.

The language is dynamically-typed with mandatory variable
declarations, `while` loops, `if`/`else` and blocks with lexical
scope.


### Status

Feature                  | Status
------------------------ | ------------------------------------
Parsing to AST           | Works
Bytecode compilation     | Works
VM (bytecode execution)  | Works
Garbage collection       | TODO


### TODO

There's a lot of stuff that's not hard to do but would require some kind
of garbage collection to be useful. Current plans are:

1. mark-and-sweep garbage collection
2. dynamic creation of strings
3. `map` and `vector` values
4. closures


### Example Code

```javascript

function calc_point(cx, cy, max_iter)
{
    var i = 0;
    var x = 0;
    var y = 0;
    while (i < max_iter) {
        var t = x*x - y*y + cx;
        y = 2*x*y + cy;
        x = t;
        if (x*x + y*y > 4)
            break;
        i = i + 1;
    }
    return i;
}

function mandelbrot(x1,y1, x2,y2, size_x,size_y, max_iter)
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
  mandelbrot(-2, -2, 2, 2, 200, 54, 1500);
}
```

### License

MIT License ([LICENSE](https://github.com/ricardo-massaro/badgerscript/blob/master/LICENSE))
