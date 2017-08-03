
function calc_point(cx, cy, max_iter) {
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

function mandelbrot(x1,y1, x2,y2, size_x,size_y, max_iter) {

    var step_x = (x2-x1)/size_x;
    var step_y = (y2-y1)/size_y;

    var y = y1;
    while (y <= y2) {
        var x = x1;
        while (x <= x2) {
            var c = calc_point(x, y, max_iter);
            if (c == max_iter)
                printf(" ");
            else
                printf("%d", c%10);
            x = x + step_x;
        }
        y = y + step_y;
        printf("\n");
    }
}
